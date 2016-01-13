// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_SINGLE_RELEASE_CALLBACK_H_
#define CC_RESOURCES_SINGLE_RELEASE_CALLBACK_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/resources/release_callback.h"

namespace cc {

class CC_EXPORT SingleReleaseCallback {
 public:
  static scoped_ptr<SingleReleaseCallback> Create(const ReleaseCallback& cb) {
    return make_scoped_ptr(new SingleReleaseCallback(cb));
  }

  ~SingleReleaseCallback();

  void Run(uint32 sync_point, bool is_lost);

 private:
  explicit SingleReleaseCallback(const ReleaseCallback& callback);

  bool has_been_run_;
  ReleaseCallback callback_;
};

}  // namespace cc

#endif  // CC_RESOURCES_SINGLE_RELEASE_CALLBACK_H_
