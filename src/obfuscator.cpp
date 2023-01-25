#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Config/llvm-config.h>

#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Support/Casting.h>

#include <cstdio>
#include <map>
#include <utility>
#include <vector>

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

void Encrypt(std::string &s) {
  int length = s.length();
  for (int i = 0; i < length; i++) {
    s[i] ^= 'a';
  }
}

class StrObfuscator : public PassInfoMixin<StrObfuscator> {
public:
  StrObfuscator() {}
  ~StrObfuscator() {}

  void ObfuscateString(Module &M, Instruction *Inst, Value *Usr,
                       GlobalVariable *GVar) {
    ConstantDataArray *GVarArr =
        dyn_cast<ConstantDataArray>(GVar->getInitializer());
    if (GVarArr == nullptr) {
      return;
    }
    std::string Origin_Str;
    // 判断字符串末尾是否包含"/x00"，需要去掉字符串末尾的"/x00"
    if (GVarArr->isString()) {
      Origin_Str = GVarArr->getAsString().str();
    } else if (GVarArr->isCString()) {
      Origin_Str = GVarArr->getAsCString().str();
    }
    outs() << "Origin string: " << Origin_Str << "\n";
    Encrypt(Origin_Str);
    Constant *NewConstStr = ConstantDataArray::getString(
        GVarArr->getContext(), StringRef(Origin_Str), false);
    GVarArr->replaceAllUsesWith(NewConstStr);

    IRBuilder<> builder(Inst);
    Type *Int8PtrTy = builder.getInt8PtrTy();
    SmallVector<Type *, 1> FuncArgs = {Int8PtrTy};
    SmallVector<Value *, 1> CallArgs = {Usr};
    FunctionType *FuncType = FunctionType::get(Int8PtrTy, FuncArgs, false);

    Value *DecryptFunc =
        M.getOrInsertFunction("__decrypt", FuncType).getCallee();
    CallInst *DecryptInst = builder.CreateCall(FuncType, DecryptFunc, CallArgs);

    Value *EncryptFunc =
        M.getOrInsertFunction("__encrypt", FuncType).getCallee();
    CallInst *EncryptInst = CallInst::Create(FuncType, EncryptFunc, CallArgs);

    // DecryptInst->insertBefore(Inst);
    EncryptInst->insertAfter(Inst);
    outs() << "IR:" << *Inst << " has been modified\n";
  }

  virtual PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    // 遍历所有全局变量，找出使用全局变量的Instruction
    for (auto &GV : M.globals()) {
      GlobalVariable *GVar = dyn_cast<GlobalVariable>(&GV);
      outs() << "GlobalVariable: " << *GVar << "\n";
      if (GVar == nullptr) {
        continue;
      }
      std::vector<std::pair<Instruction *, User *>> Target;
      bool hasExceptCallInst = false;
      // 获取使用GVar的User
      for (User *Usr : GVar->users()) {
        outs() << "User: " << *Usr << "\n";
        Instruction *Inst = dyn_cast<Instruction>(Usr);
        // 如果引用是NULL，说明这条指令并不是单独的指令，被嵌套在了其他指令中
        if (Inst == nullptr) {
          for (User *DirectUsr : Usr->users()) {
            Inst = dyn_cast<Instruction>(DirectUsr);
            outs() << "DirectUsr Instruction: " << *Inst << "\n";
            // 如果不是一个Call指令，那么是不需要修改的，我们只修改函数形参引用的字符串
            if (Inst == nullptr) {
              continue;
            }
            if (!isa<CallInst>(Inst)) {
              hasExceptCallInst = true;
              Target.clear();
            } else {
              Target.emplace_back(std::pair<Instruction *, User *>(Inst, Usr));
            }
          }
        }
      }
      // 对每个字符串进行加密
      if (hasExceptCallInst == false && Target.size() == 1) {
        for (auto &T : Target) {
          outs() << "Parameter Instruction: " << *T.first << "\n";
          outs() << "Parameter User: " << *T.second << "\n";
          ObfuscateString(M, T.first, T.second, GVar);
        }
        GVar->setConstant(false);
      }

      outs() << "\n";
    }

    outs() << "Finished\n";

    return PreservedAnalyses::all();
  }
};

} // namespace

// 注册Pass
extern "C" PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {.APIVersion = LLVM_PLUGIN_API_VERSION,
          .PluginName = "StringObfuscator",
          .PluginVersion = LLVM_VERSION_STRING,
          .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) -> bool {
                  if (Name == "obfuscator") {
                    MPM.addPass(StrObfuscator());
                    return true;
                  }
                  return false;
                });
          }};
}