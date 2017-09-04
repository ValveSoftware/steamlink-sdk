// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_MANDATORY_CALLBACK_H_
#define BLIMP_COMMON_MANDATORY_CALLBACK_H_

#include <utility>

#include "base/callback.h"
#include "base/logging.h"

namespace blimp {

// Adds a leak-detection wrapper to base::Callback objects.
//
// When a wrapped Callback is destroyed without being Run first, the checker
// will DCHECK-crash the program with a stack trace, so that developers can
// pinpoint the site of the leak.
//
// The MandatoryCallback may be passed using std::move; leak detection occurs
// when the MandatoryCallback is finally destroyed.
//
// Usage:
//   void DoStuff(int d) { ... }
//   ...
//   base::Closure my_callback = base::Bind(&DoStuff);
//   MandatoryCallback<void(int)> leak_checked_callback =
//       CreateMandatoryCallback(my_callback);
//
// Note that MandatoryCallbacks lack some functionality from
// base::CallbackInternal.
// * They can only be invoked once.
// * They do not support parameter currying with additional Bind() calls.[1]
// * They cannot be Reset().
//
// [1] Mandatory invocation support would need to be plumbed in to
//     base::CallbackInternal and base::Bind for this to work,
//     which is feasible but low-priority for the owners of the callback
//     libraries.

template <typename SignatureType>
class MandatoryCallback;

// Template specialization for extracting the function signature data types.
template <typename ReturnType, typename... ArgTypes>
class MandatoryCallback<ReturnType(ArgTypes...)> {
 public:
  using CallbackType = base::Callback<ReturnType(ArgTypes...)>;

  explicit MandatoryCallback(const CallbackType& callback) : cb_(callback) {
    DCHECK(!cb_.is_null());
  }

  MandatoryCallback(MandatoryCallback&& other) {
    cb_ = other.cb_;
    other.cb_.Reset();

#if DCHECK_IS_ON()
    was_run_ = other.was_run_;
    other.was_run_ = false;
#endif
  }

  ~MandatoryCallback() {
#if DCHECK_IS_ON()
    DCHECK(cb_.is_null() || was_run_);
#endif
  }

  // This a overload that handles the case where there are no arguments provided
  template <typename...>
  ReturnType Run() {
    DCHECK(cb_);  // Can't be run following std::move.

#if DCHECK_IS_ON()
    DCHECK(!was_run_);
    was_run_ = true;
#endif

    cb_.Run();
  }

  template <typename... RunArgs>
  ReturnType Run(RunArgs... args) {
    DCHECK(cb_);  // Can't be run following std::move.

#if DCHECK_IS_ON()
    DCHECK(!was_run_);
    was_run_ = true;
#endif

    cb_.Run(std::forward<RunArgs...>(args...));
  }

 private:
  // Nulled after being moved.
  CallbackType cb_;

#if DCHECK_IS_ON()
  bool was_run_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(MandatoryCallback);
};

// Creates a leak-checking proxy callback around |callback|.
template <typename SignatureType>
MandatoryCallback<SignatureType> CreateMandatoryCallback(
    const base::Callback<SignatureType>& callback) {
  return MandatoryCallback<SignatureType>(callback);
}

using MandatoryClosure = MandatoryCallback<void()>;

}  // namespace blimp

#endif  // BLIMP_COMMON_MANDATORY_CALLBACK_H_
