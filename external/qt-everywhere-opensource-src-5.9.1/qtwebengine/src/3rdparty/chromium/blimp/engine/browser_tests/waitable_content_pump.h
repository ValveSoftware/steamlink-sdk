// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_BROWSER_TESTS_WAITABLE_CONTENT_PUMP_H_
#define BLIMP_ENGINE_BROWSER_TESTS_WAITABLE_CONTENT_PUMP_H_

#include "base/macros.h"
#include "base/synchronization/waitable_event.h"

namespace blimp {

// A helper class to suspend execution on the current stack and still allow all
// content message loops to run until a WaitableEvent is signaled.  This
// currently only pumps the UI, IO, and FILE threads.  The typical use case
// would look like:
//
// WaitableContentPump waiter;
// GiveEventToSomeoneToEventuallySignal(waiter.event());
// waiter.Wait();
class WaitableContentPump {
 public:
  WaitableContentPump();
  virtual ~WaitableContentPump();

  // Pumps content message loops until |event| is signaled.  This will reset
  // |event| afterwards.
  static void WaitFor(base::WaitableEvent* event);

  // Helper methods to use the WaitableContentPump without needing to build a
  // WaitableEvent.
  base::WaitableEvent* event() { return &event_; }
  void Wait();

 private:
  base::WaitableEvent event_;

  DISALLOW_COPY_AND_ASSIGN(WaitableContentPump);
};

}  // namespace blimp

#endif  // BLIMP_ENGINE_BROWSER_TESTS_WAITABLE_CONTENT_PUMP_H_
