// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjse/include/cfxjse_class.h"

#include "fxjse/context.h"
#include "fxjse/include/cfxjse_arguments.h"
#include "fxjse/include/cfxjse_value.h"
#include "fxjse/scope_inline.h"

static void FXJSE_V8ConstructorCallback_Wrapper(
    const v8::FunctionCallbackInfo<v8::Value>& info);
static void FXJSE_V8FunctionCallback_Wrapper(
    const v8::FunctionCallbackInfo<v8::Value>& info);
static void FXJSE_V8GetterCallback_Wrapper(
    v8::Local<v8::String> property,
    const v8::PropertyCallbackInfo<v8::Value>& info);
static void FXJSE_V8SetterCallback_Wrapper(
    v8::Local<v8::String> property,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info);

static void FXJSE_V8FunctionCallback_Wrapper(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  const FXJSE_FUNCTION_DESCRIPTOR* lpFunctionInfo =
      static_cast<FXJSE_FUNCTION_DESCRIPTOR*>(
          info.Data().As<v8::External>()->Value());
  if (!lpFunctionInfo) {
    return;
  }
  CFX_ByteStringC szFunctionName(lpFunctionInfo->name);
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(info.This());
  std::unique_ptr<CFXJSE_Value> lpRetValue(new CFXJSE_Value(info.GetIsolate()));
  CFXJSE_Arguments impl(&info, lpRetValue.get());
  lpFunctionInfo->callbackProc(lpThisValue.get(), szFunctionName, impl);
  if (!lpRetValue->DirectGetValue().IsEmpty()) {
    info.GetReturnValue().Set(lpRetValue->DirectGetValue());
  }
}

static void FXJSE_V8ClassGlobalConstructorCallback_Wrapper(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  const FXJSE_CLASS_DESCRIPTOR* lpClassDefinition =
      static_cast<FXJSE_CLASS_DESCRIPTOR*>(
          info.Data().As<v8::External>()->Value());
  if (!lpClassDefinition) {
    return;
  }
  CFX_ByteStringC szFunctionName(lpClassDefinition->name);
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(info.This());
  std::unique_ptr<CFXJSE_Value> lpRetValue(new CFXJSE_Value(info.GetIsolate()));
  CFXJSE_Arguments impl(&info, lpRetValue.get());
  lpClassDefinition->constructor(lpThisValue.get(), szFunctionName, impl);
  if (!lpRetValue->DirectGetValue().IsEmpty()) {
    info.GetReturnValue().Set(lpRetValue->DirectGetValue());
  }
}

static void FXJSE_V8GetterCallback_Wrapper(
    v8::Local<v8::String> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  const FXJSE_PROPERTY_DESCRIPTOR* lpPropertyInfo =
      static_cast<FXJSE_PROPERTY_DESCRIPTOR*>(
          info.Data().As<v8::External>()->Value());
  if (!lpPropertyInfo) {
    return;
  }
  CFX_ByteStringC szPropertyName(lpPropertyInfo->name);
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  std::unique_ptr<CFXJSE_Value> lpPropValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(info.This());
  lpPropertyInfo->getProc(lpThisValue.get(), szPropertyName, lpPropValue.get());
  info.GetReturnValue().Set(lpPropValue->DirectGetValue());
}

static void FXJSE_V8SetterCallback_Wrapper(
    v8::Local<v8::String> property,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  const FXJSE_PROPERTY_DESCRIPTOR* lpPropertyInfo =
      static_cast<FXJSE_PROPERTY_DESCRIPTOR*>(
          info.Data().As<v8::External>()->Value());
  if (!lpPropertyInfo) {
    return;
  }
  CFX_ByteStringC szPropertyName(lpPropertyInfo->name);
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  std::unique_ptr<CFXJSE_Value> lpPropValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(info.This());
  lpPropValue->ForceSetValue(value);
  lpPropertyInfo->setProc(lpThisValue.get(), szPropertyName, lpPropValue.get());
}

static void FXJSE_V8ConstructorCallback_Wrapper(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (!info.IsConstructCall()) {
    return;
  }
  const FXJSE_CLASS_DESCRIPTOR* lpClassDefinition =
      static_cast<FXJSE_CLASS_DESCRIPTOR*>(
          info.Data().As<v8::External>()->Value());
  if (!lpClassDefinition) {
    return;
  }
  ASSERT(info.This()->InternalFieldCount());
  info.This()->SetAlignedPointerInInternalField(0, NULL);
}

v8::Isolate* CFXJSE_Arguments::GetRuntime() const {
  return m_pRetValue->GetIsolate();
}

int32_t CFXJSE_Arguments::GetLength() const {
  return m_pInfo->Length();
}

std::unique_ptr<CFXJSE_Value> CFXJSE_Arguments::GetValue(int32_t index) const {
  std::unique_ptr<CFXJSE_Value> lpArgValue(
      new CFXJSE_Value(v8::Isolate::GetCurrent()));
  lpArgValue->ForceSetValue((*m_pInfo)[index]);
  return lpArgValue;
}

FX_BOOL CFXJSE_Arguments::GetBoolean(int32_t index) const {
  return (*m_pInfo)[index]->BooleanValue();
}

int32_t CFXJSE_Arguments::GetInt32(int32_t index) const {
  return static_cast<int32_t>((*m_pInfo)[index]->NumberValue());
}

FX_FLOAT CFXJSE_Arguments::GetFloat(int32_t index) const {
  return static_cast<FX_FLOAT>((*m_pInfo)[index]->NumberValue());
}

CFX_ByteString CFXJSE_Arguments::GetUTF8String(int32_t index) const {
  v8::Local<v8::String> hString = (*m_pInfo)[index]->ToString();
  v8::String::Utf8Value szStringVal(hString);
  return CFX_ByteString(*szStringVal);
}

CFXJSE_HostObject* CFXJSE_Arguments::GetObject(int32_t index,
                                               CFXJSE_Class* pClass) const {
  v8::Local<v8::Value> hValue = (*m_pInfo)[index];
  ASSERT(!hValue.IsEmpty());
  if (!hValue->IsObject())
    return nullptr;
  return FXJSE_RetrieveObjectBinding(hValue.As<v8::Object>(), pClass);
}

CFXJSE_Value* CFXJSE_Arguments::GetReturnValue() {
  return m_pRetValue;
}

static void FXJSE_Context_GlobalObjToString(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  const FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      info.Data().As<v8::External>()->Value());
  if (!lpClass) {
    return;
  }
  if (info.This() == info.Holder() && lpClass->name) {
    CFX_ByteString szStringVal;
    szStringVal.Format("[object %s]", lpClass->name);
    info.GetReturnValue().Set(v8::String::NewFromUtf8(
        info.GetIsolate(), szStringVal.c_str(), v8::String::kNormalString,
        szStringVal.GetLength()));
  } else {
    v8::Local<v8::String> local_str =
        info.This()
            ->ObjectProtoToString(info.GetIsolate()->GetCurrentContext())
            .FromMaybe(v8::Local<v8::String>());
    info.GetReturnValue().Set(local_str);
  }
}

CFXJSE_Class* CFXJSE_Class::Create(
    CFXJSE_Context* lpContext,
    const FXJSE_CLASS_DESCRIPTOR* lpClassDefinition,
    FX_BOOL bIsJSGlobal) {
  if (!lpContext || !lpClassDefinition) {
    return NULL;
  }
  CFXJSE_Class* pClass =
      GetClassFromContext(lpContext, lpClassDefinition->name);
  if (pClass) {
    return pClass;
  }
  v8::Isolate* pIsolate = lpContext->m_pIsolate;
  pClass = new CFXJSE_Class(lpContext);
  pClass->m_szClassName = lpClassDefinition->name;
  pClass->m_lpClassDefinition = lpClassDefinition;
  CFXJSE_ScopeUtil_IsolateHandleRootContext scope(pIsolate);
  v8::Local<v8::FunctionTemplate> hFunctionTemplate = v8::FunctionTemplate::New(
      pIsolate, bIsJSGlobal ? 0 : FXJSE_V8ConstructorCallback_Wrapper,
      v8::External::New(
          pIsolate, const_cast<FXJSE_CLASS_DESCRIPTOR*>(lpClassDefinition)));
  hFunctionTemplate->SetClassName(
      v8::String::NewFromUtf8(pIsolate, lpClassDefinition->name));
  hFunctionTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  v8::Local<v8::ObjectTemplate> hObjectTemplate =
      hFunctionTemplate->InstanceTemplate();
  SetUpNamedPropHandler(pIsolate, hObjectTemplate, lpClassDefinition);

  if (lpClassDefinition->propNum) {
    for (int32_t i = 0; i < lpClassDefinition->propNum; i++) {
      hObjectTemplate->SetNativeDataProperty(
          v8::String::NewFromUtf8(pIsolate,
                                  lpClassDefinition->properties[i].name),
          lpClassDefinition->properties[i].getProc
              ? FXJSE_V8GetterCallback_Wrapper
              : NULL,
          lpClassDefinition->properties[i].setProc
              ? FXJSE_V8SetterCallback_Wrapper
              : NULL,
          v8::External::New(pIsolate, const_cast<FXJSE_PROPERTY_DESCRIPTOR*>(
                                          lpClassDefinition->properties + i)),
          static_cast<v8::PropertyAttribute>(v8::DontDelete));
    }
  }
  if (lpClassDefinition->methNum) {
    for (int32_t i = 0; i < lpClassDefinition->methNum; i++) {
      v8::Local<v8::FunctionTemplate> fun = v8::FunctionTemplate::New(
          pIsolate, FXJSE_V8FunctionCallback_Wrapper,
          v8::External::New(pIsolate, const_cast<FXJSE_FUNCTION_DESCRIPTOR*>(
                                          lpClassDefinition->methods + i)));
      fun->RemovePrototype();
      hObjectTemplate->Set(
          v8::String::NewFromUtf8(pIsolate, lpClassDefinition->methods[i].name),
          fun,
          static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));
    }
  }
  if (lpClassDefinition->constructor) {
    if (bIsJSGlobal) {
      hObjectTemplate->Set(
          v8::String::NewFromUtf8(pIsolate, lpClassDefinition->name),
          v8::FunctionTemplate::New(
              pIsolate, FXJSE_V8ClassGlobalConstructorCallback_Wrapper,
              v8::External::New(pIsolate, const_cast<FXJSE_CLASS_DESCRIPTOR*>(
                                              lpClassDefinition))),
          static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));
    } else {
      v8::Local<v8::Context> hLocalContext =
          v8::Local<v8::Context>::New(pIsolate, lpContext->m_hContext);
      FXJSE_GetGlobalObjectFromContext(hLocalContext)
          ->Set(v8::String::NewFromUtf8(pIsolate, lpClassDefinition->name),
                v8::Function::New(
                    pIsolate, FXJSE_V8ClassGlobalConstructorCallback_Wrapper,
                    v8::External::New(pIsolate,
                                      const_cast<FXJSE_CLASS_DESCRIPTOR*>(
                                          lpClassDefinition))));
    }
  }
  if (bIsJSGlobal) {
    v8::Local<v8::FunctionTemplate> fun = v8::FunctionTemplate::New(
        pIsolate, FXJSE_Context_GlobalObjToString,
        v8::External::New(
            pIsolate, const_cast<FXJSE_CLASS_DESCRIPTOR*>(lpClassDefinition)));
    fun->RemovePrototype();
    hObjectTemplate->Set(v8::String::NewFromUtf8(pIsolate, "toString"), fun);
  }
  pClass->m_hTemplate.Reset(lpContext->m_pIsolate, hFunctionTemplate);
  lpContext->m_rgClasses.push_back(std::unique_ptr<CFXJSE_Class>(pClass));
  return pClass;
}

CFXJSE_Class::CFXJSE_Class(CFXJSE_Context* lpContext)
    : m_lpClassDefinition(nullptr), m_pContext(lpContext) {}

CFXJSE_Class::~CFXJSE_Class() {}

CFXJSE_Class* CFXJSE_Class::GetClassFromContext(CFXJSE_Context* pContext,
                                                const CFX_ByteStringC& szName) {
  for (const auto& pClass : pContext->m_rgClasses) {
    if (pClass->m_szClassName == szName)
      return pClass.get();
  }
  return nullptr;
}
