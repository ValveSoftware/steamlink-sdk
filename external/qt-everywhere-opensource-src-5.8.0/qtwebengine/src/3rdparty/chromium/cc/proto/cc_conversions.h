// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PROTO_CC_CONVERSIONS_H_
#define CC_PROTO_CC_CONVERSIONS_H_

#include "cc/base/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/proto/layer.pb.h"

namespace cc {
class Region;

namespace proto {
class Region;
}  // namespace proto

// TODO(dtrainor): Move these to a class and make them static
// (crbug.com/548432).
CC_EXPORT void RegionToProto(const Region& region, proto::Region* proto);
CC_EXPORT Region RegionFromProto(const proto::Region& proto);

// Conversion methods for ScrollbarOrientation.
CC_EXPORT proto::SolidColorScrollbarLayerProperties::ScrollbarOrientation
ScrollbarOrientationToProto(const ScrollbarOrientation& orientation);
CC_EXPORT ScrollbarOrientation ScrollbarOrientationFromProto(
    const proto::SolidColorScrollbarLayerProperties::ScrollbarOrientation&
        proto);

}  // namespace cc

#endif  // CC_PROTO_CC_CONVERSIONS_H_
