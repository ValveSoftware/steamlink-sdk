// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PROTO_BASE_CONVERSIONS_H_
#define CC_PROTO_BASE_CONVERSIONS_H_

#include "base/time/time.h"
#include "cc/base/cc_export.h"

namespace cc {

// TODO(dtrainor): Move these to a class and make them static
// (crbug.com/548432).
// We should probably have a better way for sending these.
CC_EXPORT int64_t TimeTicksToProto(base::TimeTicks ticks);
CC_EXPORT base::TimeTicks ProtoToTimeTicks(int64_t ticks);

}  // namespace cc

#endif  // CC_PROTO_BASE_CONVERSIONS_H_
