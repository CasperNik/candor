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

#include <stdio.h>  // fprintf
#include <stdint.h>  // uint32_t
#include <string.h>  // strlen
#include <stdlib.h>  // NULL

#include "candor.h"
#include "isolate.h"
#include "heap.h"
#include "heap-inl.h"
#include "code-space.h"
#include "fullgen.h"
#include "fullgen-inl.h"
#include "hir.h"
#include "hir-inl.h"
#include "lir.h"
#include "lir-inl.h"
#include "runtime.h"
#include "utils.h"

namespace candor {

using namespace internal;

// Declarations
#define TYPES_LIST(V)\
    V(Value)\
    V(Nil)\
    V(Number)\
    V(Boolean)\
    V(String)\
    V(Function)\
    V(Object)\
    V(Array)\
    V(CData)

#define METHODS_ENUM(V)\
    template V* Value::As<V>();\
    template V* Value::Cast<V>(char* addr);\
    template V* Value::Cast<V>(Value* value);\
    template bool Value::Is<V>();\
    template Handle<V>::Handle();\
    template Handle<V>::Handle(Value* v);\
    template Handle<V>::~Handle();\
    template void Handle<V>::Ref();\
    template void Handle<V>::Unref();\
    template void Handle<V>::Wrap(Value* v);\
    template void Handle<V>::Unwrap();\
    template bool Handle<V>::IsEmpty();
TYPES_LIST(METHODS_ENUM)
#undef METHODS_ENUM

#undef TYPES_LIST

#define ISOLATE Isolate::GetCurrent()

static Isolate* current_isolate = NULL;

Isolate::Isolate() {
  IsolateData::GetCurrent()->isolate = this;

  heap = new Heap(2 * 1024 * 1024);
  space = new CodeSpace(heap);
  error = NULL;

  current_isolate = this;
}


Isolate::~Isolate() {
  delete heap;
  delete space;
  delete IsolateData::GetCurrent();
}


Isolate* Isolate::GetCurrent() {
  return IsolateData::GetCurrent()->isolate;
}


bool Isolate::HasError() {
  return error != NULL;
}


Error* Isolate::GetError() {
  return error;
}


void Isolate::PrintError() {
  if (!HasError()) return;

  fprintf(stderr,
          "Error on line %s#%d: %s\n",
          error->filename,
          error->line,
          error->message);
}


void Isolate::SetError(Error* err) {
  if (HasError()) {
    delete error;
  }
  error = err;
}


Array* Isolate::StackTrace() {
  char** frame = reinterpret_cast<char**>(*heap->last_frame());
  return Value::Cast<Array>(RuntimeStackTrace(heap, frame, NULL));
}


void Isolate::EnableFullgenLogging() {
  Fullgen::EnableLogging();
}


void Isolate::DisableFullgenLogging() {
  Fullgen::DisableLogging();
}


void Isolate::EnableHIRLogging() {
  HIRGen::EnableLogging();
}


void Isolate::DisableHIRLogging() {
  HIRGen::DisableLogging();
}


void Isolate::EnableLIRLogging() {
  LGen::EnableLogging();
}


void Isolate::DisableLIRLogging() {
  LGen::DisableLogging();
}


template <class T>
Handle<T>::Handle() : value(NULL), ref_count(0), ref(NULL) {
  Ref();
}


template <class T>
Handle<T>::Handle(Value* v) : value(NULL), ref_count(0) {
  Wrap(v);
  Ref();
}


template <class T>
Handle<T>::~Handle() {
  if (ISOLATE->heap != NULL) {
    Unwrap();
  }
}


template <class T>
void Handle<T>::Ref() {
  if (++ref_count == 1 && ref != NULL) {
    ref->make_persistent();
  }
}


template <class T>
void Handle<T>::Unref() {
  if (--ref_count == 0 && ref != NULL) {
    ref->make_weak();
  }
}


template <class T>
void Handle<T>::Wrap(Value* v) {
  Unwrap();

  value = v->As<T>();
  ref = ISOLATE->heap->Reference(ref_count > 0 ?
                                     Heap::kRefPersistent
                                     :
                                     Heap::kRefWeak,
                                 reinterpret_cast<HValue**>(&value),
                                 reinterpret_cast<HValue*>(value));
}


template <class T>
void Handle<T>::Unwrap() {
  if (value == NULL) return;
  ISOLATE->heap->Dereference(reinterpret_cast<HValue**>(&value),
                             reinterpret_cast<HValue*>(value));
  value = NULL;
  ref = NULL;
}


template <class T>
bool Handle<T>::IsEmpty() {
  return value == NULL;
}


Value* Value::New(char* addr) {
  return reinterpret_cast<Value*>(addr);
}


template <class T>
T* Value::As() {
  assert(Is<T>());
  return reinterpret_cast<T*>(this);
}


template <class T>
T* Value::Cast(char* addr) {
  return reinterpret_cast<T*>(addr);
}


template <class T>
T* Value::Cast(Value* value) {
  assert(value->Is<T>());
  return reinterpret_cast<T*>(value);
}


template <class T>
bool Value::Is() {
  Heap::HeapTag tag = Heap::kTagNil;

  switch (T::tag) {
    case kNil: tag = Heap::kTagNil; break;
    case kNumber: tag = Heap::kTagNumber; break;
    case kBoolean: tag = Heap::kTagBoolean; break;
    case kString: tag = Heap::kTagString; break;
    case kFunction: tag = Heap::kTagFunction; break;
    case kObject: tag = Heap::kTagObject; break;
    case kArray: tag = Heap::kTagArray; break;
    case kCData: tag = Heap::kTagCData; break;
    default: return false;
  }

  if (addr() != NULL && addr() != HNil::New()) {
    assert(!HValue::Cast(addr())->IsSoftGCMarked() &&
           !HValue::Cast(addr())->IsGCMarked());
  }

  return HValue::GetTag(addr()) == tag;
}


Value::ValueType Value::Type() {
  switch (HValue::GetTag(addr())) {
    case Heap::kTagNil: return kNil;
    case Heap::kTagNumber: return kNumber;
    case Heap::kTagBoolean: return kBoolean;
    case Heap::kTagString: return kString;
    case Heap::kTagFunction: return kFunction;
    case Heap::kTagObject: return kObject;
    case Heap::kTagArray: return kArray;
    case Heap::kTagCData: return kCData;
    default: return kNone;
  }
}


Number* Value::ToNumber() {
  return Cast<Number>(RuntimeToNumber(ISOLATE->heap, addr()));
}


Boolean* Value::ToBoolean() {
  return Cast<Boolean>(RuntimeToBoolean(ISOLATE->heap, addr()));
}


String* Value::ToString() {
  return Cast<String>(RuntimeToString(ISOLATE->heap, addr()));
}


void Value::SetWeakCallback(WeakCallback callback) {
  ISOLATE->heap->AddWeak(reinterpret_cast<HValue*>(addr()),
                         *reinterpret_cast<Heap::WeakCallback*>(&callback));
}


void Value::ClearWeak() {
  ISOLATE->heap->RemoveWeak(reinterpret_cast<HValue*>(addr()));
}



Function* Function::New(const char* filename,
                        const char* source,
                        uint32_t length) {
  char* root;
  Error* error;
  char* code = ISOLATE->space->Compile(filename,
                                       source,
                                       length,
                                       &root,
                                       &error);
  // Set errors
  if (code == NULL) {
    ISOLATE->SetError(error);
    return NULL;
  } else {
    ISOLATE->SetError(NULL);
  }

  char* obj = HFunction::New(ISOLATE->heap, NULL, code, root);

  Function* fn = Cast<Function>(obj);

  return fn;
}


Function* Function::New(const char* filename, const char* source) {
  return New(filename, source, strlen(source));
}


Function* Function::New(const char* source) {
  return New(NULL, source);
}


Function* Function::New(BindingCallback callback) {
  char* obj = HFunction::NewBinding(ISOLATE->heap,
                                    *reinterpret_cast<char**>(&callback),
                                    NULL);

  Function* fn = Cast<Function>(obj);

  return fn;
}


Object* Function::GetContext() {
  return Cast<Object>(HFunction::GetContext(addr()));
}


void Function::SetContext(Object* context) {
  return HFunction::SetContext(addr(), context->addr());
}


uint32_t Function::Argc() {
  return HNumber::Untag(HFunction::Argc(addr()));
}


Value* Function::Call(uint32_t argc, Value* argv[]) {
  return ISOLATE->space->Run(addr(), argc, argv);
}


Nil* Nil::New() {
  return Cast<Nil>(HNil::New());
}


Boolean* Boolean::New(bool value) {
  return Cast<Boolean>(ISOLATE->heap->CreateBoolean(value));
}


Boolean* Boolean::True() {
  return New(true);
}


Boolean* Boolean::False() {
  return New(false);
}


bool Boolean::IsTrue() {
  return HBoolean::Value(addr());
}


bool Boolean::IsFalse() {
  return !HBoolean::Value(addr());
}


Number* Number::NewDouble(double value) {
  return Cast<Number>(HNumber::New(
        ISOLATE->heap, Heap::kTenureNew, value));
}


Number* Number::NewIntegral(int64_t value) {
  return Cast<Number>(HNumber::New(ISOLATE->heap, value));
}


double Number::Value() {
  return HNumber::DoubleValue(addr());
}


int64_t Number::IntegralValue() {
  return HNumber::IntegralValue(addr());
}


bool Number::IsIntegral() {
  return HNumber::IsIntegral(addr());
}


String* String::New(const char* value, uint32_t len) {
  return Cast<String>(HString::New(
        ISOLATE->heap, Heap::kTenureNew, value, len));
}


String* String::New(const char* value) {
  return Cast<String>(HString::New(
        ISOLATE->heap, Heap::kTenureNew, value, strlen(value)));
}


const char* String::Value() {
  return HString::Value(ISOLATE->heap, addr());
}


uint32_t String::Length() {
  return HString::Length(addr());
}


Object* Object::New() {
  return Cast<Object>(HObject::NewEmpty(ISOLATE->heap));
}


void Object::Set(Value* key, Value* value) {
  char** slot = HObject::LookupProperty(ISOLATE->heap,
                                        addr(),
                                        key->addr(),
                                        1);
  *slot = value->addr();
}


Value* Object::Get(Value* key) {
  return Value::New(*HObject::LookupProperty(ISOLATE->heap,
                                             addr(),
                                             key->addr(),
                                             0));
}


void Object::Delete(Value* key) {
  RuntimeDeleteProperty(ISOLATE->heap, addr(), key->addr());
}


void Object::Set(const char* key, Value* value) {
  return Set(String::New(key), value);
}


Value* Object::Get(const char* key) {
  return Get(String::New(key));
}


void Object::Delete(const char* key) {
  return Delete(String::New(key));
}


Array* Object::Keys() {
  return Cast<Array>(RuntimeKeysof(ISOLATE->heap, addr()));
}


Object* Object::Clone() {
  return Cast<Object>(RuntimeCloneObject(
        ISOLATE->heap, addr()));
}


Array* Array::New() {
  return Cast<Array>(HArray::NewEmpty(ISOLATE->heap));
}


void Array::Set(int64_t key, Value* value) {
  char** slot = HObject::LookupProperty(ISOLATE->heap,
                                        addr(),
                                        HNumber::ToPointer(key),
                                        1);
  *slot = value->addr();
}


Value* Array::Get(int64_t key) {
  return Value::New(*HObject::LookupProperty(ISOLATE->heap,
                                             addr(),
                                             HNumber::ToPointer(key),
                                             1));
}


void Array::Delete(int64_t key) {
  RuntimeDeleteProperty(ISOLATE->heap, addr(), HNumber::ToPointer(key));
}


int64_t Array::Length() {
  return HArray::Length(addr(), true);
}


CData* CData::New(size_t size) {
  return Cast<CData>(HCData::New(ISOLATE->heap, size));
}


void* CData::GetContents() {
  return HCData::Data(addr());
}


CWrapper::CWrapper(const int* magic) : isolate(ISOLATE), magic_(magic) {
  CData* data = CData::New(sizeof(this));

  // Save pointer of class
  *reinterpret_cast<CWrapper**>(data->GetContents()) = this;

  // Mark data as weak
  data->SetWeakCallback(CWrapper::WeakCallback);
  ref.Wrap(data);
  ref.Unref();
}


CWrapper::~CWrapper() {
  ref.Unwrap();
}


void CWrapper::Ref() {
  ref.Ref();
}


void CWrapper::Unref() {
  ref.Unref();
}


bool CWrapper::IsWeak() {
  return ref.IsWeak();
}


bool CWrapper::IsPersistent() {
  return ref.IsPersistent();
}


void CWrapper::WeakCallback(Value* data) {
  CWrapper* wrapper = *reinterpret_cast<CWrapper**>(
      data->As<CData>()->GetContents());
  delete wrapper;
}

}  // namespace candor
