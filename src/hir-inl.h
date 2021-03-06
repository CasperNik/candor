/**
 * Copyright (c) 2012, Fedor Indutny.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _SRC_HIR_INL_H_
#define _SRC_HIR_INL_H_

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif  // _STDC_FORMAT_MACROS
#include <inttypes.h>  // PRIu64
#include <assert.h>

#include "hir.h"

namespace candor {
namespace internal {

inline void HIRGen::set_current_block(HIRBlock* b) {
  current_block_ = b;
}


inline void HIRGen::set_current_root(HIRBlock* b) {
  current_root_ = b;
}


inline HIRBlock* HIRGen::current_block() {
  return current_block_;
}


inline HIRBlock* HIRGen::current_root() {
  return current_root_;
}


inline HIRBlockList* HIRGen::blocks() {
  return &blocks_;
}


inline HIRBlockList* HIRGen::roots() {
  return &roots_;
}


inline HIRBlock* HIRGen::CreateBlock(int stack_slots) {
  HIRBlock* b = new HIRBlock(this);
  b->loop_depth = loop_depth_;
  b->root(current_root() == NULL ? b : current_root());
  b->env(new HIREnvironment(stack_slots));

  blocks_.Push(b);

  return b;
}


inline HIRBlock* HIRGen::CreateBlock() {
  // NOTE: -1 for additional logic_slot
  return CreateBlock(current_block()->env()->stack_slots() - 1);
}


inline HIRInstruction* HIRGen::CreateInstruction(HIRInstruction::Type type) {
  return new HIRInstruction(type);
}


inline HIRPhi* HIRGen::CreatePhi(ScopeSlot* slot) {
  return current_block()->CreatePhi(slot);
}


inline HIRInstruction* HIRGen::Add(HIRInstruction::Type type) {
  return current_block()->Add(type);
}


inline HIRInstruction* HIRGen::Add(HIRInstruction::Type type, ScopeSlot* slot) {
  return current_block()->Add(type, slot);
}


inline HIRInstruction* HIRGen::Add(HIRInstruction* instr) {
  return current_block()->Add(instr);
}


inline HIRInstruction* HIRGen::Goto(HIRBlock* target) {
  return current_block()->Goto(target);
}


inline HIRInstruction* HIRGen::Branch(HIRInstruction* instr,
                                      HIRBlock* t,
                                      HIRBlock* f) {
  return current_block()->Branch(instr, t, f);
}


inline HIRInstruction* HIRGen::Return(HIRInstruction* instr) {
  return current_block()->Return(instr);
}


inline void HIRGen::Print(char* out, int32_t size) {
  PrintBuffer p(out, size);
  Print(&p);
}


inline void HIRGen::Print(PrintBuffer* p) {
  HIRBlockList::Item* head = blocks_.head();
  for (; head != NULL; head = head->next()) {
    head->value()->Print(p);
  }
}


inline Root* HIRGen::root() {
  return root_;
}


inline int HIRGen::block_id() {
  return block_id_++;
}


inline int HIRGen::instr_id() {
  int r = instr_id_;

  instr_id_ += 2;

  return r;
}


inline int HIRGen::dfs_id() {
  return dfs_id_++;
}


inline HIRBlock* HIRBlock::AddSuccessor(HIRBlock* b) {
  assert(succ_count_ < 2);
  succ_[succ_count_++] = b;

  b->AddPredecessor(this);

  return b;
}


inline HIRInstruction* HIRBlock::Add(HIRInstruction::Type type) {
  HIRInstruction* instr = new HIRInstruction(type);
  return Add(instr);
}


inline HIRInstruction* HIRBlock::Add(HIRInstruction::Type type,
                                     ScopeSlot* slot) {
  HIRInstruction* instr = new HIRInstruction(type, slot);
  return Add(instr);
}


inline HIRInstruction* HIRBlock::Add(HIRInstruction* instr) {
  instr->ast(g_->current_node());
  instr->Init(g_, this);
  if (!ended_) instructions_.Push(instr);

  return instr;
}


inline HIRInstruction* HIRBlock::Goto(HIRBlock* target) {
  HIRInstruction* res = Add(new HIRGoto());

  if (!ended_) {
    AddSuccessor(target);
    ended_ = true;
  }

  return res;
}


inline HIRInstruction* HIRBlock::Branch(HIRInstruction* instr,
                                        HIRBlock* t,
                                        HIRBlock* f) {
  HIRInstruction* res = Add(instr);

  if (!ended_) {
    AddSuccessor(t);
    AddSuccessor(f);
    ended_ = true;
  }

  return res;
}


inline HIRInstruction* HIRBlock::Return(HIRInstruction* instr) {
  HIRInstruction* res = Add(instr);
  if (!ended_) ended_ = true;
  return res;
}


inline HIRBlock* HIRGen::Join(HIRBlock* b1, HIRBlock* b2) {
  HIRBlock* join = CreateBlock();

  b1->Goto(join);
  b2->Goto(join);

  return join;
}


inline HIRInstruction* HIRGen::Assign(ScopeSlot* slot, HIRInstruction* value) {
  return current_block()->Assign(slot, value);
}


inline HIRInstruction* HIRGen::GetNumber(uint64_t i) {
  AstNode* index = new AstNode(AstNode::kNumber);

  // Fast-case
  if (i < 10) {
    switch (i) {
     case 0: index->value("0"); break;
     case 1: index->value("1"); break;
     case 2: index->value("2"); break;
     case 3: index->value("3"); break;
     case 4: index->value("4"); break;
     case 5: index->value("5"); break;
     case 6: index->value("6"); break;
     case 7: index->value("7"); break;
     case 8: index->value("8"); break;
     case 9: index->value("9"); break;
     default: UNEXPECTED
    }
    index->length(1);

    return Visit(index);
  }

  char keystr[32];
  index->value(keystr);
  index->length(snprintf(keystr, sizeof(keystr), "%" PRIu64, i));

  HIRInstruction* r = Visit(index)->Unpin();
  r->ast(NULL);

  return r;
}


inline bool HIRBlock::IsEnded() {
  return ended_;
}


inline bool HIRBlock::IsEmpty() {
  return instructions_.length() == 0;
}


inline bool HIRBlock::IsLoop() {
  return loop_;
}


inline HIRPhi* HIRBlock::CreatePhi(ScopeSlot* slot) {
  HIRPhi* phi =  new HIRPhi(slot);

  phis_.Push(phi);

  return phi;
}


inline HIRBlock* HIRBlock::root() {
  return root_;
}


inline void HIRBlock::root(HIRBlock* root) {
  root_ = root;
}


inline BitField<EmptyClass>* HIRBlock::reachable_from() {
  return &reachable_from_;
}


inline HIREnvironment* HIRBlock::env() {
  assert(env_ != NULL);
  return env_;
}


inline void HIRBlock::env(HIREnvironment* env) {
  env_ = env;
}


inline HIRInstructionList* HIRBlock::instructions() {
  return &instructions_;
}


inline HIRPhiList* HIRBlock::phis() {
  return &phis_;
}


inline HIRBlock* HIRBlock::SuccAt(int i) {
  assert(i < succ_count_);
  return succ_[i];
}


inline HIRBlock* HIRBlock::PredAt(int i) {
  assert(i < pred_count_);
  return pred_[i];
}


inline int PredAt(int i);


inline int HIRBlock::pred_count() {
  return pred_count_;
}


inline int HIRBlock::succ_count() {
  return succ_count_;
}


inline HIRBlock* HIRBlock::parent() {
  return parent_;
}


inline void HIRBlock::parent(HIRBlock* parent) {
  parent_ = parent;
}


inline HIRBlock* HIRBlock::ancestor() {
  return ancestor_;
}


inline void HIRBlock::ancestor(HIRBlock* ancestor) {
  ancestor_ = ancestor;
}


inline HIRBlock* HIRBlock::label() {
  return label_;
}


inline void HIRBlock::label(HIRBlock* label) {
  label_ = label;
}


inline HIRBlock* HIRBlock::semi() {
  return semi_;
}


inline void HIRBlock::semi(HIRBlock* semi) {
  assert(semi != NULL);
  semi_ = semi;
}


inline HIRBlock* HIRBlock::dominator() {
  return dominator_;
}


inline void HIRBlock::dominator(HIRBlock* dominator) {
  dominator_ = dominator;
}


inline int HIRBlock::dominator_depth() {
  // Cache miss
  if (dominator_depth_ == -1) {
    int r = 0;
    if (dominator() != NULL) {
      r = dominator()->dominator_depth() + 1;
    }
    dominator_depth_ = r;
  }

  // Cache hit
  return dominator_depth_;
}


inline HIRBlockList* HIRBlock::dominates() {
  return &dominates_;
}


inline LBlock* HIRBlock::lir() {
  return lir_;
}


inline void HIRBlock::lir(LBlock* lir) {
  assert(lir_ == NULL);
  lir_ = lir;
}


inline void HIRBlock::Compress() {
  // Nothing to compress
  if (ancestor()->ancestor() == NULL) return;

  ancestor()->Compress();
  if (ancestor()->label()->semi()->dfs_id < label()->semi()->dfs_id) {
    label(ancestor()->label());
  }
  ancestor(ancestor()->ancestor());
}


inline HIRBlock* HIRBlock::Evaluate() {
  if (ancestor() == NULL) return this;

  Compress();
  return label();
}


inline void HIRBlock::Print(PrintBuffer* p) {
  p->Print(IsLoop() ? "# Block %d (loop)\n" : "# Block %d\n", id);
  if (dominator() != NULL) {
    p->Print("# dom: %d\n", dominator()->id);
  }

  HIRInstructionList::Item* head = instructions_.head();
  for (; head != NULL; head = head->next()) {
    head->value()->Print(p);
  }

  switch (succ_count_) {
    case 1:
      p->Print("# succ: %d\n--------\n", succ_[0]->id);
      break;
    case 2:
      p->Print("# succ: %d %d\n--------\n", succ_[0]->id, succ_[1]->id);
      break;
    default:
      break;
  }
}


inline HIRInstruction* HIREnvironment::At(int i) {
  assert(i < stack_slots_);
  return instructions_[i];
}


inline void HIREnvironment::Set(int i, HIRInstruction* value) {
  assert(i < stack_slots_);
  instructions_[i] = value;
}


inline HIRPhi* HIREnvironment::PhiAt(int i) {
  assert(i < stack_slots_);
  return phis_[i];
}


inline void HIREnvironment::SetPhi(int i, HIRPhi* phi) {
  assert(i < stack_slots_);
  phis_[i] = phi;
}


inline HIRInstruction* HIREnvironment::At(ScopeSlot* slot) {
  return At(slot->index());
}


inline void HIREnvironment::Set(ScopeSlot* slot, HIRInstruction* value) {
  Set(slot->index(), value);
}


inline HIRPhi* HIREnvironment::PhiAt(ScopeSlot* slot) {
  return PhiAt(slot->index());
}


inline void HIREnvironment::SetPhi(ScopeSlot* slot, HIRPhi* phi) {
  SetPhi(slot->index(), phi);
}


inline int HIREnvironment::stack_slots() {
  return stack_slots_;
}


inline ScopeSlot* HIREnvironment::logic_slot() {
  return logic_slot_;
}


inline HIRBlockList* BreakContinueInfo::continue_blocks() {
  return &continue_blocks_;
}

}  // namespace internal
}  // namespace candor

#endif  // _SRC_HIR_INL_H_
