#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/Internalize.h"
// #include <llvm/llvm/IR/Constants.h>
// #include <llvm/llvm/Support/Error.h>

#include <iostream>

using namespace llvm;

class DebugStream {
private:
  bool enabled;

public:
  DebugStream(bool enabled) : enabled(enabled) {}
  template <typename T>
  friend DebugStream &operator<<(DebugStream &stream, T thing) {
    if (stream.enabled) {
      std::cout << thing << std::flush;
    }
    return stream;
  }
  friend DebugStream &operator<<(DebugStream &stream,
                                 std::ostream &(*pf)(std::ostream &)) {
    if (stream.enabled) {
      std::cout << pf << std::flush;
    }
    return stream;
  }
  friend DebugStream &operator<<(DebugStream &stream,
                                 std::basic_ios<char> &(*pf)(std::ostream &)) {
    if (stream.enabled) {
      std::cout << pf << std::flush;
    }
    return stream;
  }
  friend DebugStream &operator<<(DebugStream &stream,
                                 std::ios_base &(*pf)(std::ostream &)) {
    if (stream.enabled) {
      std::cout << pf << std::flush;
    }
    return stream;
  }
};

namespace {

// This pass makes sure exactly one function is marked as the main
// function (via the __attribute__((annotate("shellvm-main"))) annotation).
// It removes this annotation, replaces it with an LLVM attribute, and marks
// all other functions in the module as private.
struct PreparePass : public ModulePass {
  static char ID;
  PreparePass() : ModulePass(ID) {}

  std::set<Function *> annotFuncs;

  bool mustPreserve(const GlobalValue &value) {
    if (!isa<Function>(value))
      return false;

    const Function &fn = cast<Function>(value);
    return (!fn.isDeclaration() && fn.hasFnAttribute("shellvm-main"));
  }

  bool doInitialization(Module &M) override {
    // Initialize the module with the list of annotated functions
    getAnnotatedFunctions(&M);
    return false;
  }

  void getAnnotatedFunctions(Module *M) {
    for (Module::global_iterator I = M->global_begin(), E = M->global_end();
         I != E; ++I) {

      if (I->getName() == "llvm.global.annotations") {
        ConstantArray *CA = dyn_cast<ConstantArray>(I->getOperand(0));
        for (auto OI = CA->op_begin(); OI != CA->op_end(); ++OI) {
          ConstantStruct *CS = dyn_cast<ConstantStruct>(OI->get());
          CS->dump();
          Function *func = dyn_cast<Function>(CS->getOperand(0));
          func->dump();
          CS->getOperand(1)->dump();
          GlobalVariable *AnnotationGL =
              dyn_cast<GlobalVariable>(CS->getOperand(1));
          StringRef annotation =
              dyn_cast<ConstantDataArray>(AnnotationGL->getInitializer())
                  ->getAsCString();
          if (annotation.compare("shellvm-main") == 0) {
            func->addFnAttr("shellvm-main");
            func->setUnnamedAddr(GlobalValue::UnnamedAddr::Local);
            annotFuncs.insert(func);
          }
        }
      }
    }
  }

  bool runOnModule(Module &M) override {
    if (annotFuncs.size() != 1) {
      report_fatal_error(
          "There should be only one function marked shellvm-main.");
    }

    llvm::internalizeModule(
        M, std::bind(&PreparePass::mustPreserve, this, std::placeholders::_1));
    return true;
  }
}; // end of struct PreparePass
} // end of anonymous namespace

char PreparePass::ID = 0;

static RegisterPass<PreparePass> X("shellvm-prepare", "SheLLVM Prepare Pass",
                                   false /* Only looks at CFG */,
                                   false /* Analysis Pass */);
