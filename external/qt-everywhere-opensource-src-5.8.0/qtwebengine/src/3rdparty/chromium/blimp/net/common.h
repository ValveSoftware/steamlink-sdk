// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_COMMON_H_
#define BLIMP_NET_COMMON_H_

#include <stddef.h>

#include "blimp/net/blimp_net_export.h"

namespace blimp {

// TODO(kmarshall): Apply SCIENCE to determine a better constant here.
// See crbug.com/542464
extern const size_t BLIMP_NET_EXPORT kMaxPacketPayloadSizeBytes;
extern const size_t BLIMP_NET_EXPORT kPacketHeaderSizeBytes;

}  // namespace blimp

#endif  // BLIMP_NET_COMMON_H_
