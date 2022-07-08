//=====- CheckedCDynStats.cpp - Compute the Size of Shared Arrays ----------===//
//
// This pass instruments a program to collect certain dynamic statistics about
// a program. Current it is sued for collecting:
// - size of largest heap object
// - size of largest shared array of pointers between a program and its library.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Instrumentation/CheckedCDynStats.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/IRBuilder.h"

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>

using namespace llvm;

char CheckedCDynStatsPass::ID = 0;

CheckedCDynStatsPass::CheckedCDynStatsPass() : ModulePass(ID){ }

StringRef CheckedCDynStatsPass::getPassName() const {
  return "CheckedCDynStatsArray Pass";
}

#define _RECORD_ALLOC      "_record_alloc"
#define _RECORD_REALLOC    "_record_realloc"
#define _CAL_ARRAY_SIZE    "_cal_array_size"
#define _REMOVE_OBJ_RANGE  "_remove_obj_range"
#define _DUMP_SUMMARY_FN   "_dump_summary"
#define _ATEXIT_FN         "atexit"

// malloc family of functions in Linux.
std::unordered_set<std::string> MallocFns = {
  "calloc", "malloc", "realloc", "reallocarray"
};

static std::unordered_set<std::string> libFuncWL = {
  "strtol", "strtoll", "strtod", "strtold", "strtoul", "strtoull", "strtok_r",
  "strtoimax", "strtoumax",
  "getpwnam_r", "getpwuid_r", "getifaddrs",
  "iconv", "posix_memalign",
  "pthread_join",
  "asprintf", "vasprintf",
};

// Runtime library functions that collects dynamic statistics.
static std::pair<FunctionType*, Function *> recordAllocFn, recordReallocFn,
  dumpSummaryFn, calArraySizeFn, removeObjRangeFn, atexitFn;

//
// Function prepareRuntimeLibFns()
// This function defines the prototypes of the runtime library functions
// that are for collecting the interested dynamic data.
//
static void prepareRuntimeLibFns(Module &M) {
  LLVMContext &C = M.getContext();
  Type *VoidTy = Type::getVoidTy(C);
  Type *VoidPtrTy = Type::getInt8Ty(C)->getPointerTo();
  Type *Int32Ty = Type::getInt32Ty(C);
  Type *Int64Ty = Type::getInt64Ty(C);

  // Define the prototype of _record_alloc()
  FunctionType *FnTy = FunctionType::get(VoidTy, {VoidPtrTy, Int64Ty}, false);
  Function *Fn = cast<Function>(M.getOrInsertFunction(_RECORD_ALLOC, FnTy));
  recordAllocFn = std::pair<FunctionType*, Function*>(FnTy, Fn);

  // Define the prototype of _record_realloc()
  FnTy = FunctionType::get(VoidTy, {VoidPtrTy, VoidPtrTy, Int64Ty}, false);
  Fn = cast<Function>(M.getOrInsertFunction(_RECORD_REALLOC, FnTy));
  recordReallocFn = std::pair<FunctionType*, Function*>(FnTy, Fn);

  // Define the prototype of _cal_array_size()
  FnTy = FunctionType::get(VoidTy, {VoidPtrTy}, false);
  Fn = cast<Function>(M.getOrInsertFunction(_CAL_ARRAY_SIZE, FnTy));
  calArraySizeFn = std::pair<FunctionType*, Function*>(FnTy, Fn);

  // Define the prototype of _remove_obj_range()
  FnTy = FunctionType::get(VoidTy, {VoidPtrTy}, false);
  Fn = cast<Function>(M.getOrInsertFunction(_REMOVE_OBJ_RANGE, FnTy));
  removeObjRangeFn = std::pair<FunctionType*, Function*>(FnTy, Fn);

  // Define the prototype of _dump_summary()
  FnTy = FunctionType::get(VoidTy, false);
  Fn = cast<Function>(M.getOrInsertFunction(_DUMP_SUMMARY_FN, FnTy));
  dumpSummaryFn = std::pair<FunctionType*, Function*>(FnTy, Fn);

  // Define the prototype of atexit()
  FunctionType *VoidFnTy = FunctionType::get(VoidTy, false);
  FnTy = FunctionType::get(Int32Ty, {VoidFnTy->getPointerTo()}, false);
  Fn = cast<Function>(M.getOrInsertFunction(_ATEXIT_FN, FnTy));
  atexitFn = std::pair<FunctionType*, Function*>(FnTy, Fn);
}

//
// Function: handleMalloc()
// This function instruments calls to malloc family of functions. (In Linux,
// they are: malloc, calloc, realloc, and reallocarray.) It inserts a call
// to a runtime library function that records the range of the allocated object.
//
// @param Call - A CallBase instruction to a malloc family function.
//
static void handleMalloc(CallBase *Call) {
  StringRef mallocName = Call->getCalledFunction()->getName();
  IRBuilder<> Builder(Call->getNextNonDebugInstruction());
  Value *size = nullptr;
  if (mallocName == "malloc" || mallocName == "calloc") {
    size = mallocName == "malloc" ? Call->getArgOperand(0) :
      Builder.CreateMul(Call->getArgOperand(0), Call->getArgOperand(1));
    Builder.CreateCall(recordAllocFn.first, recordAllocFn.second, {Call, size});
  } else {
    // Handle calls to realloc and reallocarray
    size = mallocName == "realloc" ? Call->getArgOperand(1) :
      Builder.CreateMul(Call->getArgOperand(1), Call->getArgOperand(2));
    Builder.CreateCall(recordReallocFn.first, recordReallocFn.second,
                       {Call->getArgOperand(0), Call, size});
  }
}

//
// Function: insertCallToStatSummary()
// This function inserts a call to atexit which registers _dump_summary()
// at the beginning of a program.
//
static void insertCallToStatSummary(Module &M) {
  Function *mainFn = M.getFunction("main");
  // If this pass runs on a library, there would be no main function.
  if (!mainFn) return;
  CallInst::Create(atexitFn.first, atexitFn.second, {dumpSummaryFn.second},
                   "", mainFn->front().getFirstNonPHI());
}

//
// Function: instrument()
// This is the main body of this pass. It does the following instrumentation:
// - for eahc malloc family call, insert a call to _record_obj_range() to
//   record the range and size of the allocated heap object.
// - for each call to free(), insert a call to remove the target heap object's
//   range information from the runtime database of all heap objects.
// - for each call to a library function that takes a pointer to pointer argument,
//   insert a call to calculate the size of the array.
//
static bool instrument(Module &M) {
  std::set<Function *> libFuncs;
  std::unordered_map<CallBase*, std::vector<Value*>> callsToLibArrayPtrs;
  std::vector<CallBase*> mallocs, frees;

  // Collect instrumentation sites.
  for (Function &F : M) {
    if (F.isDeclaration()) continue;

    for (BasicBlock &BB : F) {
      for (Instruction &Inst : BB) {
        Instruction *I = &Inst;
        if (CallBase *Call = dyn_cast<CallBase>(I)) {
          if (Function *callee = Call->getCalledFunction()) {
            // We currently skip calls by functions pointers that are not
            // resolved by the compiler.
            if (!callee->isDeclaration() || callee->isIntrinsic()) continue;

            StringRef calleeName = callee->getName();
            // Collect all malloc family functions.
            if (MallocFns.find(calleeName) != MallocFns.end()) {
              mallocs.push_back(Call);
            } else if (calleeName == "free") {
              frees.push_back(Call);
            } else {
              if (libFuncWL.find(calleeName) != libFuncWL.end()) continue;
              // Analayze the arguments to find an array of pointers to pr
              for (unsigned i = 0; i < Call->arg_size(); i++) {
                Value *arg = Call->getArgOperand(i);
                if (arg->getType()->isPointerTy() && cast<PointerType>(
                      arg->getType())->getElementType()->isPointerTy()) {
                  libFuncs.insert(callee);
                  callsToLibArrayPtrs[Call].push_back(arg);
                }
              }
            }
          }
        }
      }
    }
  }

  // Instrument calls to malloc family function.
  for (CallBase *mallocCall : mallocs) {
    handleMalloc(mallocCall);
  }

  // Instrument calls to free()
  for (CallBase *freeCall : frees) {
    Value *ptr = freeCall->getArgOperand(0);
    CallInst::Create(removeObjRangeFn.first, removeObjRangeFn.second, {ptr})->
      insertAfter(freeCall);
  }

  // Instruemtn calls to library functions that take an array of pointers.
  Type *VoidPtrTy = Type::getInt8Ty(M.getContext())->getPointerTo();
  for (auto callPtrArgs : callsToLibArrayPtrs) {
    Instruction *Call = callPtrArgs.first;
    for (Value *Ptr : callPtrArgs.second) {
      Ptr = new BitCastInst(Ptr, VoidPtrTy, "", Call->getNextNonDebugInstruction());
      CallInst::Create(calArraySizeFn.first, calArraySizeFn.second, {Ptr})->
        insertAfter(cast<Instruction>(Ptr));
    }
  }

  // Insert a call to the summary-printing function to the end of main().
  insertCallToStatSummary(M);

  // Write the result to a file.
  std::error_code EC;
  std::string libFnFilePath = "/tmp/lib_fn.stat";
  raw_fd_ostream libFnFile(libFnFilePath, EC, sys::fs::OF_Append);
  if (!libFuncs.empty()) {
    libFnFile << "Lib funtions:\n";
    for (Function* Fn : libFuncs) {
      libFnFile << Fn->getName() << "\n";
    }
  }
  if (size_t callsitesNum = callsToLibArrayPtrs.size())
    libFnFile << "Total call site: " << callsitesNum << "\n";

#if 0
  libFnFile << "Calls:\n";
  for (auto callPtrArgs : callsToLibArrayPtrs) {
    CallBase *Call = callPtrArgs.first;
    libFnFile << Call->getFunction()->getName() << " calls " <<
      Call->getCalledFunction()->getName() << "\n";
  }
#endif

  return !mallocs.empty() || !frees.empty() || !callsToLibArrayPtrs.empty();
}

//
// Entrance of this pass.
//
bool CheckedCDynStatsPass::runOnModule(Module &M) {
  prepareRuntimeLibFns(M);
  return instrument(M);
}

ModulePass *llvm::createCheckedCDynStatsPass() {
  return new CheckedCDynStatsPass();
}
