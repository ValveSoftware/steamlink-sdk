// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RETURN_CALLBACK_H_
#define CC_RESOURCES_RETURN_CALLBACK_H_

#include "base/callback.h"
#include "cc/resources/returned_resource.h"

namespace cc {

typedef base::Callback<void(const ReturnedResourceArray&)> ReturnCallback;

}  // namespace cc

#endif  // CC_RESOURCES_RETURN_CALLBACK_H_
