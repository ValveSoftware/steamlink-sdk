// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundTaskRunner_h
#define BackgroundTaskRunner_h

#include "platform/PlatformExport.h"
#include "wtf/Functional.h"

namespace blink {

class WebTraceLocation;

namespace BackgroundTaskRunner {

enum TaskSize {
    TaskSizeShortRunningTask,
    TaskSizeLongRunningTask,
};

PLATFORM_EXPORT void postOnBackgroundThread(const WebTraceLocation&, std::unique_ptr<CrossThreadClosure>, TaskSize);

} // BackgroundTaskRunner

} // namespace blink

#endif
