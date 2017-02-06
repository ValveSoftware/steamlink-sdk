// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_SCOPED_RESULT_CALLBACK_H_
#define MEDIA_CAPTURE_VIDEO_SCOPED_RESULT_CALLBACK_H_

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/macros.h"

namespace media {

// This class guarantees that |callback_| has either been called or will pass it
// to |on_error_callback_| on destruction. Inspired by ScopedWebCallbacks<>.
template <typename CallbackType>
class ScopedResultCallback {
 public:
  using OnErrorCallback = base::Callback<void(const CallbackType&)>;
  ScopedResultCallback(const CallbackType& callback,
                       const OnErrorCallback& on_error_callback)
      : callback_(callback), on_error_callback_(on_error_callback) {}

  ~ScopedResultCallback() {
    if (!callback_.is_null())
      on_error_callback_.Run(callback_);
  }

  ScopedResultCallback(ScopedResultCallback&& other) {
    *this = std::move(other);
  }

  ScopedResultCallback& operator=(ScopedResultCallback&& other) {
    callback_ = other.callback_;
    other.callback_.Reset();
    on_error_callback_ = other.on_error_callback_;
    other.on_error_callback_.Reset();
    return *this;
  }

  template <typename... Args>
  void Run(Args... args) {
    on_error_callback_.Reset();
    base::ResetAndReturn(&callback_).Run(std::forward<Args>(args)...);
  }

 private:
  CallbackType callback_;
  OnErrorCallback on_error_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedResultCallback);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_SCOPED_RESULT_CALLBACK_H_
