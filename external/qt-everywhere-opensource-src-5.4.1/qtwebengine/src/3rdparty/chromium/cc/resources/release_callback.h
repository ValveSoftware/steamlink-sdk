// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RELEASE_CALLBACK_H_
#define CC_RESOURCES_RELEASE_CALLBACK_H_

#include "base/callback.h"

namespace cc {

typedef base::Callback<void(uint32 sync_point, bool is_lost)> ReleaseCallback;

}  // namespace cc

#endif  // CC_RESOURCES_RELEASE_CALLBACK_H_
