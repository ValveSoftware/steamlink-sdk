// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/element_id.h"

#include <limits>
#include <ostream>

#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "cc/proto/element_id.pb.h"

namespace cc {

bool ElementId::operator==(const ElementId& o) const {
  return primaryId == o.primaryId && secondaryId == o.secondaryId;
}

bool ElementId::operator!=(const ElementId& o) const {
  return !(*this == o);
}

bool ElementId::operator<(const ElementId& o) const {
  return std::tie(primaryId, secondaryId) <
         std::tie(o.primaryId, o.secondaryId);
}

ElementId::operator bool() const {
  return !!primaryId;
}

ElementId LayerIdToElementIdForTesting(int layer_id) {
  return ElementId(std::numeric_limits<int>::max() - layer_id, 0);
}

void ElementId::AddToTracedValue(base::trace_event::TracedValue* res) const {
  res->SetInteger("primaryId", primaryId);
  res->SetInteger("secondaryId", secondaryId);
}

std::unique_ptr<base::Value> ElementId::AsValue() const {
  std::unique_ptr<base::DictionaryValue> res(new base::DictionaryValue());
  res->SetInteger("primaryId", primaryId);
  res->SetInteger("secondaryId", secondaryId);
  return std::move(res);
}

void ElementId::ToProtobuf(proto::ElementId* proto) const {
  proto->set_primary_id(primaryId);
  proto->set_secondary_id(secondaryId);
}

void ElementId::FromProtobuf(const proto::ElementId& proto) {
  primaryId = proto.primary_id();
  secondaryId = proto.secondary_id();
}

size_t ElementIdHash::operator()(ElementId key) const {
  return base::HashInts(key.primaryId, key.secondaryId);
}

std::ostream& operator<<(std::ostream& out, const ElementId& id) {
  return out << "(" << id.primaryId << ", " << id.secondaryId << ")";
}

}  // namespace cc
