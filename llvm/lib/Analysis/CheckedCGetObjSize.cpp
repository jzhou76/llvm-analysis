//=====- CheckedCGetObjSize.cpp - Compute the Size of Heap Objects --------===//
//
// This pass computes the size of each struct and dynamically allocated heap
// object for the Checked C project.
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/CheckedCGetObjSize.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

char CheckedCGetObjSizePass::ID = 0;

CheckedCGetObjSizePass::CheckedCGetObjSizePass() : FunctionPass(ID) { }

StringRef CheckedCGetObjSizePass::getPassName() const {
  return "Compute Object Size";
}

//
// Entrance of this pass.
//
bool CheckedCGetObjSizePass::runOnFunction(Function &F) {
  //TODO

  return false;
}

// Create a pass instance.
FunctionPass *llvm::createCheckedCGetObjSizePass() {
  return new CheckedCGetObjSizePass();
}
