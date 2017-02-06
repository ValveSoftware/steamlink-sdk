// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/proto/skia_conversions.h"

#include "base/logging.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/skrrect.pb.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

void SkPointToProto(const SkPoint& point, proto::PointF* proto) {
  PointFToProto(gfx::PointF(point.x(), point.y()), proto);
}

SkPoint ProtoToSkPoint(const proto::PointF& proto) {
  gfx::PointF point = ProtoToPointF(proto);
  return SkPoint::Make(point.x(), point.y());
}

}  // namespace

SkRegion::Op SkRegionOpFromProto(proto::SkRegion::Op op) {
  switch (op) {
    case proto::SkRegion::DIFFERENCE_:
      return SkRegion::Op::kDifference_Op;
    case proto::SkRegion::INTERSECT:
      return SkRegion::Op::kIntersect_Op;
    case proto::SkRegion::UNION:
      return SkRegion::Op::kUnion_Op;
    case proto::SkRegion::XOR:
      return SkRegion::Op::kXOR_Op;
    case proto::SkRegion::REVERSE_DIFFERENCE:
      return SkRegion::Op::kReverseDifference_Op;
    case proto::SkRegion::REPLACE:
      return SkRegion::Op::kReplace_Op;
  }
  return SkRegion::Op::kDifference_Op;
}

proto::SkRegion::Op SkRegionOpToProto(SkRegion::Op op) {
  switch (op) {
    case SkRegion::Op::kDifference_Op:
      return proto::SkRegion::DIFFERENCE_;
    case SkRegion::Op::kIntersect_Op:
      return proto::SkRegion::INTERSECT;
    case SkRegion::Op::kUnion_Op:
      return proto::SkRegion::UNION;
    case SkRegion::Op::kXOR_Op:
      return proto::SkRegion::XOR;
    case SkRegion::Op::kReverseDifference_Op:
      return proto::SkRegion::REVERSE_DIFFERENCE;
    case SkRegion::Op::kReplace_Op:
      return proto::SkRegion::REPLACE;
  }
  return proto::SkRegion::DIFFERENCE_;
}

SkXfermode::Mode SkXfermodeModeFromProto(proto::SkXfermode::Mode mode) {
  switch (mode) {
    case proto::SkXfermode::CLEAR_:
      return SkXfermode::Mode::kClear_Mode;
    case proto::SkXfermode::SRC:
      return SkXfermode::Mode::kSrc_Mode;
    case proto::SkXfermode::DST:
      return SkXfermode::Mode::kDst_Mode;
    case proto::SkXfermode::SRC_OVER:
      return SkXfermode::Mode::kSrcOver_Mode;
    case proto::SkXfermode::DST_OVER:
      return SkXfermode::Mode::kDstOver_Mode;
    case proto::SkXfermode::SRC_IN:
      return SkXfermode::Mode::kSrcIn_Mode;
    case proto::SkXfermode::DST_IN:
      return SkXfermode::Mode::kDstIn_Mode;
    case proto::SkXfermode::SRC_OUT:
      return SkXfermode::Mode::kSrcOut_Mode;
    case proto::SkXfermode::DST_OUT:
      return SkXfermode::Mode::kDstOut_Mode;
    case proto::SkXfermode::SRC_A_TOP:
      return SkXfermode::Mode::kSrcATop_Mode;
    case proto::SkXfermode::DST_A_TOP:
      return SkXfermode::Mode::kDstATop_Mode;
    case proto::SkXfermode::XOR:
      return SkXfermode::Mode::kXor_Mode;
    case proto::SkXfermode::PLUS:
      return SkXfermode::Mode::kPlus_Mode;
    case proto::SkXfermode::MODULATE:
      return SkXfermode::Mode::kModulate_Mode;
    case proto::SkXfermode::SCREEN:
      return SkXfermode::Mode::kScreen_Mode;
    case proto::SkXfermode::OVERLAY:
      return SkXfermode::Mode::kOverlay_Mode;
    case proto::SkXfermode::DARKEN:
      return SkXfermode::Mode::kDarken_Mode;
    case proto::SkXfermode::LIGHTEN:
      return SkXfermode::Mode::kLighten_Mode;
    case proto::SkXfermode::COLOR_DODGE:
      return SkXfermode::Mode::kColorDodge_Mode;
    case proto::SkXfermode::COLOR_BURN:
      return SkXfermode::Mode::kColorBurn_Mode;
    case proto::SkXfermode::HARD_LIGHT:
      return SkXfermode::Mode::kHardLight_Mode;
    case proto::SkXfermode::SOFT_LIGHT:
      return SkXfermode::Mode::kSoftLight_Mode;
    case proto::SkXfermode::DIFFERENCE_:
      return SkXfermode::Mode::kDifference_Mode;
    case proto::SkXfermode::EXCLUSION:
      return SkXfermode::Mode::kExclusion_Mode;
    case proto::SkXfermode::MULTIPLY:
      return SkXfermode::Mode::kMultiply_Mode;
    case proto::SkXfermode::HUE:
      return SkXfermode::Mode::kHue_Mode;
    case proto::SkXfermode::SATURATION:
      return SkXfermode::Mode::kSaturation_Mode;
    case proto::SkXfermode::COLOR:
      return SkXfermode::Mode::kColor_Mode;
    case proto::SkXfermode::LUMINOSITY:
      return SkXfermode::Mode::kLuminosity_Mode;
  }
  return SkXfermode::Mode::kClear_Mode;
}

proto::SkXfermode::Mode SkXfermodeModeToProto(SkXfermode::Mode mode) {
  switch (mode) {
    case SkXfermode::Mode::kClear_Mode:
      return proto::SkXfermode::CLEAR_;
    case SkXfermode::Mode::kSrc_Mode:
      return proto::SkXfermode::SRC;
    case SkXfermode::Mode::kDst_Mode:
      return proto::SkXfermode::DST;
    case SkXfermode::Mode::kSrcOver_Mode:
      return proto::SkXfermode::SRC_OVER;
    case SkXfermode::Mode::kDstOver_Mode:
      return proto::SkXfermode::DST_OVER;
    case SkXfermode::Mode::kSrcIn_Mode:
      return proto::SkXfermode::SRC_IN;
    case SkXfermode::Mode::kDstIn_Mode:
      return proto::SkXfermode::DST_IN;
    case SkXfermode::Mode::kSrcOut_Mode:
      return proto::SkXfermode::SRC_OUT;
    case SkXfermode::Mode::kDstOut_Mode:
      return proto::SkXfermode::DST_OUT;
    case SkXfermode::Mode::kSrcATop_Mode:
      return proto::SkXfermode::SRC_A_TOP;
    case SkXfermode::Mode::kDstATop_Mode:
      return proto::SkXfermode::DST_A_TOP;
    case SkXfermode::Mode::kXor_Mode:
      return proto::SkXfermode::XOR;
    case SkXfermode::Mode::kPlus_Mode:
      return proto::SkXfermode::PLUS;
    case SkXfermode::Mode::kModulate_Mode:
      return proto::SkXfermode::MODULATE;
    case SkXfermode::Mode::kScreen_Mode:
      return proto::SkXfermode::SCREEN;
    case SkXfermode::Mode::kOverlay_Mode:
      return proto::SkXfermode::OVERLAY;
    case SkXfermode::Mode::kDarken_Mode:
      return proto::SkXfermode::DARKEN;
    case SkXfermode::Mode::kLighten_Mode:
      return proto::SkXfermode::LIGHTEN;
    case SkXfermode::Mode::kColorDodge_Mode:
      return proto::SkXfermode::COLOR_DODGE;
    case SkXfermode::Mode::kColorBurn_Mode:
      return proto::SkXfermode::COLOR_BURN;
    case SkXfermode::Mode::kHardLight_Mode:
      return proto::SkXfermode::HARD_LIGHT;
    case SkXfermode::Mode::kSoftLight_Mode:
      return proto::SkXfermode::SOFT_LIGHT;
    case SkXfermode::Mode::kDifference_Mode:
      return proto::SkXfermode::DIFFERENCE_;
    case SkXfermode::Mode::kExclusion_Mode:
      return proto::SkXfermode::EXCLUSION;
    case SkXfermode::Mode::kMultiply_Mode:
      return proto::SkXfermode::MULTIPLY;
    case SkXfermode::Mode::kHue_Mode:
      return proto::SkXfermode::HUE;
    case SkXfermode::Mode::kSaturation_Mode:
      return proto::SkXfermode::SATURATION;
    case SkXfermode::Mode::kColor_Mode:
      return proto::SkXfermode::COLOR;
    case SkXfermode::Mode::kLuminosity_Mode:
      return proto::SkXfermode::LUMINOSITY;
  }
  return proto::SkXfermode::CLEAR_;
}

void SkRRectToProto(const SkRRect& rect, proto::SkRRect* proto) {
  RectFToProto(gfx::SkRectToRectF(rect.rect()), proto->mutable_rect());

  SkPointToProto(rect.radii(SkRRect::kUpperLeft_Corner),
                 proto->mutable_radii_upper_left());
  SkPointToProto(rect.radii(SkRRect::kUpperRight_Corner),
                 proto->mutable_radii_upper_right());
  SkPointToProto(rect.radii(SkRRect::kLowerRight_Corner),
                 proto->mutable_radii_lower_right());
  SkPointToProto(rect.radii(SkRRect::kLowerLeft_Corner),
                 proto->mutable_radii_lower_left());
}

SkRRect ProtoToSkRRect(const proto::SkRRect& proto) {
  SkRect parsed_rect = gfx::RectFToSkRect(ProtoToRectF(proto.rect()));
  SkVector parsed_radii[4];
  parsed_radii[SkRRect::kUpperLeft_Corner] =
      ProtoToSkPoint(proto.radii_upper_left());
  parsed_radii[SkRRect::kUpperRight_Corner] =
      ProtoToSkPoint(proto.radii_upper_right());
  parsed_radii[SkRRect::kLowerRight_Corner] =
      ProtoToSkPoint(proto.radii_lower_right());
  parsed_radii[SkRRect::kLowerLeft_Corner] =
      ProtoToSkPoint(proto.radii_lower_left());
  SkRRect rect;
  rect.setRectRadii(parsed_rect, parsed_radii);
  return rect;
}

}  // namespace cc
