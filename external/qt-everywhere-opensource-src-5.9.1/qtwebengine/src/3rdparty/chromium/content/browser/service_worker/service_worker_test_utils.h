// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

template <class TestClass>
class MojoServiceWorkerTestP : public TestClass,
                               public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    if (!is_mojo_enabled()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kDisableMojoServiceWorker);
    }
    TestClass::SetUp();
  }

  bool is_mojo_enabled() const { return GetParam(); }
};

template <typename Arg>
void ReceiveResult(BrowserThread::ID run_quit_thread,
                   const base::Closure& quit,
                   Arg* out, Arg actual) {
  *out = actual;
  if (!quit.is_null())
    BrowserThread::PostTask(run_quit_thread, FROM_HERE, quit);
}

template <typename Arg> base::Callback<void(Arg)>
CreateReceiver(BrowserThread::ID run_quit_thread,
               const base::Closure& quit, Arg* out) {
  return base::Bind(&ReceiveResult<Arg>, run_quit_thread, quit, out);
}

template <typename Arg>
base::Callback<void(Arg)> CreateReceiverOnCurrentThread(
    Arg* out,
    const base::Closure& quit = base::Closure()) {
  BrowserThread::ID id;
  bool ret = BrowserThread::GetCurrentThreadIdentifier(&id);
  DCHECK(ret);
  return base::Bind(&ReceiveResult<Arg>, id, quit, out);
}

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
