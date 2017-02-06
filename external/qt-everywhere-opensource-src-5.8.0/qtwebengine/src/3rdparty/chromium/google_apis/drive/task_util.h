// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_TASK_UTIL_H_
#define GOOGLE_APIS_DRIVE_TASK_UTIL_H_

#include <memory>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace google_apis {

// Runs the task with the task runner.
void RunTaskWithTaskRunner(scoped_refptr<base::TaskRunner> task_runner,
                           const base::Closure& task);

namespace internal {

// Implementation of the composed callback, whose signature is |Sig|.
template<typename Sig> struct ComposedCallback;

// ComposedCallback with no argument.
template<>
struct ComposedCallback<void()> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Closure& callback) {
    runner.Run(callback);
  }
};

// ComposedCallback with one argument.
template<typename T1>
struct ComposedCallback<void(T1)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(T1)>& callback,
      T1 arg1) {
    runner.Run(base::Bind(callback, arg1));
  }
};

// ComposedCallback with two arguments.
template<typename T1, typename T2>
struct ComposedCallback<void(T1, T2)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(T1, T2)>& callback,
      T1 arg1, T2 arg2) {
    runner.Run(base::Bind(callback, arg1, arg2));
  }
};

// ComposedCallback with two arguments, and the last one is scoped_ptr.
template <typename T1, typename T2, typename D2>
struct ComposedCallback<void(T1, std::unique_ptr<T2, D2>)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(T1, std::unique_ptr<T2, D2>)>& callback,
      T1 arg1,
      std::unique_ptr<T2, D2> arg2) {
    runner.Run(base::Bind(callback, arg1, base::Passed(&arg2)));
  }
};

// ComposedCallback with three arguments.
template<typename T1, typename T2, typename T3>
struct ComposedCallback<void(T1, T2, T3)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(T1, T2, T3)>& callback,
      T1 arg1, T2 arg2, T3 arg3) {
    runner.Run(base::Bind(callback, arg1, arg2, arg3));
  }
};

// ComposedCallback with three arguments, and the last one is scoped_ptr.
template <typename T1, typename T2, typename T3, typename D3>
struct ComposedCallback<void(T1, T2, std::unique_ptr<T3, D3>)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(T1, T2, std::unique_ptr<T3, D3>)>& callback,
      T1 arg1,
      T2 arg2,
      std::unique_ptr<T3, D3> arg3) {
    runner.Run(base::Bind(callback, arg1, arg2, base::Passed(&arg3)));
  }
};

// ComposedCallback with four arguments.
template<typename T1, typename T2, typename T3, typename T4>
struct ComposedCallback<void(T1, T2, T3, T4)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(T1, T2, T3, T4)>& callback,
      T1 arg1, T2 arg2, T3 arg3, T4 arg4) {
    runner.Run(base::Bind(callback, arg1, arg2, arg3, arg4));
  }
};

}  // namespace internal

// Returns callback that takes arguments (arg1, arg2, ...), create a closure
// by binding them to |callback|, and runs |runner| with the closure.
// I.e. the returned callback works as follows:
//   runner.Run(Bind(callback, arg1, arg2, ...))
template<typename CallbackType>
CallbackType CreateComposedCallback(
    const base::Callback<void(const base::Closure&)>& runner,
    const CallbackType& callback) {
  DCHECK(!runner.is_null());
  DCHECK(!callback.is_null());
  return base::Bind(
      &internal::ComposedCallback<typename CallbackType::RunType>::Run,
      runner, callback);
}

// Returns callback which runs the given |callback| on the current thread.
template<typename CallbackType>
CallbackType CreateRelayCallback(const CallbackType& callback) {
  return CreateComposedCallback(
      base::Bind(&RunTaskWithTaskRunner, base::ThreadTaskRunnerHandle::Get()),
      callback);
}

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_TASK_UTIL_H_
