// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/fxjs_v8.h"

#include <vector>

#include "core/fxcrt/fx_basic.h"

// Keep this consistent with the values defined in gin/public/context_holder.h
// (without actually requiring a dependency on gin itself for the standalone
// embedders of PDFIum). The value we want to use is:
//   kPerContextDataStartIndex + kEmbedderPDFium, which is 3.
static const unsigned int kPerContextDataIndex = 3u;
static unsigned int g_embedderDataSlot = 1u;
static v8::Isolate* g_isolate = nullptr;
static size_t g_isolate_ref_count = 0;
static FXJS_ArrayBufferAllocator* g_arrayBufferAllocator = nullptr;
static v8::Global<v8::ObjectTemplate>* g_DefaultGlobalObjectTemplate = nullptr;

class CFXJS_PerObjectData {
 public:
  explicit CFXJS_PerObjectData(int nObjDefID)
      : m_ObjDefID(nObjDefID), m_pPrivate(nullptr) {}

  const int m_ObjDefID;
  void* m_pPrivate;
};

class CFXJS_ObjDefinition {
 public:
  static int MaxID(v8::Isolate* pIsolate) {
    return FXJS_PerIsolateData::Get(pIsolate)->m_ObjectDefnArray.size();
  }

  static CFXJS_ObjDefinition* ForID(v8::Isolate* pIsolate, int id) {
    // Note: GetAt() halts if out-of-range even in release builds.
    return FXJS_PerIsolateData::Get(pIsolate)->m_ObjectDefnArray[id].get();
  }

  CFXJS_ObjDefinition(v8::Isolate* isolate,
                      const wchar_t* sObjName,
                      FXJSOBJTYPE eObjType,
                      CFXJS_Engine::Constructor pConstructor,
                      CFXJS_Engine::Destructor pDestructor)
      : m_ObjName(sObjName),
        m_ObjType(eObjType),
        m_pConstructor(pConstructor),
        m_pDestructor(pDestructor),
        m_pIsolate(isolate) {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    v8::Local<v8::FunctionTemplate> fun = v8::FunctionTemplate::New(isolate);
    fun->InstanceTemplate()->SetInternalFieldCount(2);
    fun->SetCallHandler([](const v8::FunctionCallbackInfo<v8::Value>& info) {
      v8::Local<v8::Object> holder = info.Holder();
      ASSERT(holder->InternalFieldCount() == 2);
      holder->SetAlignedPointerInInternalField(0, nullptr);
      holder->SetAlignedPointerInInternalField(1, nullptr);
    });
    if (eObjType == FXJSOBJTYPE_GLOBAL) {
      fun->InstanceTemplate()->Set(
          v8::Symbol::GetToStringTag(isolate),
          v8::String::NewFromUtf8(isolate, "global", v8::NewStringType::kNormal)
              .ToLocalChecked());
    }
    m_FunctionTemplate.Reset(isolate, fun);

    v8::Local<v8::Signature> sig = v8::Signature::New(isolate, fun);
    m_Signature.Reset(isolate, sig);
  }

  int AssignID() {
    FXJS_PerIsolateData* pData = FXJS_PerIsolateData::Get(m_pIsolate);
    pData->m_ObjectDefnArray.emplace_back(this);
    return pData->m_ObjectDefnArray.size() - 1;
  }

  v8::Local<v8::ObjectTemplate> GetInstanceTemplate() {
    v8::EscapableHandleScope scope(m_pIsolate);
    v8::Local<v8::FunctionTemplate> function =
        m_FunctionTemplate.Get(m_pIsolate);
    return scope.Escape(function->InstanceTemplate());
  }

  v8::Local<v8::Signature> GetSignature() {
    v8::EscapableHandleScope scope(m_pIsolate);
    return scope.Escape(m_Signature.Get(m_pIsolate));
  }

  const wchar_t* const m_ObjName;
  const FXJSOBJTYPE m_ObjType;
  const CFXJS_Engine::Constructor m_pConstructor;
  const CFXJS_Engine::Destructor m_pDestructor;

  v8::Isolate* m_pIsolate;
  v8::Global<v8::FunctionTemplate> m_FunctionTemplate;
  v8::Global<v8::Signature> m_Signature;
};

static v8::Local<v8::ObjectTemplate> GetGlobalObjectTemplate(
    v8::Isolate* pIsolate) {
  int maxID = CFXJS_ObjDefinition::MaxID(pIsolate);
  for (int i = 0; i < maxID; ++i) {
    CFXJS_ObjDefinition* pObjDef = CFXJS_ObjDefinition::ForID(pIsolate, i);
    if (pObjDef->m_ObjType == FXJSOBJTYPE_GLOBAL)
      return pObjDef->GetInstanceTemplate();
  }
  if (!g_DefaultGlobalObjectTemplate) {
    v8::Local<v8::ObjectTemplate> hGlobalTemplate =
        v8::ObjectTemplate::New(pIsolate);
    hGlobalTemplate->Set(
        v8::Symbol::GetToStringTag(pIsolate),
        v8::String::NewFromUtf8(pIsolate, "global", v8::NewStringType::kNormal)
            .ToLocalChecked());
    g_DefaultGlobalObjectTemplate =
        new v8::Global<v8::ObjectTemplate>(pIsolate, hGlobalTemplate);
  }
  return g_DefaultGlobalObjectTemplate->Get(pIsolate);
}

void* FXJS_ArrayBufferAllocator::Allocate(size_t length) {
  return calloc(1, length);
}

void* FXJS_ArrayBufferAllocator::AllocateUninitialized(size_t length) {
  return malloc(length);
}

void FXJS_ArrayBufferAllocator::Free(void* data, size_t length) {
  free(data);
}

void V8TemplateMapTraits::Dispose(v8::Isolate* isolate,
                                  v8::Global<v8::Object> value,
                                  void* key) {
  v8::Local<v8::Object> obj = value.Get(isolate);
  if (obj.IsEmpty())
    return;
  CFXJS_Engine* pEngine = CFXJS_Engine::CurrentEngineFromIsolate(isolate);
  int id = pEngine->GetObjDefnID(obj);
  if (id == -1)
    return;
  CFXJS_ObjDefinition* pObjDef = CFXJS_ObjDefinition::ForID(isolate, id);
  if (!pObjDef)
    return;
  if (pObjDef->m_pDestructor)
    pObjDef->m_pDestructor(pEngine, obj);
  CFXJS_Engine::FreeObjectPrivate(obj);
}

V8TemplateMapTraits::MapType* V8TemplateMapTraits::MapFromWeakCallbackInfo(
    const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {
  V8TemplateMap* pMap =
      (FXJS_PerIsolateData::Get(data.GetIsolate()))->m_pDynamicObjsMap.get();
  return pMap ? &pMap->m_map : nullptr;
}

void FXJS_Initialize(unsigned int embedderDataSlot, v8::Isolate* pIsolate) {
  if (g_isolate) {
    ASSERT(g_embedderDataSlot == embedderDataSlot);
    ASSERT(g_isolate == pIsolate);
    return;
  }
  g_embedderDataSlot = embedderDataSlot;
  g_isolate = pIsolate;
}

void FXJS_Release() {
  ASSERT(!g_isolate || g_isolate_ref_count == 0);
  delete g_DefaultGlobalObjectTemplate;
  g_DefaultGlobalObjectTemplate = nullptr;
  g_isolate = nullptr;

  delete g_arrayBufferAllocator;
  g_arrayBufferAllocator = nullptr;
}

bool FXJS_GetIsolate(v8::Isolate** pResultIsolate) {
  if (g_isolate) {
    *pResultIsolate = g_isolate;
    return false;
  }
  // Provide backwards compatibility when no external isolate.
  if (!g_arrayBufferAllocator)
    g_arrayBufferAllocator = new FXJS_ArrayBufferAllocator();
  v8::Isolate::CreateParams params;
  params.array_buffer_allocator = g_arrayBufferAllocator;
  *pResultIsolate = v8::Isolate::New(params);
  return true;
}

size_t FXJS_GlobalIsolateRefCount() {
  return g_isolate_ref_count;
}

V8TemplateMap::V8TemplateMap(v8::Isolate* isolate) : m_map(isolate) {}

V8TemplateMap::~V8TemplateMap() {}

void V8TemplateMap::set(void* key, v8::Local<v8::Object> handle) {
  ASSERT(!m_map.Contains(key));
  m_map.Set(key, handle);
}

FXJS_PerIsolateData::~FXJS_PerIsolateData() {}

// static
void FXJS_PerIsolateData::SetUp(v8::Isolate* pIsolate) {
  if (!pIsolate->GetData(g_embedderDataSlot))
    pIsolate->SetData(g_embedderDataSlot, new FXJS_PerIsolateData(pIsolate));
}

// static
FXJS_PerIsolateData* FXJS_PerIsolateData::Get(v8::Isolate* pIsolate) {
  return static_cast<FXJS_PerIsolateData*>(
      pIsolate->GetData(g_embedderDataSlot));
}

FXJS_PerIsolateData::FXJS_PerIsolateData(v8::Isolate* pIsolate)
    : m_pDynamicObjsMap(new V8TemplateMap(pIsolate)) {}

CFXJS_Engine::CFXJS_Engine() : m_isolate(nullptr) {}

CFXJS_Engine::CFXJS_Engine(v8::Isolate* pIsolate) : m_isolate(pIsolate) {}

CFXJS_Engine::~CFXJS_Engine() {
  m_V8PersistentContext.Reset();
}

// static
CFXJS_Engine* CFXJS_Engine::CurrentEngineFromIsolate(v8::Isolate* pIsolate) {
  return static_cast<CFXJS_Engine*>(
      pIsolate->GetCurrentContext()->GetAlignedPointerFromEmbedderData(
          kPerContextDataIndex));
}

// static
int CFXJS_Engine::GetObjDefnID(v8::Local<v8::Object> pObj) {
  if (pObj.IsEmpty() || !pObj->InternalFieldCount())
    return -1;
  CFXJS_PerObjectData* pPerObjectData = static_cast<CFXJS_PerObjectData*>(
      pObj->GetAlignedPointerFromInternalField(0));
  if (!pPerObjectData)
    return -1;
  return pPerObjectData->m_ObjDefID;
}

// static
void CFXJS_Engine::FreeObjectPrivate(void* pPerObjectData) {
  delete static_cast<CFXJS_PerObjectData*>(pPerObjectData);
}

// static
void CFXJS_Engine::FreeObjectPrivate(v8::Local<v8::Object> pObj) {
  if (pObj.IsEmpty() || !pObj->InternalFieldCount())
    return;
  FreeObjectPrivate(pObj->GetAlignedPointerFromInternalField(0));
  pObj->SetAlignedPointerInInternalField(0, nullptr);
}

int CFXJS_Engine::DefineObj(const wchar_t* sObjName,
                            FXJSOBJTYPE eObjType,
                            CFXJS_Engine::Constructor pConstructor,
                            CFXJS_Engine::Destructor pDestructor) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  FXJS_PerIsolateData::SetUp(m_isolate);
  CFXJS_ObjDefinition* pObjDef = new CFXJS_ObjDefinition(
      m_isolate, sObjName, eObjType, pConstructor, pDestructor);
  return pObjDef->AssignID();
}

void CFXJS_Engine::DefineObjMethod(int nObjDefnID,
                                   const wchar_t* sMethodName,
                                   v8::FunctionCallback pMethodCall) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  CFX_ByteString bsMethodName = CFX_WideString(sMethodName).UTF8Encode();
  CFXJS_ObjDefinition* pObjDef =
      CFXJS_ObjDefinition::ForID(m_isolate, nObjDefnID);
  v8::Local<v8::FunctionTemplate> fun = v8::FunctionTemplate::New(
      m_isolate, pMethodCall, v8::Local<v8::Value>(), pObjDef->GetSignature());
  fun->RemovePrototype();
  pObjDef->GetInstanceTemplate()->Set(
      v8::String::NewFromUtf8(m_isolate, bsMethodName.c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked(),
      fun, v8::ReadOnly);
}

void CFXJS_Engine::DefineObjProperty(int nObjDefnID,
                                     const wchar_t* sPropName,
                                     v8::AccessorGetterCallback pPropGet,
                                     v8::AccessorSetterCallback pPropPut) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  CFX_ByteString bsPropertyName = CFX_WideString(sPropName).UTF8Encode();
  CFXJS_ObjDefinition* pObjDef =
      CFXJS_ObjDefinition::ForID(m_isolate, nObjDefnID);
  pObjDef->GetInstanceTemplate()->SetAccessor(
      v8::String::NewFromUtf8(m_isolate, bsPropertyName.c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked(),
      pPropGet, pPropPut);
}

void CFXJS_Engine::DefineObjAllProperties(
    int nObjDefnID,
    v8::NamedPropertyQueryCallback pPropQurey,
    v8::NamedPropertyGetterCallback pPropGet,
    v8::NamedPropertySetterCallback pPropPut,
    v8::NamedPropertyDeleterCallback pPropDel) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  CFXJS_ObjDefinition* pObjDef =
      CFXJS_ObjDefinition::ForID(m_isolate, nObjDefnID);
  pObjDef->GetInstanceTemplate()->SetNamedPropertyHandler(pPropGet, pPropPut,
                                                          pPropQurey, pPropDel);
}

void CFXJS_Engine::DefineObjConst(int nObjDefnID,
                                  const wchar_t* sConstName,
                                  v8::Local<v8::Value> pDefault) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  CFX_ByteString bsConstName = CFX_WideString(sConstName).UTF8Encode();
  CFXJS_ObjDefinition* pObjDef =
      CFXJS_ObjDefinition::ForID(m_isolate, nObjDefnID);
  pObjDef->GetInstanceTemplate()->Set(m_isolate, bsConstName.c_str(), pDefault);
}

void CFXJS_Engine::DefineGlobalMethod(const wchar_t* sMethodName,
                                      v8::FunctionCallback pMethodCall) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  CFX_ByteString bsMethodName = CFX_WideString(sMethodName).UTF8Encode();
  v8::Local<v8::FunctionTemplate> fun =
      v8::FunctionTemplate::New(m_isolate, pMethodCall);
  fun->RemovePrototype();
  GetGlobalObjectTemplate(m_isolate)->Set(
      v8::String::NewFromUtf8(m_isolate, bsMethodName.c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked(),
      fun, v8::ReadOnly);
}

void CFXJS_Engine::DefineGlobalConst(const wchar_t* sConstName,
                                     v8::FunctionCallback pConstGetter) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  CFX_ByteString bsConst = CFX_WideString(sConstName).UTF8Encode();
  v8::Local<v8::FunctionTemplate> fun =
      v8::FunctionTemplate::New(m_isolate, pConstGetter);
  fun->RemovePrototype();
  GetGlobalObjectTemplate(m_isolate)->SetAccessorProperty(
      v8::String::NewFromUtf8(m_isolate, bsConst.c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked(),
      fun);
}

void CFXJS_Engine::InitializeEngine() {
  if (m_isolate == g_isolate)
    ++g_isolate_ref_count;

  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);

  // This has to happen before we call GetGlobalObjectTemplate because that
  // method gets the PerIsolateData from m_isolate.
  FXJS_PerIsolateData::SetUp(m_isolate);

  v8::Local<v8::Context> v8Context =
      v8::Context::New(m_isolate, nullptr, GetGlobalObjectTemplate(m_isolate));
  v8::Context::Scope context_scope(v8Context);

  v8Context->SetAlignedPointerInEmbedderData(kPerContextDataIndex, this);

  int maxID = CFXJS_ObjDefinition::MaxID(m_isolate);
  m_StaticObjects.resize(maxID + 1);
  for (int i = 0; i < maxID; ++i) {
    CFXJS_ObjDefinition* pObjDef = CFXJS_ObjDefinition::ForID(m_isolate, i);
    if (pObjDef->m_ObjType == FXJSOBJTYPE_GLOBAL) {
      v8Context->Global()
          ->GetPrototype()
          ->ToObject(v8Context)
          .ToLocalChecked()
          ->SetAlignedPointerInInternalField(0, new CFXJS_PerObjectData(i));

      if (pObjDef->m_pConstructor)
        pObjDef->m_pConstructor(this, v8Context->Global()
                                          ->GetPrototype()
                                          ->ToObject(v8Context)
                                          .ToLocalChecked());
    } else if (pObjDef->m_ObjType == FXJSOBJTYPE_STATIC) {
      CFX_ByteString bs = CFX_WideString(pObjDef->m_ObjName).UTF8Encode();
      v8::Local<v8::String> m_ObjName =
          v8::String::NewFromUtf8(m_isolate, bs.c_str(),
                                  v8::NewStringType::kNormal, bs.GetLength())
              .ToLocalChecked();

      v8::Local<v8::Object> obj = NewFxDynamicObj(i, true);
      v8Context->Global()->Set(v8Context, m_ObjName, obj).FromJust();
      m_StaticObjects[i] = new v8::Global<v8::Object>(m_isolate, obj);
    }
  }
  m_V8PersistentContext.Reset(m_isolate, v8Context);
}

void CFXJS_Engine::ReleaseEngine() {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::HandleScope handle_scope(m_isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(m_isolate, m_V8PersistentContext);
  v8::Context::Scope context_scope(context);

  FXJS_PerIsolateData* pData = FXJS_PerIsolateData::Get(m_isolate);
  if (!pData)
    return;

  m_ConstArrays.clear();

  int maxID = CFXJS_ObjDefinition::MaxID(m_isolate);
  for (int i = 0; i < maxID; ++i) {
    CFXJS_ObjDefinition* pObjDef = CFXJS_ObjDefinition::ForID(m_isolate, i);
    v8::Local<v8::Object> pObj;
    if (pObjDef->m_ObjType == FXJSOBJTYPE_GLOBAL) {
      pObj =
          context->Global()->GetPrototype()->ToObject(context).ToLocalChecked();
    } else if (m_StaticObjects[i] && !m_StaticObjects[i]->IsEmpty()) {
      pObj = v8::Local<v8::Object>::New(m_isolate, *m_StaticObjects[i]);
      delete m_StaticObjects[i];
      m_StaticObjects[i] = nullptr;
    }

    if (!pObj.IsEmpty()) {
      if (pObjDef->m_pDestructor)
        pObjDef->m_pDestructor(this, pObj);
      FreeObjectPrivate(pObj);
    }
  }

  m_V8PersistentContext.Reset();

  if (m_isolate == g_isolate && --g_isolate_ref_count > 0)
    return;

  delete pData;
  m_isolate->SetData(g_embedderDataSlot, nullptr);
}

int CFXJS_Engine::Execute(const CFX_WideString& script, FXJSErr* pError) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::TryCatch try_catch(m_isolate);
  CFX_ByteString bsScript = script.UTF8Encode();
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  v8::Local<v8::Script> compiled_script;
  if (!v8::Script::Compile(context,
                           v8::String::NewFromUtf8(m_isolate, bsScript.c_str(),
                                                   v8::NewStringType::kNormal,
                                                   bsScript.GetLength())
                               .ToLocalChecked())
           .ToLocal(&compiled_script)) {
    v8::String::Utf8Value error(try_catch.Exception());
    // TODO(tsepez): return error via pError->message.
    return -1;
  }

  v8::Local<v8::Value> result;
  if (!compiled_script->Run(context).ToLocal(&result)) {
    v8::String::Utf8Value error(try_catch.Exception());
    // TODO(tsepez): return error via pError->message.
    return -1;
  }
  return 0;
}

v8::Local<v8::Object> CFXJS_Engine::NewFxDynamicObj(int nObjDefnID,
                                                    bool bStatic) {
  v8::Isolate::Scope isolate_scope(m_isolate);
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  if (nObjDefnID == -1) {
    v8::Local<v8::ObjectTemplate> objTempl = v8::ObjectTemplate::New(m_isolate);
    v8::Local<v8::Object> obj;
    if (!objTempl->NewInstance(context).ToLocal(&obj))
      return v8::Local<v8::Object>();
    return obj;
  }

  FXJS_PerIsolateData* pData = FXJS_PerIsolateData::Get(m_isolate);
  if (!pData)
    return v8::Local<v8::Object>();

  if (nObjDefnID < 0 || nObjDefnID >= CFXJS_ObjDefinition::MaxID(m_isolate))
    return v8::Local<v8::Object>();

  CFXJS_ObjDefinition* pObjDef =
      CFXJS_ObjDefinition::ForID(m_isolate, nObjDefnID);
  v8::Local<v8::Object> obj;
  if (!pObjDef->GetInstanceTemplate()->NewInstance(context).ToLocal(&obj))
    return v8::Local<v8::Object>();

  CFXJS_PerObjectData* pPerObjData = new CFXJS_PerObjectData(nObjDefnID);
  obj->SetAlignedPointerInInternalField(0, pPerObjData);
  if (pObjDef->m_pConstructor)
    pObjDef->m_pConstructor(this, obj);

  if (!bStatic && FXJS_PerIsolateData::Get(m_isolate)->m_pDynamicObjsMap) {
    FXJS_PerIsolateData::Get(m_isolate)->m_pDynamicObjsMap->set(pPerObjData,
                                                                obj);
  }
  return obj;
}

v8::Local<v8::Object> CFXJS_Engine::GetThisObj() {
  v8::Isolate::Scope isolate_scope(m_isolate);
  if (!FXJS_PerIsolateData::Get(m_isolate))
    return v8::Local<v8::Object>();

  // Return the global object.
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  return context->Global()->GetPrototype()->ToObject(context).ToLocalChecked();
}

void CFXJS_Engine::Error(const CFX_WideString& message) {
  // Conversion from pdfium's wchar_t wide-strings to v8's uint16_t
  // wide-strings isn't handled by v8, so use UTF8 as a common
  // intermediate format.
  CFX_ByteString utf8_message = message.UTF8Encode();
  m_isolate->ThrowException(v8::String::NewFromUtf8(m_isolate,
                                                    utf8_message.c_str(),
                                                    v8::NewStringType::kNormal)
                                .ToLocalChecked());
}

void CFXJS_Engine::SetObjectPrivate(v8::Local<v8::Object> pObj, void* p) {
  if (pObj.IsEmpty() || !pObj->InternalFieldCount())
    return;
  CFXJS_PerObjectData* pPerObjectData = static_cast<CFXJS_PerObjectData*>(
      pObj->GetAlignedPointerFromInternalField(0));
  if (!pPerObjectData)
    return;
  pPerObjectData->m_pPrivate = p;
}

void* CFXJS_Engine::GetObjectPrivate(v8::Local<v8::Object> pObj) {
  if (pObj.IsEmpty())
    return nullptr;
  CFXJS_PerObjectData* pPerObjectData = nullptr;
  if (pObj->InternalFieldCount()) {
    pPerObjectData = static_cast<CFXJS_PerObjectData*>(
        pObj->GetAlignedPointerFromInternalField(0));
  } else {
    // It could be a global proxy object.
    v8::Local<v8::Value> v = pObj->GetPrototype();
    v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
    if (v->IsObject()) {
      pPerObjectData = static_cast<CFXJS_PerObjectData*>(
          v->ToObject(context)
              .ToLocalChecked()
              ->GetAlignedPointerFromInternalField(0));
    }
  }
  return pPerObjectData ? pPerObjectData->m_pPrivate : nullptr;
}

v8::Local<v8::String> CFXJS_Engine::WSToJSString(
    const CFX_WideString& wsPropertyName) {
  v8::Isolate* pIsolate = m_isolate ? m_isolate : v8::Isolate::GetCurrent();
  CFX_ByteString bs = wsPropertyName.UTF8Encode();
  return v8::String::NewFromUtf8(pIsolate, bs.c_str(),
                                 v8::NewStringType::kNormal, bs.GetLength())
      .ToLocalChecked();
}

v8::Local<v8::Value> CFXJS_Engine::GetObjectProperty(
    v8::Local<v8::Object> pObj,
    const CFX_WideString& wsPropertyName) {
  if (pObj.IsEmpty())
    return v8::Local<v8::Value>();
  v8::Local<v8::Value> val;
  if (!pObj->Get(m_isolate->GetCurrentContext(), WSToJSString(wsPropertyName))
           .ToLocal(&val))
    return v8::Local<v8::Value>();
  return val;
}

std::vector<CFX_WideString> CFXJS_Engine::GetObjectPropertyNames(
    v8::Local<v8::Object> pObj) {
  if (pObj.IsEmpty())
    return std::vector<CFX_WideString>();

  v8::Local<v8::Array> val;
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  if (!pObj->GetPropertyNames(context).ToLocal(&val))
    return std::vector<CFX_WideString>();

  std::vector<CFX_WideString> result;
  for (uint32_t i = 0; i < val->Length(); ++i) {
    result.push_back(ToString(val->Get(context, i).ToLocalChecked()));
  }

  return result;
}

void CFXJS_Engine::PutObjectString(v8::Local<v8::Object> pObj,
                                   const CFX_WideString& wsPropertyName,
                                   const CFX_WideString& wsValue) {
  if (pObj.IsEmpty())
    return;
  pObj->Set(m_isolate->GetCurrentContext(), WSToJSString(wsPropertyName),
            WSToJSString(wsValue))
      .FromJust();
}

void CFXJS_Engine::PutObjectNumber(v8::Local<v8::Object> pObj,
                                   const CFX_WideString& wsPropertyName,
                                   int nValue) {
  if (pObj.IsEmpty())
    return;
  pObj->Set(m_isolate->GetCurrentContext(), WSToJSString(wsPropertyName),
            v8::Int32::New(m_isolate, nValue))
      .FromJust();
}

void CFXJS_Engine::PutObjectNumber(v8::Local<v8::Object> pObj,
                                   const CFX_WideString& wsPropertyName,
                                   float fValue) {
  if (pObj.IsEmpty())
    return;
  pObj->Set(m_isolate->GetCurrentContext(), WSToJSString(wsPropertyName),
            v8::Number::New(m_isolate, (double)fValue))
      .FromJust();
}

void CFXJS_Engine::PutObjectNumber(v8::Local<v8::Object> pObj,
                                   const CFX_WideString& wsPropertyName,
                                   double dValue) {
  if (pObj.IsEmpty())
    return;
  pObj->Set(m_isolate->GetCurrentContext(), WSToJSString(wsPropertyName),
            v8::Number::New(m_isolate, (double)dValue))
      .FromJust();
}

void CFXJS_Engine::PutObjectBoolean(v8::Local<v8::Object> pObj,
                                    const CFX_WideString& wsPropertyName,
                                    bool bValue) {
  if (pObj.IsEmpty())
    return;
  pObj->Set(m_isolate->GetCurrentContext(), WSToJSString(wsPropertyName),
            v8::Boolean::New(m_isolate, bValue))
      .FromJust();
}

void CFXJS_Engine::PutObjectObject(v8::Local<v8::Object> pObj,
                                   const CFX_WideString& wsPropertyName,
                                   v8::Local<v8::Object> pPut) {
  if (pObj.IsEmpty())
    return;
  pObj->Set(m_isolate->GetCurrentContext(), WSToJSString(wsPropertyName), pPut)
      .FromJust();
}

void CFXJS_Engine::PutObjectNull(v8::Local<v8::Object> pObj,
                                 const CFX_WideString& wsPropertyName) {
  if (pObj.IsEmpty())
    return;
  pObj->Set(m_isolate->GetCurrentContext(), WSToJSString(wsPropertyName),
            v8::Local<v8::Object>())
      .FromJust();
}

v8::Local<v8::Array> CFXJS_Engine::NewArray() {
  return v8::Array::New(m_isolate);
}

unsigned CFXJS_Engine::PutArrayElement(v8::Local<v8::Array> pArray,
                                       unsigned index,
                                       v8::Local<v8::Value> pValue) {
  if (pArray.IsEmpty())
    return 0;
  if (pArray->Set(m_isolate->GetCurrentContext(), index, pValue).IsNothing())
    return 0;
  return 1;
}

v8::Local<v8::Value> CFXJS_Engine::GetArrayElement(v8::Local<v8::Array> pArray,
                                                   unsigned index) {
  if (pArray.IsEmpty())
    return v8::Local<v8::Value>();
  v8::Local<v8::Value> val;
  if (!pArray->Get(m_isolate->GetCurrentContext(), index).ToLocal(&val))
    return v8::Local<v8::Value>();
  return val;
}

unsigned CFXJS_Engine::GetArrayLength(v8::Local<v8::Array> pArray) {
  if (pArray.IsEmpty())
    return 0;
  return pArray->Length();
}

v8::Local<v8::Context> CFXJS_Engine::NewLocalContext() {
  return v8::Local<v8::Context>::New(m_isolate, m_V8PersistentContext);
}

v8::Local<v8::Context> CFXJS_Engine::GetPersistentContext() {
  return m_V8PersistentContext.Get(m_isolate);
}

v8::Local<v8::Value> CFXJS_Engine::NewNumber(int number) {
  return v8::Int32::New(m_isolate, number);
}

v8::Local<v8::Value> CFXJS_Engine::NewNumber(double number) {
  return v8::Number::New(m_isolate, number);
}

v8::Local<v8::Value> CFXJS_Engine::NewNumber(float number) {
  return v8::Number::New(m_isolate, (float)number);
}

v8::Local<v8::Value> CFXJS_Engine::NewBoolean(bool b) {
  return v8::Boolean::New(m_isolate, b);
}

v8::Local<v8::Value> CFXJS_Engine::NewString(const wchar_t* str) {
  return WSToJSString(str);
}

v8::Local<v8::Value> CFXJS_Engine::NewNull() {
  return v8::Local<v8::Value>();
}

v8::Local<v8::Date> CFXJS_Engine::NewDate(double d) {
  return v8::Date::New(m_isolate->GetCurrentContext(), d)
      .ToLocalChecked()
      .As<v8::Date>();
}

int CFXJS_Engine::ToInt32(v8::Local<v8::Value> pValue) {
  if (pValue.IsEmpty())
    return 0;
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  return pValue->ToInt32(context).ToLocalChecked()->Value();
}

bool CFXJS_Engine::ToBoolean(v8::Local<v8::Value> pValue) {
  if (pValue.IsEmpty())
    return false;
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  return pValue->ToBoolean(context).ToLocalChecked()->Value();
}

double CFXJS_Engine::ToNumber(v8::Local<v8::Value> pValue) {
  if (pValue.IsEmpty())
    return 0.0;
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  return pValue->ToNumber(context).ToLocalChecked()->Value();
}

v8::Local<v8::Object> CFXJS_Engine::ToObject(v8::Local<v8::Value> pValue) {
  if (pValue.IsEmpty())
    return v8::Local<v8::Object>();
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  return pValue->ToObject(context).ToLocalChecked();
}

CFX_WideString CFXJS_Engine::ToString(v8::Local<v8::Value> pValue) {
  if (pValue.IsEmpty())
    return L"";
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  v8::String::Utf8Value s(pValue->ToString(context).ToLocalChecked());
  return CFX_WideString::FromUTF8(CFX_ByteStringC(*s, s.length()));
}

v8::Local<v8::Array> CFXJS_Engine::ToArray(v8::Local<v8::Value> pValue) {
  if (pValue.IsEmpty())
    return v8::Local<v8::Array>();
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  return v8::Local<v8::Array>::Cast(pValue->ToObject(context).ToLocalChecked());
}

void CFXJS_Engine::SetConstArray(const CFX_WideString& name,
                                 v8::Local<v8::Array> array) {
  m_ConstArrays[name] = v8::Global<v8::Array>(GetIsolate(), array);
}

v8::Local<v8::Array> CFXJS_Engine::GetConstArray(const CFX_WideString& name) {
  return v8::Local<v8::Array>::New(GetIsolate(), m_ConstArrays[name]);
}
