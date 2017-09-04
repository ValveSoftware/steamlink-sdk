// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Worklet_h
#define Worklet_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class ResourceFetcher;
class WorkletGlobalScopeProxy;
class WorkletScriptLoader;

class CORE_EXPORT Worklet : public GarbageCollectedFinalized<Worklet>,
                            public ScriptWrappable,
                            public ActiveDOMObject {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Worklet);
  WTF_MAKE_NONCOPYABLE(Worklet);

 public:
  virtual void initialize() {}
  virtual bool isInitialized() const { return true; }

  virtual WorkletGlobalScopeProxy* workletGlobalScopeProxy() const = 0;

  // Worklet
  ScriptPromise import(ScriptState*, const String& url);

  void notifyFinished(WorkletScriptLoader*);

  // ActiveDOMObject
  void contextDestroyed() final;

  DECLARE_VIRTUAL_TRACE();

 protected:
  // The Worklet inherits the url and userAgent from the frame->document().
  explicit Worklet(LocalFrame*);

 private:
  ResourceFetcher* fetcher() const { return m_fetcher.get(); }

  Member<ResourceFetcher> m_fetcher;
  HeapHashSet<Member<WorkletScriptLoader>> m_scriptLoaders;
};

}  // namespace blink

#endif  // Worklet_h
