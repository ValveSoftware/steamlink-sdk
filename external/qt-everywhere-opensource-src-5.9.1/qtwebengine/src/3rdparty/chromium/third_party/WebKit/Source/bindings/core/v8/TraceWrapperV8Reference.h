// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TraceWrapperV8Reference_h
#define TraceWrapperV8Reference_h

#include "bindings/core/v8/ScriptWrappableVisitor.h"

namespace blink {

/**
 * TraceWrapperV8Reference is used to trace from Blink to V8. If wrapper
 * tracing is disabled, the reference is a weak v8::Persistent. Otherwise,
 * the reference is (strongly) traced by wrapper tracing.
 *
 * TODO(mlippautz): Use a better handle type than v8::Persistent.
 */
template <typename T>
class TraceWrapperV8Reference {
 public:
  explicit TraceWrapperV8Reference(void* parent) : m_parent(parent) {}

  TraceWrapperV8Reference(v8::Isolate* isolate,
                          void* parent,
                          v8::Local<T> handle)
      : m_parent(parent) {
    internalSet(isolate, handle);
    m_handle.SetWeak();
  }

  ~TraceWrapperV8Reference() { clear(); }

  void set(v8::Isolate* isolate, v8::Local<T> handle) {
    internalSet(isolate, handle);
    m_handle.SetWeak();
  }

  template <typename P>
  void set(v8::Isolate* isolate,
           v8::Local<T> handle,
           P* parameters,
           void (*callback)(const v8::WeakCallbackInfo<P>&),
           v8::WeakCallbackType type = v8::WeakCallbackType::kParameter) {
    internalSet(isolate, handle);
    m_handle.SetWeak(parameters, callback, type);
  }

  ALWAYS_INLINE v8::Local<T> newLocal(v8::Isolate* isolate) const {
    return v8::Local<T>::New(isolate, m_handle);
  }

  bool isEmpty() const { return m_handle.IsEmpty(); }
  void clear() { m_handle.Reset(); }
  ALWAYS_INLINE v8::Persistent<T>& get() { return m_handle; }

  void setReference(const v8::Persistent<v8::Object>& parent,
                    v8::Isolate* isolate) {
    DCHECK(!RuntimeEnabledFeatures::traceWrappablesEnabled());
    isolate->SetReference(parent, m_handle);
  }

  template <typename S>
  const TraceWrapperV8Reference<S>& cast() const {
    return reinterpret_cast<const TraceWrapperV8Reference<S>&>(
        const_cast<const TraceWrapperV8Reference<T>&>(*this));
  }

 private:
  inline void internalSet(v8::Isolate* isolate, v8::Local<T> handle) {
    m_handle.Reset(isolate, handle);
    ScriptWrappableVisitor::writeBarrier(m_parent, &cast<v8::Value>());
  }

  v8::Persistent<T> m_handle;
  void* m_parent;
};

}  // namespace blink

#endif  // TraceWrapperV8Reference_h
