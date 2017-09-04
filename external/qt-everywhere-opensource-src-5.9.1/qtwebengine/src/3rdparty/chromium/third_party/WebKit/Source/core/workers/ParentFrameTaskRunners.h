// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ParentFrameTaskRunners_h
#define ParentFrameTaskRunners_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/TaskRunnerHelper.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class LocalFrame;
class WebTaskRunner;

// Represents a set of task runners of the parent (or associated) document's
// frame. This could be accessed from worker thread(s) and must be initialized
// on the parent context thread (i.e. MainThread) on construction time, rather
// than being done lazily.
//
// This observes LocalFrame lifecycle only for in-process worker cases (i.e.
// only when a non-null LocalFrame is given).
class CORE_EXPORT ParentFrameTaskRunners final
    : public GarbageCollectedFinalized<ParentFrameTaskRunners>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ParentFrameTaskRunners);
  WTF_MAKE_NONCOPYABLE(ParentFrameTaskRunners);

 public:
  static ParentFrameTaskRunners* create(LocalFrame* frame) {
    return new ParentFrameTaskRunners(frame);
  }

  // Might return nullptr for unsupported task types.
  WebTaskRunner* get(TaskType);

  DECLARE_VIRTUAL_TRACE();

 private:
  using TaskRunnerHashMap =
      HashMap<TaskType, WebTaskRunner*, WTF::IntHash<TaskType>, TaskTypeTraits>;

  // LocalFrame could be nullptr if the worker is not associated with a
  // particular local frame.
  explicit ParentFrameTaskRunners(LocalFrame*);

  void contextDestroyed() override;

  Mutex m_taskRunnersMutex;
  TaskRunnerHashMap m_taskRunners;
};

}  // namespace blink

#endif  // ParentFrameTaskRunners_h
