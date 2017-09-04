// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorklet_h
#define AnimationWorklet_h

#include "core/workers/Worklet.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class ThreadedWorkletMessagingProxy;
class WorkletGlobalScopeProxy;

class MODULES_EXPORT AnimationWorklet final : public Worklet {
  WTF_MAKE_NONCOPYABLE(AnimationWorklet);

 public:
  static AnimationWorklet* create(LocalFrame*);
  ~AnimationWorklet() override;

  void initialize() final;
  bool isInitialized() const final;

  WorkletGlobalScopeProxy* workletGlobalScopeProxy() const final;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit AnimationWorklet(LocalFrame*);

  // The proxy outlives the worklet as it is used to perform thread shutdown,
  // it deletes itself once this has occured.
  ThreadedWorkletMessagingProxy* m_workletMessagingProxy;
};

}  // namespace blink

#endif  // AnimationWorklet_h
