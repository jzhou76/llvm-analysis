//=====--- CheckedCDynStats.h - Compute the Size of Shared Arrays ----------===//
//
// This pass instruments a program to compute the sizes of shared arrays of
// pointers between the program and its library.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_CHECKEDCMARSHAL_H
#define LLVM_ANALYSIS_CHECKEDCMARSHAL_H

#include "llvm/Pass.h"

namespace llvm {

class CheckedCDynStatsPass : public ModulePass {
public:
  static char ID;

  CheckedCDynStatsPass();

  virtual StringRef getPassName() const override;

  virtual bool runOnModule(Module &M) override;
};

ModulePass *createCheckedCDynStatsPass(void);

}

#endif

