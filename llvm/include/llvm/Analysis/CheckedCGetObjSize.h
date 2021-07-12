//=======- CheckedCGetObjSize.h - Compute the Size of Heap Objects --------===//
//
// This pass computes the size of each struct and dynamically allocated heap
// object for the Checked C project.
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_CHECKEDCGETOBJSIZE_H
#define LLVM_ANALYSIS_CHECKEDCGETOBJSIZE_H

#include "llvm/Pass.h"

namespace llvm {

class CheckedCGetObjSizePass : public ModulePass {

public:
  static char ID;

  CheckedCGetObjSizePass();

  virtual StringRef getPassName() const override;

  virtual bool runOnModule(Module &M) override;

};

ModulePass *createCheckedCGetObjSizePass(void);

}

#endif
