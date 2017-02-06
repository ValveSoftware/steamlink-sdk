// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_BIND_TO_TASK_RUNNER_H_
#define CHROMECAST_BASE_BIND_TO_TASK_RUNNER_H_

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

// This is a helper utility for Bind()ing callbacks to a given TaskRunner.
// The typical use is when |a| (of class |A|) wants to hand a callback such as
// base::Bind(&A::AMethod, a) to |b|, but needs to ensure that when |b| executes
// the callback, it does so on a specific TaskRunner (for example, |a|'s current
// MessageLoop).
//
// Typical usage: request to be called back on the current thread:
// other->StartAsyncProcessAndCallMeBack(
//    BindToTaskRunner(my_task_runner_, base::Bind(&MyClass::MyMethod, this)));
//
// Note that like base::Bind(), BindToTaskRunner() can't bind non-constant
// references, and that *unlike* base::Bind(), BindToTaskRunner() makes copies
// of its arguments, and thus can't be used with arrays. Note that the callback
// is always posted to the target TaskRunner.
//
// As a convenience, you can use BindToCurrentThread() to bind to the
// TaskRunner for the current thread (ie, base::ThreadTaskRunnerHandle::Get()).

namespace chromecast {
namespace bind_helpers {

template <typename T>
T& TrampolineForward(T& t) {
  return t;
}

template <typename T, typename R>
base::internal::PassedWrapper<std::unique_ptr<T, R>> TrampolineForward(
    std::unique_ptr<T, R>& p) {
  return base::Passed(&p);
}

template <typename T>
base::internal::PassedWrapper<ScopedVector<T>> TrampolineForward(
    ScopedVector<T>& p) {
  return base::Passed(&p);
}

template <typename Sig>
struct BindToTaskRunnerTrampoline;

template <typename... Args>
struct BindToTaskRunnerTrampoline<void(Args...)> {
  static void Run(const scoped_refptr<base::TaskRunner>& task_runner,
                  const base::Callback<void(Args...)>& cb,
                  Args... args) {
    task_runner->PostTask(FROM_HERE,
                          base::Bind(cb, TrampolineForward(args)...));
  }
};

}  // namespace bind_helpers

template <typename T>
base::Callback<T> BindToTaskRunner(
    const scoped_refptr<base::TaskRunner>& task_runner,
    const base::Callback<T>& cb) {
  return base::Bind(&bind_helpers::BindToTaskRunnerTrampoline<T>::Run,
                    task_runner, cb);
}

template <typename T>
base::Callback<T> BindToCurrentThread(const base::Callback<T>& cb) {
  return BindToTaskRunner(base::ThreadTaskRunnerHandle::Get(), cb);
}

}  // namespace chromecast

#endif  // CHROMECAST_BASE_BIND_TO_TASK_RUNNER_H_
