// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy.h"

#include "base/message_loop/message_loop_proxy.h"
#include "base/single_thread_task_runner.h"

namespace cc {

base::SingleThreadTaskRunner* Proxy::MainThreadTaskRunner() const {
  return main_task_runner_.get();
}

bool Proxy::HasImplThread() const { return !!impl_task_runner_.get(); }

base::SingleThreadTaskRunner* Proxy::ImplThreadTaskRunner() const {
  return impl_task_runner_.get();
}

bool Proxy::IsMainThread() const {
#if DCHECK_IS_ON
  if (impl_thread_is_overridden_)
    return false;

  bool is_main_thread = base::PlatformThread::CurrentId() == main_thread_id_;
  if (is_main_thread && main_task_runner_.get()) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
  }
  return is_main_thread;
#else
  return true;
#endif
}

bool Proxy::IsImplThread() const {
#if DCHECK_IS_ON
  if (impl_thread_is_overridden_)
    return true;
  if (!impl_task_runner_.get())
    return false;
  return impl_task_runner_->BelongsToCurrentThread();
#else
  return true;
#endif
}

#if DCHECK_IS_ON
void Proxy::SetCurrentThreadIsImplThread(bool is_impl_thread) {
  impl_thread_is_overridden_ = is_impl_thread;
}
#endif

bool Proxy::IsMainThreadBlocked() const {
#if DCHECK_IS_ON
  return is_main_thread_blocked_;
#else
  return true;
#endif
}

#if DCHECK_IS_ON
void Proxy::SetMainThreadBlocked(bool is_main_thread_blocked) {
  is_main_thread_blocked_ = is_main_thread_blocked;
}
#endif

Proxy::Proxy(scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner)
    : main_task_runner_(base::MessageLoopProxy::current()),
#if !DCHECK_IS_ON
      impl_task_runner_(impl_task_runner) {
#else
      impl_task_runner_(impl_task_runner),
      main_thread_id_(base::PlatformThread::CurrentId()),
      impl_thread_is_overridden_(false),
      is_main_thread_blocked_(false) {
#endif
}

Proxy::~Proxy() {
  DCHECK(IsMainThread());
}

scoped_ptr<base::Value> Proxy::SchedulerAsValueForTesting() {
  return make_scoped_ptr(base::Value::CreateNullValue());
}

}  // namespace cc
