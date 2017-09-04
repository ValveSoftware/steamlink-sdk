// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeathAwareScriptWrappable_h
#define DeathAwareScriptWrappable_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/TraceWrapperMember.h"
#include "platform/heap/Heap.h"
#include "wtf/text/WTFString.h"
#include <signal.h>

namespace blink {

class DeathAwareScriptWrappable
    : public GarbageCollectedFinalized<DeathAwareScriptWrappable>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  static DeathAwareScriptWrappable* s_instance;
  static bool s_hasDied;

 public:
  virtual ~DeathAwareScriptWrappable() {
    if (this == s_instance) {
      s_hasDied = true;
    }
  }

  static DeathAwareScriptWrappable* create() {
    return new DeathAwareScriptWrappable();
  }

  static bool hasDied() { return s_hasDied; }
  static void observeDeathsOf(DeathAwareScriptWrappable* instance) {
    s_hasDied = false;
    s_instance = instance;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_rawDependency);
    visitor->trace(m_wrappedDependency);
    visitor->trace(m_wrappedVectorDependency);
    visitor->trace(m_wrappedHashMapDependency);
  }

  DEFINE_INLINE_VIRTUAL_TRACE_WRAPPERS() {
    visitor->traceWrappersWithManualWriteBarrier(m_rawDependency);
    visitor->traceWrappers(m_wrappedDependency);
    for (auto dep : m_wrappedVectorDependency) {
      visitor->traceWrappers(dep);
    }
    for (auto pair : m_wrappedHashMapDependency) {
      visitor->traceWrappers(pair.key);
      visitor->traceWrappers(pair.value);
    }
  }

  void setRawDependency(DeathAwareScriptWrappable* dependency) {
    ScriptWrappableVisitor::writeBarrier(this, dependency);
    m_rawDependency = dependency;
  }

  void setWrappedDependency(DeathAwareScriptWrappable* dependency) {
    m_wrappedDependency = dependency;
  }

  void addWrappedVectorDependency(DeathAwareScriptWrappable* dependency) {
    m_wrappedVectorDependency.append(Wrapper(this, dependency));
  }

  void addWrappedHashMapDependency(DeathAwareScriptWrappable* key,
                                   DeathAwareScriptWrappable* value) {
    m_wrappedHashMapDependency.add(Wrapper(this, key), Wrapper(this, value));
  }

 private:
  typedef TraceWrapperMember<DeathAwareScriptWrappable> Wrapper;
  DeathAwareScriptWrappable() : m_wrappedDependency(this, nullptr) {}

  Member<DeathAwareScriptWrappable> m_rawDependency;
  Wrapper m_wrappedDependency;
  HeapVector<Wrapper> m_wrappedVectorDependency;
  HeapHashMap<Wrapper, Wrapper> m_wrappedHashMapDependency;
};

}  // namespace blink

#endif  // DeathAwareScriptWrappable_h
