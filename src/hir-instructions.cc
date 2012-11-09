#include "hir.h"
#include "hir-inl.h"
#include "hir-instructions.h"
#include "hir-instructions-inl.h"

namespace candor {
namespace internal {

HIRInstruction::HIRInstruction(Type type) :
    id(-1),
    gcm_visited(0),
    gvn_visited(0),
    is_live(0),
    type_(type),
    slot_(NULL),
    ast_(NULL),
    lir_(NULL),
    hashed_(false),
    hash_(0),
    removed_(false),
    pinned_(true),
    representation_(kHoleRepresentation) {
}


HIRInstruction::HIRInstruction(Type type, ScopeSlot* slot) :
    id(-1),
    gcm_visited(0),
    gvn_visited(0),
    is_live(0),
    type_(type),
    slot_(slot),
    ast_(NULL),
    lir_(NULL),
    hashed_(false),
    hash_(0),
    removed_(false),
    pinned_(true),
    representation_(kHoleRepresentation) {
}


void HIRInstruction::Init(HIRGen* g, HIRBlock* block) {
  id = g->instr_id();
  block_ = block;
}


bool HIRInstruction::HasSideEffects() {
  return false;
}


bool HIRInstruction::HasGVNSideEffects() {
  return HasSideEffects();
}


inline void HIRInstruction::CalculateRepresentation() {
  representation_ = kUnknownRepresentation;
}


void HIRInstruction::ReplaceArg(HIRInstruction* o, HIRInstruction* n) {
  HIRInstructionList::Item* head = args()->head();
  HIRInstructionList::Item* next;
  for (; head != NULL; head = next) {
    HIRInstruction* arg = head->value();
    next = head->next();

    if (arg == o) {
      args()->InsertBefore(head, n);
      args()->Remove(head);

      o->RemoveUse(this);
      n->uses()->Push(this);

      break;
    }
  }
}


void HIRInstruction::Remove() {
  removed_ = true;

  HIRInstructionList::Item* head = args()->head();
  for (; head != NULL; head = head->next()) {
    head->value()->RemoveUse(this);
  }
}


void HIRInstruction::RemoveUse(HIRInstruction* i) {
  HIRInstructionList::Item* head = uses()->head();
  HIRInstructionList::Item* next;
  for (; head != NULL; head = next) {
    HIRInstruction* use = head->value();
    next = head->next();

    if (use == i) {
      uses()->Remove(head);
      break;
    }
  }
}


uint32_t HIRInstruction::Hash(HIRInstruction* instr) {
  // Loop detected
  if (instr->hashed_) return instr->hash_;
  instr->hashed_ = true;

  // Some outstanding hash - just in case of loops
  instr->hash_ = 0xffff;

  // Jenkins for following sequence
  // [type] [hash of input 1] ... [hash of input N]
  uint32_t r = instr->type() & 0xff;
  r += r << 10;
  r ^= r >> 6;

  HIRInstructionList::Item* ahead = instr->args()->head();
  for (; ahead != NULL; ahead = ahead->next()) {
    uint32_t arg_hash = Hash(ahead->value());

    while (arg_hash != 0) {
      r += arg_hash & 0xff;
      r += r << 10;
      r ^= r >> 6;
      arg_hash = arg_hash >> 8;
    }
  }

  // Shuffle bits
  r += r << 3;
  r ^= r >> 13;
  r += r << 15;

  instr->hash_ = r;
  return r;
}


int HIRInstruction::Compare(HIRInstruction* a, HIRInstruction* b) {
  if (a == b) return 0;

  // Types should be equal
  if (a->type() != b->type()) return -1;

  // Arguments should be equal too
  if (a->args()->length() != b->args()->length()) return -1;

  HIRInstructionList::Item* ahead = a->args()->head();
  HIRInstructionList::Item* bhead = b->args()->head();
  for (; ahead != NULL; ahead = ahead->next(), bhead = bhead->next()) {
    if (ahead->value() != bhead->value()) return -1;
  }

  return a->IsGVNEqual(b) ? 0 : 1;
}


bool HIRInstruction::IsGVNEqual(HIRInstruction* to) {
  // Default implementation
  return true;
}


void HIRInstruction::Print(PrintBuffer* p) {
  p->Print("i%d = ", id);

  p->Print("%s", TypeToStr(type_));

  if (type() == HIRInstruction::kLiteral &&
      ast() != NULL && ast()->value() != NULL) {
    p->Print("[");
    p->PrintValue(ast()->value(), ast()->length());
    p->Print("]");
  }

  if (args()->length() == 0) {
    p->Print("\n");
    return;
  }

  HIRInstructionList::Item* head = args()->head();
  p->Print("(");
  for (; head != NULL; head = head->next()) {
    p->Print("i%d", head->value()->id);
    if (head->next() != NULL) p->Print(", ");
  }
  p->Print(")\n");
}


HIRPhi::HIRPhi(ScopeSlot* slot) : HIRInstruction(kPhi, slot),
                                  input_count_(0) {
  inputs_[0] = NULL;
  inputs_[1] = NULL;
}


void HIRPhi::Init(HIRGen* g, HIRBlock* b) {
  b->env()->Set(slot(), this);
  b->env()->SetPhi(slot(), this);

  HIRInstruction::Init(g, b);
}


void HIRPhi::CalculateRepresentation() {
  int result = kAnyRepresentation;

  for (int i = 0; i < input_count_; i++) {
    result = result & inputs_[i]->representation();
  }

  representation_ = static_cast<Representation>(result);
}


void HIRPhi::ReplaceArg(HIRInstruction* o, HIRInstruction* n) {
  HIRInstruction::ReplaceArg(o, n);

  for (int i = 0; i < input_count(); i++) {
    if (inputs_[i] == o) inputs_[i] = n;
  }
}


HIRLiteral::HIRLiteral(AstNode::Type type, ScopeSlot* slot) :
    HIRInstruction(kLiteral),
    type_(type),
    root_slot_(slot) {
}


bool HIRLiteral::IsGVNEqual(HIRInstruction* to) {
  if (!to->Is(HIRInstruction::kLiteral)) return false;
  HIRLiteral* lto = HIRLiteral::Cast(to);

  return this->root_slot()->is_equal(lto->root_slot());
}


void HIRLiteral::CalculateRepresentation() {
  switch (type_) {
   case AstNode::kNumber:
    representation_ = root_slot_->is_immediate() ?
        kSmiRepresentation : kHeapNumberRepresentation;
    break;
   case AstNode::kString:
   case AstNode::kProperty:
    representation_ = kStringRepresentation;
    break;
   case AstNode::kTrue:
   case AstNode::kFalse:
    representation_ = kBooleanRepresentation;
    break;
   default:
    representation_ = kUnknownRepresentation;
  }
}


HIRFunction::HIRFunction(AstNode* ast) : HIRInstruction(kFunction),
                                         body(NULL),
                                         arg_count(0) {
  ast_ = ast;
}


void HIRFunction::CalculateRepresentation() {
  representation_ = kFunctionRepresentation;
}


void HIRFunction::Print(PrintBuffer* p) {
  p->Print("i%d = Function[b%d]\n", id, body->id);
}


bool HIRFunction::IsGVNEqual(HIRInstruction* to) {
  return this == to;
}


HIRNil::HIRNil() : HIRInstruction(kNil) {
}


HIREntry::HIREntry(int context_slots_) : HIRInstruction(kEntry),
                                         context_slots_(context_slots_) {
}


bool HIREntry::HasSideEffects() {
  return true;
}


void HIREntry::Print(PrintBuffer* p) {
  p->Print("i%d = Entry[%d]\n", id, context_slots_);
}


HIRReturn::HIRReturn() : HIRInstruction(kReturn) {
}


bool HIRReturn::HasSideEffects() {
  return true;
}


HIRIf::HIRIf() : HIRInstruction(kIf) {
}


bool HIRIf::HasSideEffects() {
  return true;
}


HIRGoto::HIRGoto() : HIRInstruction(kGoto) {
}


bool HIRGoto::HasSideEffects() {
  return true;
}


HIRCollectGarbage::HIRCollectGarbage() : HIRInstruction(kCollectGarbage) {
}


bool HIRCollectGarbage::HasSideEffects() {
  return true;
}


HIRGetStackTrace::HIRGetStackTrace() : HIRInstruction(kGetStackTrace) {
}


bool HIRGetStackTrace::HasSideEffects() {
  return true;
}


HIRBinOp::HIRBinOp(BinOp::BinOpType type) : HIRInstruction(kBinOp),
                                            binop_type_(type) {
}


void HIRBinOp::CalculateRepresentation() {
  int left = args()->head()->value()->representation();
  int right = args()->tail()->value()->representation();
  int res;

  if (BinOp::is_binary(binop_type_)) {
    res = kSmiRepresentation;
  } else if (BinOp::is_logic(binop_type_)) {
    res = kBooleanRepresentation;
  } else if (BinOp::is_math(binop_type_)) {
    if (binop_type_ != BinOp::kAdd) {
      res = kNumberRepresentation;
    } else if ((left | right) & kStringRepresentation) {
      // "123" + any, or any + "123"
      res = kStringRepresentation;
    } else {
      int mask = kSmiRepresentation |
                 kHeapNumberRepresentation |
                 kNilRepresentation;
      res = left & right & mask;
    }
  } else {
    res = kUnknownRepresentation;
  }

  representation_ = static_cast<Representation>(res);
}


bool HIRBinOp::IsGVNEqual(HIRInstruction* to) {
  return binop_type_ == HIRBinOp::Cast(to)->binop_type_;
}


HIRLoadContext::HIRLoadContext(ScopeSlot* slot)
    : HIRInstruction(kLoadContext),
      context_slot_(slot) {
}


bool HIRLoadContext::HasGVNSideEffects() {
  return true;
}


HIRStoreContext::HIRStoreContext(ScopeSlot* slot) :
    HIRInstruction(kStoreContext),
    context_slot_(slot) {
}


bool HIRStoreContext::HasSideEffects() {
  return true;
}


void HIRStoreContext::CalculateRepresentation() {
  // Basically store property returns it's first argument
  assert(args()->length() == 1);
  representation_ = args()->tail()->value()->representation();
}


HIRLoadProperty::HIRLoadProperty() : HIRInstruction(kLoadProperty) {
}


HIRStoreProperty::HIRStoreProperty() : HIRInstruction(kStoreProperty) {
}


bool HIRStoreProperty::HasSideEffects() {
  return true;
}


void HIRStoreProperty::CalculateRepresentation() {
  // Basically store property returns it's first argument
  assert(args()->length() == 3);
  representation_ = args()->head()->value()->representation();
}


HIRDeleteProperty::HIRDeleteProperty() : HIRInstruction(kDeleteProperty) {
}


HIRAllocateObject::HIRAllocateObject(int size)
    : HIRInstruction(kAllocateObject),
      size_(RoundUp(PowerOfTwo(size + 1), 64)) {
}


bool HIRAllocateObject::HasGVNSideEffects() {
  return true;
}


void HIRAllocateObject::CalculateRepresentation() {
  representation_ = kObjectRepresentation;
}


HIRAllocateArray::HIRAllocateArray(int size)
    : HIRInstruction(kAllocateArray),
      size_(RoundUp(PowerOfTwo(size + 1), 16)) {
}


bool HIRAllocateArray::HasGVNSideEffects() {
  return true;
}


void HIRAllocateArray::CalculateRepresentation() {
  representation_ = kArrayRepresentation;
}


HIRLoadArg::HIRLoadArg() : HIRInstruction(kLoadArg) {
}


HIRLoadVarArg::HIRLoadVarArg() : HIRInstruction(kLoadVarArg) {
}


bool HIRLoadVarArg::HasSideEffects() {
  return true;
}


void HIRLoadVarArg::CalculateRepresentation() {
  representation_ = kArrayRepresentation;
}


HIRStoreArg::HIRStoreArg() : HIRInstruction(kStoreArg) {
}


bool HIRStoreArg::HasSideEffects() {
  return true;
}


HIRStoreVarArg::HIRStoreVarArg() : HIRInstruction(kStoreVarArg) {
}


bool HIRStoreVarArg::HasSideEffects() {
  return true;
}


HIRAlignStack::HIRAlignStack() : HIRInstruction(kAlignStack) {
}


bool HIRAlignStack::HasSideEffects() {
  return true;
}


HIRCall::HIRCall() : HIRInstruction(kCall) {
}


bool HIRCall::HasSideEffects() {
  return true;
}


HIRKeysof::HIRKeysof() : HIRInstruction(kKeysof) {
}


void HIRKeysof::CalculateRepresentation() {
  representation_ = kArrayRepresentation;
}


HIRSizeof::HIRSizeof() : HIRInstruction(kSizeof) {
}


void HIRSizeof::CalculateRepresentation() {
  representation_ = kSmiRepresentation;
}


HIRTypeof::HIRTypeof() : HIRInstruction(kTypeof) {
}


void HIRTypeof::CalculateRepresentation() {
  representation_ = kStringRepresentation;
}


HIRClone::HIRClone() : HIRInstruction(kClone) {
}


bool HIRClone::HasGVNSideEffects() {
  return true;
}


void HIRClone::CalculateRepresentation() {
  representation_ = kObjectRepresentation;
}

} // namespace internal
} // namespace candor
