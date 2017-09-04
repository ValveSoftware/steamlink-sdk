// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorklet_h
#define AudioWorklet_h

#include "core/workers/Worklet.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class ThreadedWorkletGlobalScopeProxy;
class WorkletGlobalScopeProxy;

class MODULES_EXPORT AudioWorklet final : public Worklet {
  WTF_MAKE_NONCOPYABLE(AudioWorklet);

 public:
  static AudioWorklet* create(LocalFrame*);
  ~AudioWorklet() override;

  WorkletGlobalScopeProxy* workletGlobalScopeProxy() const final;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit AudioWorklet(LocalFrame*);

  // TODO(ikilpatrick): this will change to a raw ptr once we have a thread.
  std::unique_ptr<ThreadedWorkletGlobalScopeProxy> m_workletGlobalScopeProxy;
};

}  // namespace blink

#endif  // AudioWorklet_h
