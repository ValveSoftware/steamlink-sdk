// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/common.h"

namespace blimp {

const size_t kMaxPacketPayloadSizeBytes = 20 * 1024 * 1024;  // 20MB
const size_t kPacketHeaderSizeBytes = 4;

}  // namespace blimp
