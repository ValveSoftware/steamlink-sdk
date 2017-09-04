// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ParentFrameTaskRunners.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/ThreadingPrimitives.h"

namespace blink {

ParentFrameTaskRunners::ParentFrameTaskRunners(LocalFrame* frame)
    : ContextLifecycleObserver(frame ? frame->document() : nullptr) {
  if (frame && frame->document())
    DCHECK(frame->document()->isContextThread());

  // For now we only support very limited task types.
  for (auto type :
       {TaskType::Internal, TaskType::Networking, TaskType::PostedMessage,
        TaskType::CanvasBlobSerialization}) {
    m_taskRunners.add(type, TaskRunnerHelper::get(type, frame));
  }
}

WebTaskRunner* ParentFrameTaskRunners::get(TaskType type) {
  MutexLocker lock(m_taskRunnersMutex);
  return m_taskRunners.get(type);
}

DEFINE_TRACE(ParentFrameTaskRunners) {
  ContextLifecycleObserver::trace(visitor);
}

void ParentFrameTaskRunners::contextDestroyed() {
  MutexLocker lock(m_taskRunnersMutex);
  for (auto& entry : m_taskRunners)
    entry.value = Platform::current()->currentThread()->getWebTaskRunner();
}

}  // namespace blink
