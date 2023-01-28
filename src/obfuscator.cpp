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
#include <unordered_set>
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

  // 加密字符串策略是这样的：对每个User所在的Function的EntryBlock上Alloc一个与字符串等长
  // 的Buffer，然后插入__decrypt函数，将字符串解密到Buffer中，然后将Function内部所有的
  // 字符串引用替换为Buffer的引用
  void ObfuscateString(Module &M, Function *Func, Value *Usr,
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

    // 获取Function的EntryBlock
    BasicBlock *EntryBlock = &Func->getEntryBlock();
    // 在EntryBlock上插入Alloc指令，分配一个与字符串等长的Buffer数组变量,align=16
    IRBuilder<> Builder(EntryBlock, EntryBlock->getFirstInsertionPt());
    Type *Int8Ty = Builder.getInt8Ty();
    Type *Int8PtrTy = Builder.getInt8PtrTy();
    // align=16
    Value *AllocInst =
        Builder.CreateAlloca(ArrayType::get(Int8Ty, Origin_Str.length()),
                             nullptr, GVar->getName() + "_buffer");
    // 将AllocInst转换为Int8PtrTy类型
    AllocInst = Builder.CreateBitCast(AllocInst, Int8PtrTy,
                                      AllocInst->getName() + "pointer");
    // 将Function内部所有的字符串引用替换为Buffer的引用
    Usr->replaceAllUsesWith(AllocInst);
    // 在EntryBlock上插入__decrypt函数，将字符串解密到Buffer中，调用@llvm.memcpy
    SmallVector<Type *, 1> FuncArgs = {Int8PtrTy};
    SmallVector<Value *, 1> CallArgs = {Usr};
    FunctionType *FuncType = FunctionType::get(Int8PtrTy, FuncArgs, false);
    Value *DecryptFunc =
        M.getOrInsertFunction("__decrypt", FuncType).getCallee();

    CallInst *DecryptCall = Builder.CreateCall(FuncType, DecryptFunc, CallArgs);
    // 在EntryBlock上插入@llvm.memcpy函数，将字符串解密到Buffer中，返回值为void
    FunctionType *MemFuncType = FunctionType::get(Builder.getVoidTy(), false);
    Value *MemcpyFunc =
        M.getOrInsertFunction("llvm.memcpy.p0i8.p0i8.i64", Builder.getVoidTy(),
                              Int8PtrTy, Int8PtrTy, Builder.getInt64Ty(),
                              Builder.getInt1Ty())
            .getCallee();
    SmallVector<Value *, 4> MemcpyArgs = {AllocInst, DecryptCall,
                                          Builder.getInt64(Origin_Str.length()),
                                          Builder.getInt1(0)};
    CallInst *MemcpyCall =
        Builder.CreateCall(MemFuncType, MemcpyFunc, MemcpyArgs);
    // // 获取Function的ExitBlock
    // BasicBlock *ExitBlock = &Func->getBasicBlockList().back();
    // // 在ExitBlock的尾部最后一个指令前插入__encrypt函数，将原字符串加密
    // Builder.SetInsertPoint(ExitBlock, --ExitBlock->end());
    // Value *EncryptFunc =
    //     M.getOrInsertFunction("__encrypt", FuncType).getCallee();
    // CallInst *EncryptCall = Builder.CreateCall(FuncType, EncryptFunc,
    // CallArgs);
  }

  virtual PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    // 遍历所有全局变量，找出使用全局变量的Instruction
    for (auto &GV : M.globals()) {
      GlobalVariable *GVar = dyn_cast<GlobalVariable>(&GV);
      outs() << "GlobalVariable: " << *GVar << "\n";
      if (GVar == nullptr) {
        continue;
      }
      std::set<std::pair<Function *, User *>> Target;
      bool hasExceptCallInst = false;
      // 获取使用GVar的User
      for (User *Usr : GVar->users()) {
        outs() << "User: " << *Usr << "\n";
        Instruction *Inst = dyn_cast<Instruction>(Usr);
        // 如果引用是NULL，说明这条指令并不是单独的指令，被嵌套在了其他指令中
        if (Inst == nullptr) {
          for (User *DirectUsr : Usr->users()) {
            Inst = dyn_cast<Instruction>(DirectUsr);
            outs() << "DirectUsr: " << *Inst << "\n";
            if (Inst == nullptr) {
              continue;
            }
            // 如果不是一个Call指令，那么是不需要修改的，我们只修改函数形参引用的字符串
            if (!isa<CallInst>(Inst)) {
              hasExceptCallInst = true;
              Target.clear();
            } else {
              Target.emplace(
                  std::pair<Function *, User *>(Inst->getFunction(), Usr));
            }
          }
        }
      }
      if (hasExceptCallInst == false) {
        for (auto &T : Target) {
          ObfuscateString(M, T.first, T.second, GVar);
          outs() << "Parameter Instruction: " << *T.first << "\n";
          outs() << "Parameter User: " << *T.second << "\n";
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