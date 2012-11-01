#include "lir.h"
#include "lir-inl.h"
#include "lir-instructions.h"
#include "lir-instructions-inl.h"
#include "macroassembler.h"

namespace candor {
namespace internal {

void LGen::VisitNop(HIRInstruction* instr) {
}


void LGen::VisitNil(HIRInstruction* instr) {
  Bind(new LNil())
      ->SetResult(CreateVirtual(), LUse::kAny);
}


void LGen::VisitEntry(HIRInstruction* instr) {
  Bind(new LEntry(HIREntry::Cast(instr)->context_slots()));
}


void LGen::VisitReturn(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  Bind(new LReturn())
      ->AddArg(lhs, LUse::kRegister);
}


void LGen::VisitLiteral(HIRInstruction* instr) {
  Bind(new LLiteral(HIRLiteral::Cast(instr)->root_slot()))
      ->SetResult(CreateVirtual(), LUse::kAny);
}


void LGen::VisitAllocateObject(HIRInstruction* instr) {
  LInstruction* op = Bind(new LAllocateObject())->MarkHasCall();

  ResultFromFixed(op, eax);
}


void LGen::VisitAllocateArray(HIRInstruction* instr) {
  LInstruction* op = Bind(new LAllocateArray())->MarkHasCall();

  ResultFromFixed(op, eax);
}


void LGen::VisitFunction(HIRInstruction* instr) {
  HIRFunction* fn = HIRFunction::Cast(instr);

  // Generate block and label if needed
  if (fn->body->lir() == NULL) new LBlock(fn->body);

  // XXX : Store address somewhere
  LInstruction* op = Bind(new LFunction(fn->body->lir(), fn->arg_count))
      ->MarkHasCall()
      ->AddScratch(CreateVirtual());

  ResultFromFixed(op, eax);
}


void LGen::VisitNot(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInstruction* op = Bind(new LNot())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitBinOp(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInterval* rhs = ToFixed(instr->right(), ebx);
  LInstruction* op = Bind(new LBinOp())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister)
      ->AddArg(rhs, LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitSizeof(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInstruction* op = Bind(new LSizeof())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitTypeof(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInstruction* op = Bind(new LTypeof())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitKeysof(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInstruction* op = Bind(new LKeysof())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitClone(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInstruction* op = Bind(new LClone())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitLoadContext(HIRInstruction* instr) {
  Bind(new LLoadContext())
      ->SetSlot(HIRLoadContext::Cast(instr)->context_slot())
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitStoreContext(HIRInstruction* instr) {
  Bind(new LStoreContext())
      ->SetSlot(HIRStoreContext::Cast(instr)->context_slot())
      ->AddScratch(CreateVirtual())
      ->AddArg(instr->left(), LUse::kRegister);
}


void LGen::VisitLoadProperty(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInterval* rhs = ToFixed(instr->right(), ebx);
  LInstruction* load = Bind(new LLoadProperty())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister)
      ->AddArg(rhs, LUse::kRegister);

  ResultFromFixed(load, eax);
}


void LGen::VisitStoreProperty(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInterval* rhs = ToFixed(instr->right(), ebx);
  LInterval* third = ToFixed(instr->third(), ecx);
  LInstruction* load = Bind(new LStoreProperty())
      ->MarkHasCall()
      ->AddScratch(CreateVirtual())
      ->AddArg(lhs, LUse::kRegister)
      ->AddArg(rhs, LUse::kRegister)
      ->SetResult(third, LUse::kRegister);
}


void LGen::VisitDeleteProperty(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInterval* rhs = ToFixed(instr->right(), ebx);
  Bind(new LDeleteProperty())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister)
      ->AddArg(rhs, LUse::kRegister);
}


void LGen::VisitGetStackTrace(HIRInstruction* instr) {
  LInstruction* trace = Bind(new LGetStackTrace())
      ->MarkHasCall();

  ResultFromFixed(trace, eax);
}


void LGen::VisitCollectGarbage(HIRInstruction* instr) {
  Bind(new LCollectGarbage())
      ->MarkHasCall();
}


void LGen::VisitLoadArg(HIRInstruction* instr) {
  Bind(new LLoadArg())
      ->AddArg(instr->left(), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kAny);
}


void LGen::VisitLoadVarArg(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInterval* rhs = ToFixed(instr->right(), ebx);
  LInterval* third = ToFixed(instr->third(), ecx);
  Bind(new LLoadVarArg())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister)
      ->AddArg(rhs, LUse::kRegister)
      ->SetResult(third, LUse::kAny);
}


void LGen::VisitStoreArg(HIRInstruction* instr) {
  Bind(new LStoreArg())
      ->AddArg(instr->left(), LUse::kRegister);
}


void LGen::VisitAlignStack(HIRInstruction* instr) {
  Bind(new LAlignStack())
      ->AddScratch(CreateVirtual())
      ->AddArg(instr->left(), LUse::kRegister);
}


void LGen::VisitStoreVarArg(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  Bind(new LStoreVarArg())
      ->MarkHasCall()
      ->AddScratch(CreateVirtual())
      ->AddArg(lhs, LUse::kRegister);
}


void LGen::VisitCall(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), ebx);
  LInterval* rhs = ToFixed(instr->right(), eax);
  LInstruction* op = Bind(new LCall())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister)
      ->AddArg(rhs, LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitIf(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  assert(instr->block()->succ_count() == 2);
  Bind(new LBranch())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister);
}


} // namespace internal
} // namespace candor
