//=====- CheckedCGetObjSize.cpp - Compute the Size of Heap Objects --------===//
//
// This pass computes the size of each struct and dynamically allocated heap
// object for the Checked C project.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/CheckedCGetObjSize.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/FileSystem.h"

using namespace llvm;

char CheckedCGetObjSizePass::ID = 0;

CheckedCGetObjSizePass::CheckedCGetObjSizePass() : ModulePass(ID) { }

StringRef CheckedCGetObjSizePass::getPassName() const {
  return "Compute Object Size";
}

//
// Function: findLargestStruct()
//
// This function finds out the largest struct in a Module.
//
static void findLargestStruct(Module &M) {
  DataLayout DL = DataLayout(&M);
  unsigned largestST = 0;
  for (StructType *ST : M.getIdentifiedStructTypes()) {
    if (!ST->isSized()) continue;

    unsigned structSize = DL.getTypeAllocSize(ST);
    largestST = structSize > largestST ? structSize : largestST;
  }

  // FIXME: Write the result to a file.
  std::error_code EC;
  std::string tmpFilePath = "/tmp/struct_size.txt";
  raw_fd_ostream TmpFile(tmpFilePath, EC, sys::fs::OF_Append);
  TmpFile << "Largest Struct: " << largestST << " bytes\n";
  errs() << "Largest Struct: " << largestST << " bytes\n";
}


//
// Entrance of this pass.
//
bool CheckedCGetObjSizePass::runOnModule(Module &M) {
  // Compute the size of struct
  findLargestStruct(M);

  return false;
}

// Create a pass instance.
ModulePass *llvm::createCheckedCGetObjSizePass() {
  return new CheckedCGetObjSizePass();
}
