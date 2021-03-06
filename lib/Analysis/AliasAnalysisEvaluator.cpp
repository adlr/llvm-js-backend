//===- AliasAnalysisEvaluator.cpp - Alias Analysis Accuracy Evaluator -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements a simple N^2 alias analysis accuracy evaluator.
// Basically, for each function in the program, it simply queries to see how the
// alias analysis implementation answers alias queries between each pair of
// pointers in the function.
//
// This is inspired and adapted from code by: Naveen Neelakantam, Francesco
// Spadini, and Wojciech Stryjewski.
//
//===----------------------------------------------------------------------===//

#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Assembly/Writer.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SetVector.h"
using namespace llvm;

static cl::opt<bool> PrintAll("print-all-alias-modref-info", cl::ReallyHidden);

static cl::opt<bool> PrintNoAlias("print-no-aliases", cl::ReallyHidden);
static cl::opt<bool> PrintMayAlias("print-may-aliases", cl::ReallyHidden);
static cl::opt<bool> PrintMustAlias("print-must-aliases", cl::ReallyHidden);

static cl::opt<bool> PrintNoModRef("print-no-modref", cl::ReallyHidden);
static cl::opt<bool> PrintMod("print-mod", cl::ReallyHidden);
static cl::opt<bool> PrintRef("print-ref", cl::ReallyHidden);
static cl::opt<bool> PrintModRef("print-modref", cl::ReallyHidden);

namespace {
  /// AAEval - Base class for exhaustive alias analysis evaluators.
  class AAEval {
  protected:
    unsigned NoAlias, MayAlias, MustAlias;
    unsigned NoModRef, Mod, Ref, ModRef;

    SetVector<Value *> Pointers;
    SetVector<CallSite> CallSites;

    void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<AliasAnalysis>();
      AU.setPreservesAll();
    }

    void doInitialization(Module &M) {
      NoAlias = MayAlias = MustAlias = 0;
      NoModRef = Mod = Ref = ModRef = 0;

      if (PrintAll) {
        PrintNoAlias = PrintMayAlias = PrintMustAlias = true;
        PrintNoModRef = PrintMod = PrintRef = PrintModRef = true;
      }
    }

    void runOnFunction(Function &F);
    void evaluate(AliasAnalysis *AA, Module *M);
    void doFinalization(Module &M);
  };

  class FunctionAAEval : public FunctionPass, AAEval {
  public:
    static char ID; // Pass identification, replacement for typeid
    FunctionAAEval() : FunctionPass(&ID) {}

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      return AAEval::getAnalysisUsage(AU);
    }

    virtual bool doInitialization(Module &M) {
      AAEval::doInitialization(M);
      return false;
    }

    virtual bool runOnFunction(Function &F) {
      AAEval::runOnFunction(F);

      if (PrintNoAlias || PrintMayAlias || PrintMustAlias ||
          PrintNoModRef || PrintMod || PrintRef || PrintModRef)
        errs() << "Function: " << F.getName() << ": " << Pointers.size()
               << " pointers, " << CallSites.size() << " call sites\n";

      AAEval::evaluate(&getAnalysis<AliasAnalysis>(), F.getParent());
      return false;
    }

    virtual bool doFinalization(Module &M) {
      AAEval::doFinalization(M);
      return false;
    }
  };

  class InterproceduralAAEval : public ModulePass, AAEval {
  public:
    static char ID; // Pass identification, replacement for typeid
    InterproceduralAAEval() : ModulePass(&ID) {}

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      return AAEval::getAnalysisUsage(AU);
    }

    virtual bool runOnModule(Module &M) {
      AAEval::doInitialization(M);
      for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I)
        AAEval::runOnFunction(*I);

      if (PrintNoAlias || PrintMayAlias || PrintMustAlias ||
          PrintNoModRef || PrintMod || PrintRef || PrintModRef)
        errs() << "Module: " << Pointers.size()
               << " pointers, " << CallSites.size() << " call sites\n";

      AAEval::evaluate(&getAnalysis<AliasAnalysis>(), &M);
      AAEval::doFinalization(M);
      return false;
    }
  };
}

char FunctionAAEval::ID = 0;
static RegisterPass<FunctionAAEval>
X("aa-eval", "Exhaustive Alias Analysis Precision Evaluator", false, true);

FunctionPass *llvm::createAAEvalPass() { return new FunctionAAEval(); }

char InterproceduralAAEval::ID = 0;
static RegisterPass<InterproceduralAAEval>
Y("interprocedural-aa-eval",
  "Exhaustive Interprocedural Alias Analysis Precision Evaluator", false, true);

Pass *llvm::createInterproceduralAAEvalPass() {
  return new InterproceduralAAEval();
}

static void PrintResults(const char *Msg, bool P, const Value *V1,
                         const Value *V2, const Module *M) {
  if (P) {
    std::string o1, o2;
    {
      raw_string_ostream os1(o1), os2(o2);
      WriteAsOperand(os1, V1, true, M);
      WriteAsOperand(os2, V2, true, M);
    }
    
    if (o2 < o1)
      std::swap(o1, o2);
    errs() << "  " << Msg << ":\t"
           << o1 << ", "
           << o2 << "\n";
  }
}

static inline void
PrintModRefResults(const char *Msg, bool P, Instruction *I, Value *Ptr,
                   Module *M) {
  if (P) {
    errs() << "  " << Msg << ":  Ptr: ";
    WriteAsOperand(errs(), Ptr, true, M);
    errs() << "\t<->" << *I << '\n';
  }
}

static inline bool isInterestingPointer(Value *V) {
  return V->getType()->isPointerTy()
      && !isa<ConstantPointerNull>(V);
}

void AAEval::runOnFunction(Function &F) {
  for (Function::arg_iterator I = F.arg_begin(), E = F.arg_end(); I != E; ++I)
    if (I->getType()->isPointerTy())    // Add all pointer arguments.
      Pointers.insert(I);

  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    if (I->getType()->isPointerTy()) // Add all pointer instructions.
      Pointers.insert(&*I);
    Instruction &Inst = *I;
    CallSite CS = CallSite::get(&Inst);
    if (CS) {
      Value *Callee = CS.getCalledValue();
      // Skip actual functions for direct function calls.
      if (!isa<Function>(Callee) && isInterestingPointer(Callee))
        Pointers.insert(Callee);
      // Consider formals.
      for (CallSite::arg_iterator AI = CS.arg_begin(), AE = CS.arg_end();
           AI != AE; ++AI)
        if (isInterestingPointer(*AI))
          Pointers.insert(*AI);
    } else {
      // Consider all operands.
      for (Instruction::op_iterator OI = Inst.op_begin(), OE = Inst.op_end();
           OI != OE; ++OI)
        if (isInterestingPointer(*OI))
          Pointers.insert(*OI);
    }

    if (CS.getInstruction()) CallSites.insert(CS);
  }
}

void AAEval::evaluate(AliasAnalysis *AA, Module *M) {

  // iterate over the worklist, and run the full (n^2)/2 disambiguations
  for (SetVector<Value *>::iterator I1 = Pointers.begin(), E = Pointers.end();
       I1 != E; ++I1) {
    unsigned I1Size = ~0u;
    const Type *I1ElTy = cast<PointerType>((*I1)->getType())->getElementType();
    if (I1ElTy->isSized()) I1Size = AA->getTypeStoreSize(I1ElTy);

    for (SetVector<Value *>::iterator I2 = Pointers.begin(); I2 != I1; ++I2) {
      unsigned I2Size = ~0u;
      const Type *I2ElTy =cast<PointerType>((*I2)->getType())->getElementType();
      if (I2ElTy->isSized()) I2Size = AA->getTypeStoreSize(I2ElTy);

      switch (AA->alias(*I1, I1Size, *I2, I2Size)) {
      case AliasAnalysis::NoAlias:
        PrintResults("NoAlias", PrintNoAlias, *I1, *I2, M);
        ++NoAlias; break;
      case AliasAnalysis::MayAlias:
        PrintResults("MayAlias", PrintMayAlias, *I1, *I2, M);
        ++MayAlias; break;
      case AliasAnalysis::MustAlias:
        PrintResults("MustAlias", PrintMustAlias, *I1, *I2, M);
        ++MustAlias; break;
      default:
        errs() << "Unknown alias query result!\n";
      }
    }
  }

  // Mod/ref alias analysis: compare all pairs of calls and values
  for (SetVector<CallSite>::iterator C = CallSites.begin(),
         Ce = CallSites.end(); C != Ce; ++C) {
    Instruction *I = C->getInstruction();

    for (SetVector<Value *>::iterator V = Pointers.begin(), Ve = Pointers.end();
         V != Ve; ++V) {
      unsigned Size = ~0u;
      const Type *ElTy = cast<PointerType>((*V)->getType())->getElementType();
      if (ElTy->isSized()) Size = AA->getTypeStoreSize(ElTy);

      switch (AA->getModRefInfo(*C, *V, Size)) {
      case AliasAnalysis::NoModRef:
        PrintModRefResults("NoModRef", PrintNoModRef, I, *V, M);
        ++NoModRef; break;
      case AliasAnalysis::Mod:
        PrintModRefResults("     Mod", PrintMod, I, *V, M);
        ++Mod; break;
      case AliasAnalysis::Ref:
        PrintModRefResults("     Ref", PrintRef, I, *V, M);
        ++Ref; break;
      case AliasAnalysis::ModRef:
        PrintModRefResults("  ModRef", PrintModRef, I, *V, M);
        ++ModRef; break;
      default:
        errs() << "Unknown alias query result!\n";
      }
    }
  }

  Pointers.clear();
  CallSites.clear();
}

static void PrintPercent(unsigned Num, unsigned Sum) {
  errs() << "(" << Num*100ULL/Sum << "."
         << ((Num*1000ULL/Sum) % 10) << "%)\n";
}

void AAEval::doFinalization(Module &M) {
  unsigned AliasSum = NoAlias + MayAlias + MustAlias;
  errs() << "===== Alias Analysis Evaluator Report =====\n";
  if (AliasSum == 0) {
    errs() << "  Alias Analysis Evaluator Summary: No pointers!\n";
  } else {
    errs() << "  " << AliasSum << " Total Alias Queries Performed\n";
    errs() << "  " << NoAlias << " no alias responses ";
    PrintPercent(NoAlias, AliasSum);
    errs() << "  " << MayAlias << " may alias responses ";
    PrintPercent(MayAlias, AliasSum);
    errs() << "  " << MustAlias << " must alias responses ";
    PrintPercent(MustAlias, AliasSum);
    errs() << "  Alias Analysis Evaluator Pointer Alias Summary: "
           << NoAlias*100/AliasSum  << "%/" << MayAlias*100/AliasSum << "%/"
           << MustAlias*100/AliasSum << "%\n";
  }

  // Display the summary for mod/ref analysis
  unsigned ModRefSum = NoModRef + Mod + Ref + ModRef;
  if (ModRefSum == 0) {
    errs() << "  Alias Analysis Mod/Ref Evaluator Summary: no mod/ref!\n";
  } else {
    errs() << "  " << ModRefSum << " Total ModRef Queries Performed\n";
    errs() << "  " << NoModRef << " no mod/ref responses ";
    PrintPercent(NoModRef, ModRefSum);
    errs() << "  " << Mod << " mod responses ";
    PrintPercent(Mod, ModRefSum);
    errs() << "  " << Ref << " ref responses ";
    PrintPercent(Ref, ModRefSum);
    errs() << "  " << ModRef << " mod & ref responses ";
    PrintPercent(ModRef, ModRefSum);
    errs() << "  Alias Analysis Evaluator Mod/Ref Summary: "
           << NoModRef*100/ModRefSum  << "%/" << Mod*100/ModRefSum << "%/"
           << Ref*100/ModRefSum << "%/" << ModRef*100/ModRefSum << "%\n";
  }
}
