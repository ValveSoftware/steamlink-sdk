// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/TaskRunnerHelper.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/platform/WebThread.h"

namespace blink {

WebTaskRunner* TaskRunnerHelper::get(TaskType type, LocalFrame* frame) {
  // TODO(haraken): Optimize the mapping from TaskTypes to task runners.
  switch (type) {
    case TaskType::DOMManipulation:
    case TaskType::UserInteraction:
    case TaskType::HistoryTraversal:
    case TaskType::Embed:
    case TaskType::MediaElementEvent:
    case TaskType::CanvasBlobSerialization:
    case TaskType::RemoteEvent:
    case TaskType::WebSocket:
    case TaskType::Microtask:
    case TaskType::PostedMessage:
    case TaskType::UnshippedPortMessage:
    case TaskType::Timer:
    case TaskType::Internal:
      return frame ? frame->frameScheduler()->timerTaskRunner()
                   : Platform::current()->currentThread()->getWebTaskRunner();
    case TaskType::Networking:
      return frame ? frame->frameScheduler()->loadingTaskRunner()
                   : Platform::current()->currentThread()->getWebTaskRunner();
    case TaskType::Unthrottled:
      return frame ? frame->frameScheduler()->unthrottledTaskRunner()
                   : Platform::current()->currentThread()->getWebTaskRunner();
    default:
      NOTREACHED();
  }
  return nullptr;
}

WebTaskRunner* TaskRunnerHelper::get(TaskType type, Document* document) {
  return get(type, document ? document->frame() : nullptr);
}

WebTaskRunner* TaskRunnerHelper::get(TaskType type,
                                     ExecutionContext* executionContext) {
  return get(type, executionContext && executionContext->isDocument()
                       ? static_cast<Document*>(executionContext)
                       : nullptr);
}

WebTaskRunner* TaskRunnerHelper::get(TaskType type, ScriptState* scriptState) {
  return get(type, scriptState ? scriptState->getExecutionContext() : nullptr);
}

}  // namespace blink
