// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_SHUTDOWN_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_SHUTDOWN_H_

#include "content/common/content_export.h"

namespace content {

// This can be used for as-fast-as-possible shutdown, in cases where
// time for shutdown is limited and we just need to write out as much
// data as possible before our time runs out.
//
// This causes the shutdown sequence embodied by
// BrowserMainParts::PostMainMessageLoopRun through
// BrowserMainParts::PostDestroyThreads to occur, i.e. we pretend the
// message loop finished, all threads are stopped in sequence and then
// PostDestroyThreads is called.
//
// As this violates the normal order of shutdown, likely leaving the
// process in a bad state, the last thing this function does is
// terminate the process (right after calling
// BrowserMainParts::PostDestroyThreads).
CONTENT_EXPORT void ImmediateShutdownAndExitProcess();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_SHUTDOWN_H_
