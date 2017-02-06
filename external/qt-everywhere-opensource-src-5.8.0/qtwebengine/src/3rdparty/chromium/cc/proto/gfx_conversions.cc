// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/proto/gfx_conversions.h"

#include "cc/proto/point.pb.h"
#include "cc/proto/point3f.pb.h"
#include "cc/proto/pointf.pb.h"
#include "cc/proto/rect.pb.h"
#include "cc/proto/rectf.pb.h"
#include "cc/proto/scroll_offset.pb.h"
#include "cc/proto/size.pb.h"
#include "cc/proto/sizef.pb.h"
#include "cc/proto/transform.pb.h"
#include "cc/proto/vector2d.pb.h"
#include "cc/proto/vector2df.pb.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/transform.h"

namespace cc {

void PointToProto(const gfx::Point& point, proto::Point* proto) {
  proto->set_x(point.x());
  proto->set_y(point.y());
}

gfx::Point ProtoToPoint(const proto::Point& proto) {
  return gfx::Point(proto.x(), proto.y());
}

void PointFToProto(const gfx::PointF& point, proto::PointF* proto) {
  proto->set_x(point.x());
  proto->set_y(point.y());
}

gfx::PointF ProtoToPointF(const proto::PointF& proto) {
  return gfx::PointF(proto.x(), proto.y());
}

void Point3FToProto(const gfx::Point3F& point, proto::Point3F* proto) {
  proto->set_x(point.x());
  proto->set_y(point.y());
  proto->set_z(point.z());
}

gfx::Point3F ProtoToPoint3F(const proto::Point3F& proto) {
  return gfx::Point3F(proto.x(), proto.y(), proto.z());
}

void RectToProto(const gfx::Rect& rect, proto::Rect* proto) {
  proto->mutable_origin()->set_x(rect.x());
  proto->mutable_origin()->set_y(rect.y());
  proto->mutable_size()->set_width(rect.width());
  proto->mutable_size()->set_height(rect.height());
}

gfx::Rect ProtoToRect(const proto::Rect& proto) {
  return gfx::Rect(proto.origin().x(), proto.origin().y(), proto.size().width(),
                   proto.size().height());
}

void RectFToProto(const gfx::RectF& rect, proto::RectF* proto) {
  proto->mutable_origin()->set_x(rect.x());
  proto->mutable_origin()->set_y(rect.y());
  proto->mutable_size()->set_width(rect.width());
  proto->mutable_size()->set_height(rect.height());
}

gfx::RectF ProtoToRectF(const proto::RectF& proto) {
  return gfx::RectF(proto.origin().x(), proto.origin().y(),
                    proto.size().width(), proto.size().height());
}

void SizeToProto(const gfx::Size& size, proto::Size* proto) {
  proto->set_width(size.width());
  proto->set_height(size.height());
}

gfx::Size ProtoToSize(const proto::Size& proto) {
  return gfx::Size(proto.width(), proto.height());
}

void SizeFToProto(const gfx::SizeF& size, proto::SizeF* proto) {
  proto->set_width(size.width());
  proto->set_height(size.height());
}

gfx::SizeF ProtoToSizeF(const proto::SizeF& proto) {
  return gfx::SizeF(proto.width(), proto.height());
}

void TransformToProto(const gfx::Transform& transform,
                      proto::Transform* proto) {
  if (transform.IsIdentity())
    return;

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      proto->add_matrix(transform.matrix().get(i, j));
    }
  }
}

gfx::Transform ProtoToTransform(const proto::Transform& proto) {
  if (proto.matrix_size() == 0)
    return gfx::Transform();

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  DCHECK_EQ(16, proto.matrix_size());
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      transform.matrix().set(i, j, proto.matrix(i * 4 + j));
    }
  }
  return transform;
}

void Vector2dFToProto(const gfx::Vector2dF& vector, proto::Vector2dF* proto) {
  proto->set_x(vector.x());
  proto->set_y(vector.y());
}

gfx::Vector2dF ProtoToVector2dF(const proto::Vector2dF& proto) {
  return gfx::Vector2dF(proto.x(), proto.y());
}

void ScrollOffsetToProto(const gfx::ScrollOffset& scroll_offset,
                         proto::ScrollOffset* proto) {
  proto->set_x(scroll_offset.x());
  proto->set_y(scroll_offset.y());
}

gfx::ScrollOffset ProtoToScrollOffset(const proto::ScrollOffset& proto) {
  return gfx::ScrollOffset(proto.x(), proto.y());
}

void Vector2dToProto(const gfx::Vector2d& vector, proto::Vector2d* proto) {
  proto->set_x(vector.x());
  proto->set_y(vector.y());
}

gfx::Vector2d ProtoToVector2d(const proto::Vector2d& proto) {
  return gfx::Vector2d(proto.x(), proto.y());
}

}  // namespace cc
