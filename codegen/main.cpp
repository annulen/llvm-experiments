#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/PassManager.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/CodeGen/LinkAllAsmWriterComponents.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include <llvm/ADT/Triple.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <iostream>
using namespace llvm;

static int compileModule(LLVMContext &context, Module *mod)
{
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmPrinters();

    PassRegistry *Registry = PassRegistry::getPassRegistry();
    initializeCore(*Registry);
    initializeCodeGen(*Registry);

    Triple TheTriple;
    TheTriple.setTriple(sys::getDefaultTargetTriple());
    std::string Error;
    const Target *TheTarget = TargetRegistry::lookupTarget(MArch, TheTriple, Error);
    if (!TheTarget) {
        std::cerr << Error;
        return 1;
    }

    TargetOptions Options;
    OwningPtr<TargetMachine>
        target(TheTarget->createTargetMachine(TheTriple.getTriple(),
                    MCPU, "", Options));
    assert(target.get() && "Could not allocate target machine!");
    assert(mod && "Should have exited after outputting help!");
    TargetMachine &Target = *target.get();

    // Build up all of the passes that we want to do to the module.
    PassManager PM;

    // Add an appropriate TargetLibraryInfo pass for the module's triple.
    TargetLibraryInfo *TLI = new TargetLibraryInfo(TheTriple);
    PM.add(TLI);

    // Add intenal analysis passes from the target machine.
    Target.addAnalysisPasses(PM);

    // Add the target data from the target machine, if it exists, or the module.
    if (const DataLayout *TD = Target.getDataLayout())
        PM.add(new DataLayout(*TD));
    else
        PM.add(new DataLayout(mod));

    raw_os_ostream out(std::cout);
    formatted_raw_ostream FOS(out);

    // Ask the target to add backend passes as necessary.
    if (Target.addPassesToEmitFile(PM, FOS, TargetMachine::CGFT_AssemblyFile)) {
        std::cerr << ": target does not support generation of this"
            << " file type!\n";
        return 1;
    }

    PM.run(*mod);

    return 0;
}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);

    LLVMContext &context = getGlobalContext();
    Module *TheModule = new Module("hello", context);
    IRBuilder<> Builder(context);
    FunctionType *FT = FunctionType::get(Type::getInt32Ty(getGlobalContext()),
                                                   false);
    Function *TheFunction = Function::Create(FT, Function::ExternalLinkage, "main", TheModule);
    BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", TheFunction);
    Builder.SetInsertPoint(BB);

    Constant *msg_0 = ConstantDataArray::getString(context, "Hello world!", true);

    GlobalVariable *hellomsg = new GlobalVariable(
      *TheModule,
      msg_0->getType(),
      true,
      GlobalValue::PrivateLinkage,
      msg_0,
      "hellomsg");

    //declare i32 @puts(i8 *)
    Function *puts_func = cast<Function>(TheModule->
      getOrInsertFunction("puts", IntegerType::getInt32Ty(context),
                      PointerType::getUnqual(IntegerType::getInt8Ty(context)), NULL));

    //call i32 @puts(i8 *getelementptr([%d x i8] *@hellomsg, i32 0, i32 0))
    {
      Constant *zero_32 = Constant::getNullValue(IntegerType::getInt32Ty(context));

      Constant *gep_params[] = {
        zero_32,
        zero_32
      };

      Constant *msgptr = ConstantExpr::
        getGetElementPtr(hellomsg, gep_params);

      Value *puts_params[] = {
        msgptr
      };

      CallInst *puts_call =
        CallInst::Create(puts_func,
                         puts_params,
                         "", BB);
      puts_call->setTailCall(false);
    }

    //Builder.CreateGEP(HelloWorld, gep_params);
    Builder.CreateRet(ConstantInt::get(context, APInt(32, 0, true))); // ret i32 0

    TheModule->dump();
    // Validate the generated code, checking for consistency.
    verifyModule(*TheModule);
    compileModule(context, TheModule);
    return 0;
}
