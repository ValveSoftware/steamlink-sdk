// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser_tests/waitable_content_pump.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"

namespace blimp {

WaitableContentPump::WaitableContentPump()
    : event_(base::WaitableEvent::ResetPolicy::MANUAL,
             base::WaitableEvent::InitialState::NOT_SIGNALED) {}

WaitableContentPump::~WaitableContentPump() = default;

// static
void WaitableContentPump::WaitFor(base::WaitableEvent* event) {
  while (!event->IsSignaled()) {
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
    content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
  }
  event->Reset();
}

void WaitableContentPump::Wait() {
  WaitableContentPump::WaitFor(&event_);
}

}  // namespace blimp
