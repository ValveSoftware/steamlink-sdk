// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJSE_SCOPE_INLINE_H_
#define FXJSE_SCOPE_INLINE_H_

#include "fxjse/context.h"
#include "fxjse/runtime.h"

class CFXJSE_ScopeUtil_IsolateHandle {
 public:
  explicit CFXJSE_ScopeUtil_IsolateHandle(v8::Isolate* pIsolate)
      : m_isolate(pIsolate), m_iscope(pIsolate), m_hscope(pIsolate) {}
  v8::Isolate* GetIsolate() { return m_isolate; }

 private:
  CFXJSE_ScopeUtil_IsolateHandle(const CFXJSE_ScopeUtil_IsolateHandle&) =
      delete;
  void operator=(const CFXJSE_ScopeUtil_IsolateHandle&) = delete;
  void* operator new(size_t size) = delete;
  void operator delete(void*, size_t) = delete;

  v8::Isolate* m_isolate;
  v8::Isolate::Scope m_iscope;
  v8::HandleScope m_hscope;
};

class CFXJSE_ScopeUtil_IsolateHandleRootContext {
 public:
  explicit CFXJSE_ScopeUtil_IsolateHandleRootContext(v8::Isolate* pIsolate)
      : m_parent(pIsolate),
        m_cscope(v8::Local<v8::Context>::New(
            pIsolate,
            CFXJSE_RuntimeData::Get(pIsolate)->m_hRootContext)) {}

 private:
  CFXJSE_ScopeUtil_IsolateHandleRootContext(
      const CFXJSE_ScopeUtil_IsolateHandleRootContext&) = delete;
  void operator=(const CFXJSE_ScopeUtil_IsolateHandleRootContext&) = delete;
  void* operator new(size_t size) = delete;
  void operator delete(void*, size_t) = delete;

  CFXJSE_ScopeUtil_IsolateHandle m_parent;
  v8::Context::Scope m_cscope;
};

class CFXJSE_ScopeUtil_IsolateHandleContext {
 public:
  explicit CFXJSE_ScopeUtil_IsolateHandleContext(CFXJSE_Context* pContext)
      : m_context(pContext),
        m_parent(pContext->m_pIsolate),
        m_cscope(v8::Local<v8::Context>::New(pContext->m_pIsolate,
                                             pContext->m_hContext)) {}
  v8::Isolate* GetIsolate() { return m_context->m_pIsolate; }
  v8::Local<v8::Context> GetLocalContext() {
    return v8::Local<v8::Context>::New(m_context->m_pIsolate,
                                       m_context->m_hContext);
  }

 private:
  CFXJSE_ScopeUtil_IsolateHandleContext(
      const CFXJSE_ScopeUtil_IsolateHandleContext&) = delete;
  void operator=(const CFXJSE_ScopeUtil_IsolateHandleContext&) = delete;
  void* operator new(size_t size) = delete;
  void operator delete(void*, size_t) = delete;

  CFXJSE_Context* m_context;
  CFXJSE_ScopeUtil_IsolateHandle m_parent;
  v8::Context::Scope m_cscope;
};

#endif  // FXJSE_SCOPE_INLINE_H_
