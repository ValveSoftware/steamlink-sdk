// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"

#include "base/synchronization/waitable_event.h"

namespace ui {

namespace {

void OnRunPostedTaskAndSignal(const base::Closure& callback,
                              base::WaitableEvent* wait) {
  callback.Run();
  wait->Signal();
}

}  // namespace

void PostSyncTask(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::Closure& callback) {
  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool success = task_runner->PostTask(
      FROM_HERE, base::Bind(OnRunPostedTaskAndSignal, callback, &wait));
  if (success)
    wait.Wait();
}

}  // namespace ui
