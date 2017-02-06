// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjse/include/cfxjse_arguments.h"
#include "fxjse/include/cfxjse_class.h"
#include "fxjse/include/cfxjse_value.h"

static void FXJSE_DynPropGetterAdapter_MethodCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> hCallBackInfo = info.Data().As<v8::Object>();
  FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      hCallBackInfo->GetAlignedPointerFromInternalField(0));
  v8::Local<v8::String> hPropName =
      hCallBackInfo->GetInternalField(1).As<v8::String>();
  ASSERT(lpClass && !hPropName.IsEmpty());
  v8::String::Utf8Value szPropName(hPropName);
  CFX_ByteStringC szFxPropName = *szPropName;
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(info.This());
  std::unique_ptr<CFXJSE_Value> lpRetValue(new CFXJSE_Value(info.GetIsolate()));
  CFXJSE_Arguments impl(&info, lpRetValue.get());
  lpClass->dynMethodCall(lpThisValue.get(), szFxPropName, impl);
  if (!lpRetValue->DirectGetValue().IsEmpty()) {
    info.GetReturnValue().Set(lpRetValue->DirectGetValue());
  }
}

static void FXJSE_DynPropGetterAdapter(const FXJSE_CLASS_DESCRIPTOR* lpClass,
                                       CFXJSE_Value* pObject,
                                       const CFX_ByteStringC& szPropName,
                                       CFXJSE_Value* pValue) {
  ASSERT(lpClass);
  int32_t nPropType =
      lpClass->dynPropTypeGetter == nullptr
          ? FXJSE_ClassPropType_Property
          : lpClass->dynPropTypeGetter(pObject, szPropName, FALSE);
  if (nPropType == FXJSE_ClassPropType_Property) {
    if (lpClass->dynPropGetter) {
      lpClass->dynPropGetter(pObject, szPropName, pValue);
    }
  } else if (nPropType == FXJSE_ClassPropType_Method) {
    if (lpClass->dynMethodCall && pValue) {
      v8::Isolate* pIsolate = pValue->GetIsolate();
      v8::HandleScope hscope(pIsolate);
      v8::Local<v8::ObjectTemplate> hCallBackInfoTemplate =
          v8::ObjectTemplate::New(pIsolate);
      hCallBackInfoTemplate->SetInternalFieldCount(2);
      v8::Local<v8::Object> hCallBackInfo =
          hCallBackInfoTemplate->NewInstance();
      hCallBackInfo->SetAlignedPointerInInternalField(
          0, const_cast<FXJSE_CLASS_DESCRIPTOR*>(lpClass));
      hCallBackInfo->SetInternalField(
          1, v8::String::NewFromUtf8(
                 pIsolate, reinterpret_cast<const char*>(szPropName.raw_str()),
                 v8::String::kNormalString, szPropName.GetLength()));
      pValue->ForceSetValue(
          v8::Function::New(pValue->GetIsolate()->GetCurrentContext(),
                            FXJSE_DynPropGetterAdapter_MethodCallback,
                            hCallBackInfo, 0, v8::ConstructorBehavior::kThrow)
              .ToLocalChecked());
    }
  }
}

static void FXJSE_DynPropSetterAdapter(const FXJSE_CLASS_DESCRIPTOR* lpClass,
                                       CFXJSE_Value* pObject,
                                       const CFX_ByteStringC& szPropName,
                                       CFXJSE_Value* pValue) {
  ASSERT(lpClass);
  int32_t nPropType =
      lpClass->dynPropTypeGetter == nullptr
          ? FXJSE_ClassPropType_Property
          : lpClass->dynPropTypeGetter(pObject, szPropName, FALSE);
  if (nPropType != FXJSE_ClassPropType_Method) {
    if (lpClass->dynPropSetter) {
      lpClass->dynPropSetter(pObject, szPropName, pValue);
    }
  }
}

static FX_BOOL FXJSE_DynPropQueryAdapter(const FXJSE_CLASS_DESCRIPTOR* lpClass,
                                         CFXJSE_Value* pObject,
                                         const CFX_ByteStringC& szPropName) {
  ASSERT(lpClass);
  int32_t nPropType =
      lpClass->dynPropTypeGetter == nullptr
          ? FXJSE_ClassPropType_Property
          : lpClass->dynPropTypeGetter(pObject, szPropName, TRUE);
  return nPropType != FXJSE_ClassPropType_None;
}

static FX_BOOL FXJSE_DynPropDeleterAdapter(
    const FXJSE_CLASS_DESCRIPTOR* lpClass,
    CFXJSE_Value* pObject,
    const CFX_ByteStringC& szPropName) {
  ASSERT(lpClass);
  int32_t nPropType =
      lpClass->dynPropTypeGetter == nullptr
          ? FXJSE_ClassPropType_Property
          : lpClass->dynPropTypeGetter(pObject, szPropName, FALSE);
  if (nPropType != FXJSE_ClassPropType_Method) {
    if (lpClass->dynPropDeleter) {
      return lpClass->dynPropDeleter(pObject, szPropName);
    } else {
      return nPropType == FXJSE_ClassPropType_Property ? FALSE : TRUE;
    }
  }
  return FALSE;
}

static void FXJSE_V8_GenericNamedPropertyQueryCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Integer>& info) {
  v8::Local<v8::Object> thisObject = info.This();
  const FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      info.Data().As<v8::External>()->Value());
  v8::Isolate* pIsolate = info.GetIsolate();
  v8::HandleScope scope(pIsolate);
  v8::String::Utf8Value szPropName(property);
  CFX_ByteStringC szFxPropName(*szPropName, szPropName.length());
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(thisObject);
  if (FXJSE_DynPropQueryAdapter(lpClass, lpThisValue.get(), szFxPropName)) {
    info.GetReturnValue().Set(v8::DontDelete);
  } else {
    const int32_t iV8Absent = 64;
    info.GetReturnValue().Set(iV8Absent);
  }
}

static void FXJSE_V8_GenericNamedPropertyDeleterCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  v8::Local<v8::Object> thisObject = info.This();
  const FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      info.Data().As<v8::External>()->Value());
  v8::Isolate* pIsolate = info.GetIsolate();
  v8::HandleScope scope(pIsolate);
  v8::String::Utf8Value szPropName(property);
  CFX_ByteStringC szFxPropName(*szPropName, szPropName.length());
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(thisObject);
  info.GetReturnValue().Set(
      !!FXJSE_DynPropDeleterAdapter(lpClass, lpThisValue.get(), szFxPropName));
}

static void FXJSE_V8_GenericNamedPropertyGetterCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> thisObject = info.This();
  const FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      info.Data().As<v8::External>()->Value());
  v8::String::Utf8Value szPropName(property);
  CFX_ByteStringC szFxPropName(*szPropName, szPropName.length());
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(thisObject);
  std::unique_ptr<CFXJSE_Value> lpNewValue(new CFXJSE_Value(info.GetIsolate()));
  FXJSE_DynPropGetterAdapter(lpClass, lpThisValue.get(), szFxPropName,
                             lpNewValue.get());
  info.GetReturnValue().Set(lpNewValue->DirectGetValue());
}

static void FXJSE_V8_GenericNamedPropertySetterCallback(
    v8::Local<v8::Name> property,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> thisObject = info.This();
  const FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      info.Data().As<v8::External>()->Value());
  v8::String::Utf8Value szPropName(property);
  CFX_ByteStringC szFxPropName(*szPropName, szPropName.length());
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(thisObject);

  CFXJSE_Value* lpNewValue = new CFXJSE_Value(info.GetIsolate());
  lpNewValue->ForceSetValue(value);
  FXJSE_DynPropSetterAdapter(lpClass, lpThisValue.get(), szFxPropName,
                             lpNewValue);
  info.GetReturnValue().Set(value);
}

static void FXJSE_V8_GenericNamedPropertyEnumeratorCallback(
    const v8::PropertyCallbackInfo<v8::Array>& info) {
  const FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      info.Data().As<v8::External>()->Value());
  v8::Isolate* pIsolate = info.GetIsolate();
  v8::Local<v8::Array> newArray = v8::Array::New(pIsolate, lpClass->propNum);
  for (int i = 0; i < lpClass->propNum; i++) {
    newArray->Set(
        i, v8::String::NewFromUtf8(pIsolate, lpClass->properties[i].name));
  }
  info.GetReturnValue().Set(newArray);
}

void CFXJSE_Class::SetUpNamedPropHandler(
    v8::Isolate* pIsolate,
    v8::Local<v8::ObjectTemplate>& hObjectTemplate,
    const FXJSE_CLASS_DESCRIPTOR* lpClassDefinition) {
  v8::NamedPropertyHandlerConfiguration configuration(
      lpClassDefinition->dynPropGetter
          ? FXJSE_V8_GenericNamedPropertyGetterCallback
          : 0,
      lpClassDefinition->dynPropSetter
          ? FXJSE_V8_GenericNamedPropertySetterCallback
          : 0,
      lpClassDefinition->dynPropTypeGetter
          ? FXJSE_V8_GenericNamedPropertyQueryCallback
          : 0,
      lpClassDefinition->dynPropDeleter
          ? FXJSE_V8_GenericNamedPropertyDeleterCallback
          : 0,
      FXJSE_V8_GenericNamedPropertyEnumeratorCallback,
      v8::External::New(pIsolate,
                        const_cast<FXJSE_CLASS_DESCRIPTOR*>(lpClassDefinition)),
      v8::PropertyHandlerFlags::kNonMasking);
  hObjectTemplate->SetHandler(configuration);
}
