// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletScriptLoader_h
#define WorkletScriptLoader_h

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/fetch/ResourceClient.h"
#include "core/fetch/ResourceOwner.h"
#include "core/loader/resource/ScriptResource.h"

namespace blink {

class Worklet;

// Class that is responsible for processing {@code resource} that is associated
// with worklet's import promise.
class WorkletScriptLoader final
    : public GarbageCollectedFinalized<WorkletScriptLoader>,
      public ResourceOwner<ScriptResource, ScriptResourceClient> {
  USING_GARBAGE_COLLECTED_MIXIN(WorkletScriptLoader);
  WTF_MAKE_NONCOPYABLE(WorkletScriptLoader);

 public:
  static WorkletScriptLoader* create(
      ScriptPromiseResolver* scriptPromiseResolver,
      Worklet* worklet,
      ScriptResource* resource) {
    return new WorkletScriptLoader(scriptPromiseResolver, worklet, resource);
  }

  ~WorkletScriptLoader() override = default;

  // Cancels loading of {@code m_resource}.
  //
  // Typically it gets called when WorkletScriptLoader's host is about to be
  // disposed off.
  void cancel();

  DECLARE_TRACE();

 private:
  // Default constructor.
  //
  // @param resolver Promise resolver that is used to reject/resolve the promise
  // on ScriptResourceClient::notifyFinished event.
  // @param host Host that needs be notified when the resource is downloaded.
  // @param resource that needs to be downloaded.
  WorkletScriptLoader(ScriptPromiseResolver*, Worklet* host, ScriptResource*);

  // ResourceClient
  void notifyFinished(Resource*) final;
  String debugName() const final { return "WorkletLoader"; }

  Member<ScriptPromiseResolver> m_resolver;
  Member<Worklet> m_host;
};

}  // namespace blink

#endif  // WorkletScriptLoader_h
