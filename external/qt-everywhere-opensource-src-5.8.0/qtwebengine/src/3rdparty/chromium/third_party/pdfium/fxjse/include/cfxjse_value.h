// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJSE_INCLUDE_CFXJSE_VALUE_H_
#define FXJSE_INCLUDE_CFXJSE_VALUE_H_

#include "fxjse/scope_inline.h"

class CFXJSE_Value {
 public:
  explicit CFXJSE_Value(v8::Isolate* pIsolate);
  ~CFXJSE_Value();

  FX_BOOL IsUndefined() const {
    if (m_hValue.IsEmpty()) {
      return FALSE;
    }
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return hValue->IsUndefined();
  }
  FX_BOOL IsNull() const {
    if (m_hValue.IsEmpty()) {
      return FALSE;
    }
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return hValue->IsNull();
  }
  FX_BOOL IsBoolean() const {
    if (m_hValue.IsEmpty()) {
      return FALSE;
    }
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return hValue->IsBoolean();
  }
  FX_BOOL IsString() const {
    if (m_hValue.IsEmpty()) {
      return FALSE;
    }
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return hValue->IsString();
  }
  FX_BOOL IsNumber() const {
    if (m_hValue.IsEmpty()) {
      return FALSE;
    }
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return hValue->IsNumber();
  }
  FX_BOOL IsInteger() const {
    if (m_hValue.IsEmpty()) {
      return FALSE;
    }
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return hValue->IsInt32();
  }
  FX_BOOL IsObject() const {
    if (m_hValue.IsEmpty()) {
      return FALSE;
    }
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return hValue->IsObject();
  }
  FX_BOOL IsArray() const {
    if (m_hValue.IsEmpty()) {
      return FALSE;
    }
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return hValue->IsArray();
  }
  FX_BOOL IsFunction() const {
    if (m_hValue.IsEmpty()) {
      return FALSE;
    }
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return hValue->IsFunction();
  }
  FX_BOOL IsDate() const {
    if (m_hValue.IsEmpty()) {
      return FALSE;
    }
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return hValue->IsDate();
  }

  FX_BOOL ToBoolean() const {
    ASSERT(!m_hValue.IsEmpty());
    CFXJSE_ScopeUtil_IsolateHandleRootContext scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return static_cast<FX_BOOL>(hValue->BooleanValue());
  }
  FX_FLOAT ToFloat() const {
    ASSERT(!m_hValue.IsEmpty());
    CFXJSE_ScopeUtil_IsolateHandleRootContext scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return static_cast<FX_FLOAT>(hValue->NumberValue());
  }
  double ToDouble() const {
    ASSERT(!m_hValue.IsEmpty());
    CFXJSE_ScopeUtil_IsolateHandleRootContext scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return static_cast<double>(hValue->NumberValue());
  }
  int32_t ToInteger() const {
    ASSERT(!m_hValue.IsEmpty());
    CFXJSE_ScopeUtil_IsolateHandleRootContext scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    return static_cast<int32_t>(hValue->NumberValue());
  }
  CFX_ByteString ToString() const {
    ASSERT(!m_hValue.IsEmpty());
    CFXJSE_ScopeUtil_IsolateHandleRootContext scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Local<v8::Value>::New(m_pIsolate, m_hValue);
    v8::Local<v8::String> hString = hValue->ToString();
    v8::String::Utf8Value hStringVal(hString);
    return CFX_ByteString(*hStringVal);
  }
  CFX_WideString ToWideString() const {
    return CFX_WideString::FromUTF8(ToString().AsStringC());
  }
  CFXJSE_HostObject* ToHostObject(CFXJSE_Class* lpClass) const;

  void SetUndefined() {
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue = v8::Undefined(m_pIsolate);
    m_hValue.Reset(m_pIsolate, hValue);
  }
  void SetNull() {
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue = v8::Null(m_pIsolate);
    m_hValue.Reset(m_pIsolate, hValue);
  }
  void SetBoolean(FX_BOOL bBoolean) {
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue =
        v8::Boolean::New(m_pIsolate, bBoolean != FALSE);
    m_hValue.Reset(m_pIsolate, hValue);
  }
  void SetInteger(int32_t nInteger) {
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue = v8::Integer::New(m_pIsolate, nInteger);
    m_hValue.Reset(m_pIsolate, hValue);
  }
  void SetDouble(double dDouble) {
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue = v8::Number::New(m_pIsolate, dDouble);
    m_hValue.Reset(m_pIsolate, hValue);
  }
  void SetString(const CFX_ByteStringC& szString) {
    CFXJSE_ScopeUtil_IsolateHandle scope(m_pIsolate);
    v8::Local<v8::Value> hValue = v8::String::NewFromUtf8(
        m_pIsolate, reinterpret_cast<const char*>(szString.raw_str()),
        v8::String::kNormalString, szString.GetLength());
    m_hValue.Reset(m_pIsolate, hValue);
  }
  void SetFloat(FX_FLOAT fFloat);
  void SetJSObject() {
    CFXJSE_ScopeUtil_IsolateHandleRootContext scope(m_pIsolate);
    v8::Local<v8::Value> hValue = v8::Object::New(m_pIsolate);
    m_hValue.Reset(m_pIsolate, hValue);
  }

  void SetObject(CFXJSE_HostObject* lpObject, CFXJSE_Class* pClass);
  void SetHostObject(CFXJSE_HostObject* lpObject, CFXJSE_Class* lpClass);
  void SetArray(uint32_t uValueCount, CFXJSE_Value** rgValues);
  void SetDate(double dDouble);

  FX_BOOL GetObjectProperty(const CFX_ByteStringC& szPropName,
                            CFXJSE_Value* lpPropValue);
  FX_BOOL SetObjectProperty(const CFX_ByteStringC& szPropName,
                            CFXJSE_Value* lpPropValue);
  FX_BOOL GetObjectPropertyByIdx(uint32_t uPropIdx, CFXJSE_Value* lpPropValue);
  FX_BOOL SetObjectProperty(uint32_t uPropIdx, CFXJSE_Value* lpPropValue);
  FX_BOOL DeleteObjectProperty(const CFX_ByteStringC& szPropName);
  FX_BOOL HasObjectOwnProperty(const CFX_ByteStringC& szPropName,
                               FX_BOOL bUseTypeGetter);
  FX_BOOL SetObjectOwnProperty(const CFX_ByteStringC& szPropName,
                               CFXJSE_Value* lpPropValue);
  FX_BOOL SetFunctionBind(CFXJSE_Value* lpOldFunction, CFXJSE_Value* lpNewThis);
  FX_BOOL Call(CFXJSE_Value* lpReceiver,
               CFXJSE_Value* lpRetValue,
               uint32_t nArgCount,
               CFXJSE_Value** lpArgs);

  v8::Isolate* GetIsolate() const { return m_pIsolate; }
  const v8::Global<v8::Value>& DirectGetValue() const { return m_hValue; }
  void ForceSetValue(v8::Local<v8::Value> hValue) {
    m_hValue.Reset(m_pIsolate, hValue);
  }
  void Assign(const CFXJSE_Value* lpValue) {
    ASSERT(lpValue);
    if (lpValue) {
      m_hValue.Reset(m_pIsolate, lpValue->m_hValue);
    } else {
      m_hValue.Reset();
    }
  }

 private:
  friend class CFXJSE_Class;
  friend class CFXJSE_Context;

  CFXJSE_Value();
  CFXJSE_Value(const CFXJSE_Value&);
  CFXJSE_Value& operator=(const CFXJSE_Value&);

  v8::Isolate* m_pIsolate;
  v8::Global<v8::Value> m_hValue;
};

#endif  // FXJSE_INCLUDE_CFXJSE_VALUE_H_
