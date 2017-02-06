// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/threading/platform_thread.h"
#include "content/public/common/main_function_params.h"

namespace content {

// Mainline routine for running as the download process.
int DownloadMain(const MainFunctionParams& parameters) {
  // The main message loop of the utility process.
  base::MessageLoop main_message_loop;
  base::PlatformThread::SetName("CrDownloadMain");
  base::MessageLoop::current()->Run();

  return 0;
}

}  // namespace content
