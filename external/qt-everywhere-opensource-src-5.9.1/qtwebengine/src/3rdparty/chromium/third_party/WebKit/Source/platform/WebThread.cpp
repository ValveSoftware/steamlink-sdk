// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebThread.h"

#include "platform/WebTaskRunner.h"
#include "wtf/Assertions.h"

#if OS(WIN)
#include <windows.h>
#elif OS(POSIX)
#include <unistd.h>
#endif

namespace blink {

#if OS(WIN)
static_assert(sizeof(blink::PlatformThreadId) >= sizeof(DWORD),
              "size of platform thread id is too small");
#elif OS(POSIX)
static_assert(sizeof(blink::PlatformThreadId) >= sizeof(pid_t),
              "size of platform thread id is too small");
#else
#error Unexpected platform
#endif

base::SingleThreadTaskRunner* WebThread::getSingleThreadTaskRunner() {
  return getWebTaskRunner()->toSingleThreadTaskRunner();
}

}  // namespace blink
