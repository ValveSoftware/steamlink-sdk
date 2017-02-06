// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_SINGLE_RELEASE_CALLBACK_H_
#define CC_RESOURCES_SINGLE_RELEASE_CALLBACK_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "cc/base/cc_export.h"
#include "cc/resources/release_callback.h"

namespace cc {

class CC_EXPORT SingleReleaseCallback {
 public:
  static std::unique_ptr<SingleReleaseCallback> Create(
      const ReleaseCallback& cb) {
    return base::WrapUnique(new SingleReleaseCallback(cb));
  }

  ~SingleReleaseCallback();

  void Run(const gpu::SyncToken& sync_token, bool is_lost);

 private:
  explicit SingleReleaseCallback(const ReleaseCallback& callback);

  ReleaseCallback callback_;
};

}  // namespace cc

#endif  // CC_RESOURCES_SINGLE_RELEASE_CALLBACK_H_
