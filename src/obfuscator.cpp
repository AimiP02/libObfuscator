#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/SHA1.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <vector>

#include "CryptoUtils.h"

using namespace llvm;

struct EncryptedGV {
  GlobalVariable *GV;
  uint64_t key;
  uint32_t len;
};

namespace {

static cl::opt<int>
    ObfuTimes("gvobfus-times", cl::init(1),
              cl::desc("Run GlobalsEncryption pass <gvobfus-times> time(s)"));

static cl::opt<bool> OnlyStr("onlystr", cl::init(false),
                             cl::desc("Encrypt string variable only"));

class GVObfuscator : public PassInfoMixin<GVObfuscator> {
public:
  GVObfuscator() {}
  ~GVObfuscator() {}

  LLVMContext *ctx;

  virtual void InsertIntDecryption(Module &M, EncryptedGV encGV);
  virtual void InsertArrayDecryption(Module &M, EncryptedGV encGV);

  virtual PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

std::string GenHashedName(GlobalVariable *GV) {
  Module &M = *GV->getParent();
  std::string funcName =
      formatv("{0}_{1:x-}", M.getName(), M.getMDKindID(GV->getName()));
  SHA1 sha1;
  sha1.update(funcName);
  StringRef digest = sha1.final();

  std::stringstream ss;
  ss << std::hex;

  for (size_t i = 0; i < digest.size(); i++) {
    ss << std::setw(2) << std::setfill('0') << (unsigned)(digest[i] & 0xFF);
  }

  return ss.str();
}

void GVObfuscator::InsertIntDecryption(Module &M, EncryptedGV encGV) {
  std::vector<Type *> funcArgs;
  FunctionType *funcType =
      FunctionType::get(Type::getVoidTy(M.getContext()), funcArgs, false);
  std::string funcName = GenHashedName(encGV.GV);
  FunctionCallee callee = M.getOrInsertFunction(funcName, funcType);
  Function *func = cast<Function>(callee.getCallee());

  BasicBlock *entry = BasicBlock::Create(*ctx, "entry", func);
  IRBuilder<> builder(*ctx);
  builder.SetInsertPoint(entry);
  LoadInst *val = builder.CreateLoad(encGV.GV);
  Value *xorVal = builder.CreateXor(
      val, ConstantInt::get(encGV.GV->getValueType(), encGV.key));
  builder.CreateStore(xorVal, encGV.GV);
  builder.CreateRetVoid();
  appendToGlobalCtors(M, func, 0);
}

void GVObfuscator::InsertArrayDecryption(Module &M, EncryptedGV encGV) {
  std::vector<Type *> funcArgs;
  FunctionType *funcType =
      FunctionType::get(Type::getVoidTy(M.getContext()), funcArgs, false);
  std::string funcName = GenHashedName(encGV.GV);
  FunctionCallee callee = M.getOrInsertFunction(funcName, funcType);
  Function *func = cast<Function>(callee.getCallee());

  BasicBlock *entry = BasicBlock::Create(*ctx, "entry", func);
  BasicBlock *forCond = BasicBlock::Create(*ctx, "for.cond", func);
  BasicBlock *forBody = BasicBlock::Create(*ctx, "for.body", func);
  BasicBlock *forInc = BasicBlock::Create(*ctx, "for.inc", func);
  BasicBlock *forEnd = BasicBlock::Create(*ctx, "for.inc", func);

  IRBuilder<> builder(*ctx);
  Type *Int32Ty = builder.getInt32Ty();
  builder.SetInsertPoint(entry);
  AllocaInst *indexPtr =
      builder.CreateAlloca(Int32Ty, ConstantInt::get(Int32Ty, 1, false), "i");
  builder.CreateStore(ConstantInt::get(Int32Ty, 0), indexPtr);
  builder.CreateBr(forCond);
  builder.SetInsertPoint(forCond);
  LoadInst *index = builder.CreateLoad(Int32Ty, indexPtr);
  ICmpInst *cond = cast<ICmpInst>(
      builder.CreateICmpSLT(index, ConstantInt::get(Int32Ty, encGV.len)));
  builder.CreateCondBr(cond, forBody, forEnd);
  builder.SetInsertPoint(forBody);
  Value *indexList[2] = {ConstantInt::get(Int32Ty, 0), index};
  Value *ele = builder.CreateGEP(encGV.GV, ArrayRef<Value *>(indexList, 2));
  ArrayType *arrTy = cast<ArrayType>(encGV.GV->getValueType());
  Type *eleTy = arrTy->getElementType();
  Value *encEle = builder.CreateXor(builder.CreateLoad(ele),
                                    ConstantInt::get(eleTy, encGV.key));
  builder.CreateStore(encEle, ele);
  builder.CreateBr(forInc);
  builder.SetInsertPoint(forInc);
  builder.CreateStore(builder.CreateAdd(index, ConstantInt::get(Int32Ty, 1)),
                      indexPtr);
  builder.CreateBr(forCond);

  builder.SetInsertPoint(forEnd);
  builder.CreateRetVoid();
  appendToGlobalCtors(M, func, 0);
}

PreservedAnalyses GVObfuscator::run(Module &M, ModuleAnalysisManager &MAM) {
  outs() << "Pass start...\n";

  ctx = &M.getContext();
  std::vector<GlobalVariable *> GVs;

  for (auto &GV : M.globals()) {
    GVs.push_back(&GV);
  }

  for (int i = 0; i < ObfuTimes; i++) {
    outs() << "Current ObfuTimes: " << i << "\n";
    for (auto *GV : GVs) {
      // 只对Integer和Array类型进行加密
      if (!GV->getValueType()->isIntegerTy() &&
          !GV->getValueType()->isArrayTy()) {
        continue;
      }
      // 筛出".str"全局变量，LLVM IR的metadata同样也要保留
      if (GV->hasInitializer() && GV->getInitializer() &&
          (GV->getName().contains(".str") || !OnlyStr) &&
          !GV->getName().contains("llvm.metadata")) {
        Constant *initializer = GV->getInitializer();
        ConstantInt *intData = dyn_cast<ConstantInt>(initializer);
        ConstantDataArray *arrayData = dyn_cast<ConstantDataArray>(initializer);
        // 处理数组
        if (arrayData) {
          // 获取数组的长度和数组元素的大小
          outs() << "Get global arraydata\n";
          uint32_t eleSize = arrayData->getElementByteSize();
          uint32_t eleNum = arrayData->getNumElements();
          uint32_t arrLen = eleNum * eleSize;
          outs() << "Global Variable: " << *GV << "\n"
                 << "Array Length: " << eleSize << " * " << eleNum << " = "
                 << arrLen << "\n";
          char *data = const_cast<char *>(arrayData->getRawDataValues().data());
          char *dataCopy = new char[arrLen];
          memcpy(dataCopy, data, arrLen);
          // 生成密钥
          uint64_t key = cryptoutils->get_uint64_t();
          for (uint32_t i = 0; i < arrLen; i++) {
            dataCopy[i] ^= ((char *)&key)[i % eleSize];
          }
          GV->setInitializer(
              ConstantDataArray::getRaw(StringRef(dataCopy, arrLen), eleNum,
                                        arrayData->getElementType()));
          GV->setConstant(false);
          InsertArrayDecryption(M, {GV, key, eleNum});
        }
        // 处理整数
        else if (intData) {
          uint64_t key = cryptoutils->get_uint64_t();
          ConstantInt *enc = ConstantInt::get(intData->getType(),
                                              key ^ intData->getZExtValue());
          GV->setInitializer(enc);
          InsertIntDecryption(M, {GV, key, 1LL});
        }
      }
    }
  }

  outs() << "Pass end...\n";

  return PreservedAnalyses::all();
}

} // namespace

// 注册Pass
extern "C" PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {.APIVersion = LLVM_PLUGIN_API_VERSION,
          .PluginName = "GVObfuscator",
          .PluginVersion = LLVM_VERSION_STRING,
          .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) -> bool {
                  if (Name == "gvobfus") {
                    MPM.addPass(GVObfuscator());
                    return true;
                  }
                  return false;
                });
          }};
}