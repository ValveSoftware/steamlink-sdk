// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_MAIN_H_
#define CONTENT_PUBLIC_APP_CONTENT_MAIN_H_

#include <stddef.h>

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace sandbox {
struct SandboxInterfaceInfo;
}

namespace content {
class ContentMainDelegate;

struct ContentMainParams {
  explicit ContentMainParams(ContentMainDelegate* delegate)
      : delegate(delegate),
#if defined(OS_WIN)
        instance(NULL),
        sandbox_info(NULL),
#elif !defined(OS_ANDROID)
        argc(0),
        argv(NULL),
#endif
        ui_task(NULL),
        setup_signal_handlers(true) {
  }

  ContentMainDelegate* delegate;

#if defined(OS_WIN)
  HINSTANCE instance;

  // |sandbox_info| should be initialized using InitializeSandboxInfo from
  // content_main_win.h
  sandbox::SandboxInterfaceInfo* sandbox_info;
#elif !defined(OS_ANDROID)
  int argc;
  const char** argv;
#endif

  // Used by browser_tests. If non-null BrowserMain schedules this task to run
  // on the MessageLoop. It's owned by the test code.
  base::Closure* ui_task;

  bool setup_signal_handlers;
};

#if defined(OS_ANDROID)
// In the Android, the content main starts from ContentMain.java, This function
// provides a way to set the |delegate| as ContentMainDelegate for
// ContentMainRunner.
// This should only be called once before ContentMainRunner actually running.
// The ownership of |delegate| is transferred.
CONTENT_EXPORT void SetContentMainDelegate(ContentMainDelegate* delegate);
#else
// ContentMain should be called from the embedder's main() function to do the
// initial setup for every process. The embedder has a chance to customize
// startup using the ContentMainDelegate interface. The embedder can also pass
// in NULL for |delegate| if they don't want to override default startup.
CONTENT_EXPORT int ContentMain(const ContentMainParams& params);
#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_MAIN_H_
