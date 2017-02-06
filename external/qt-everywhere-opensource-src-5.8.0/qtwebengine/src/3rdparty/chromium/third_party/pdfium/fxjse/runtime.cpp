// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjse/runtime.h"

#include <algorithm>

#include "fpdfsdk/jsapi/include/fxjs_v8.h"
#include "fxjse/scope_inline.h"

// Duplicates fpdfsdk's cjs_runtime.h, but keeps XFA from depending on it.
// TODO(tsepez): make a single version of this.
class FXJSE_ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
  void* Allocate(size_t length) override { return calloc(1, length); }
  void* AllocateUninitialized(size_t length) override { return malloc(length); }
  void Free(void* data, size_t length) override { free(data); }
};

static void FXJSE_KillV8() {
  v8::V8::Dispose();
}

void FXJSE_Initialize() {
  if (!CFXJSE_IsolateTracker::g_pInstance)
    CFXJSE_IsolateTracker::g_pInstance = new CFXJSE_IsolateTracker;

  static FX_BOOL bV8Initialized = FALSE;
  if (bV8Initialized)
    return;

  bV8Initialized = TRUE;
  atexit(FXJSE_KillV8);
}

static void FXJSE_Runtime_DisposeCallback(v8::Isolate* pIsolate, bool bOwned) {
  if (FXJS_PerIsolateData* pData = FXJS_PerIsolateData::Get(pIsolate)) {
    delete pData->m_pFXJSERuntimeData;
    pData->m_pFXJSERuntimeData = nullptr;
  }
  if (bOwned)
    pIsolate->Dispose();
}

void FXJSE_Finalize() {
  if (!CFXJSE_IsolateTracker::g_pInstance)
    return;

  CFXJSE_IsolateTracker::g_pInstance->RemoveAll(FXJSE_Runtime_DisposeCallback);
  delete CFXJSE_IsolateTracker::g_pInstance;
  CFXJSE_IsolateTracker::g_pInstance = nullptr;
}

v8::Isolate* FXJSE_Runtime_Create_Own() {
  v8::Isolate::CreateParams params;
  params.array_buffer_allocator = new FXJSE_ArrayBufferAllocator();
  v8::Isolate* pIsolate = v8::Isolate::New(params);
  ASSERT(pIsolate && CFXJSE_IsolateTracker::g_pInstance);
  CFXJSE_IsolateTracker::g_pInstance->Append(pIsolate);
  return pIsolate;
}

void FXJSE_Runtime_Release(v8::Isolate* pIsolate) {
  if (!pIsolate)
    return;
  CFXJSE_IsolateTracker::g_pInstance->Remove(pIsolate,
                                             FXJSE_Runtime_DisposeCallback);
}

CFXJSE_RuntimeData::CFXJSE_RuntimeData(v8::Isolate* pIsolate)
    : m_pIsolate(pIsolate) {}

CFXJSE_RuntimeData::~CFXJSE_RuntimeData() {}

CFXJSE_RuntimeData* CFXJSE_RuntimeData::Create(v8::Isolate* pIsolate) {
  CFXJSE_RuntimeData* pRuntimeData = new CFXJSE_RuntimeData(pIsolate);
  CFXJSE_ScopeUtil_IsolateHandle scope(pIsolate);
  v8::Local<v8::FunctionTemplate> hFuncTemplate =
      v8::FunctionTemplate::New(pIsolate);
  v8::Local<v8::Context> hContext =
      v8::Context::New(pIsolate, 0, hFuncTemplate->InstanceTemplate());
  hContext->SetSecurityToken(v8::External::New(pIsolate, pIsolate));
  pRuntimeData->m_hRootContextGlobalTemplate.Reset(pIsolate, hFuncTemplate);
  pRuntimeData->m_hRootContext.Reset(pIsolate, hContext);
  return pRuntimeData;
}

CFXJSE_RuntimeData* CFXJSE_RuntimeData::Get(v8::Isolate* pIsolate) {
  FXJS_PerIsolateData::SetUp(pIsolate);
  FXJS_PerIsolateData* pData = FXJS_PerIsolateData::Get(pIsolate);
  if (!pData->m_pFXJSERuntimeData)
    pData->m_pFXJSERuntimeData = CFXJSE_RuntimeData::Create(pIsolate);
  return pData->m_pFXJSERuntimeData;
}

CFXJSE_IsolateTracker* CFXJSE_IsolateTracker::g_pInstance = nullptr;

CFXJSE_IsolateTracker::CFXJSE_IsolateTracker() {}

CFXJSE_IsolateTracker::~CFXJSE_IsolateTracker() {}

void CFXJSE_IsolateTracker::Append(v8::Isolate* pIsolate) {
  m_OwnedIsolates.push_back(pIsolate);
}

void CFXJSE_IsolateTracker::Remove(
    v8::Isolate* pIsolate,
    CFXJSE_IsolateTracker::DisposeCallback lpfnDisposeCallback) {
  auto it = std::find(m_OwnedIsolates.begin(), m_OwnedIsolates.end(), pIsolate);
  bool bFound = it != m_OwnedIsolates.end();
  if (bFound)
    m_OwnedIsolates.erase(it);
  lpfnDisposeCallback(pIsolate, bFound);
}

void CFXJSE_IsolateTracker::RemoveAll(
    CFXJSE_IsolateTracker::DisposeCallback lpfnDisposeCallback) {
  for (v8::Isolate* pIsolate : m_OwnedIsolates)
    lpfnDisposeCallback(pIsolate, true);

  m_OwnedIsolates.clear();
}
