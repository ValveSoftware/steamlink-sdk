// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/version_vector.h"

#include <algorithm>

#include "base/logging.h"

namespace blimp {
namespace helium {

VersionVector::VersionVector() {}

VersionVector::VersionVector(Revision local_revision, Revision remote_revision)
    : local_revision_(local_revision), remote_revision_(remote_revision) {}

VersionVector::Comparison VersionVector::CompareTo(
    const VersionVector& other) const {
  DCHECK_GE(local_revision_, other.local_revision());

  if (local_revision_ == other.local_revision()) {
    if (remote_revision_ == other.remote_revision()) {
      return VersionVector::Comparison::EqualTo;
    } else if (remote_revision_ < other.remote_revision()) {
      return VersionVector::Comparison::LessThan;
    } else {
      return VersionVector::Comparison::GreaterThan;
    }
  } else {
    if (local_revision_ > other.local_revision()) {
      if (remote_revision_ == other.remote_revision()) {
        return VersionVector::Comparison::GreaterThan;
      } else {
        return VersionVector::Comparison::Conflict;
      }
    } else {  // We know its not equal or greater, so its smaller
      if (remote_revision_ == other.remote_revision()) {
        return VersionVector::Comparison::LessThan;
      } else {
        LOG(FATAL) << "Local revision should always be greater or equal.";
        return VersionVector::Comparison::Conflict;
      }
    }
  }
}

VersionVector VersionVector::MergeWith(const VersionVector& other) const {
  VersionVector result(std::max(local_revision_, other.local_revision()),
                       std::max(remote_revision_, other.remote_revision()));
  return result;
}

void VersionVector::IncrementLocal() {
  local_revision_++;
}

proto::VersionVectorMessage VersionVector::ToProto() const {
  proto::VersionVectorMessage result;
  result.set_local_revision(local_revision_);
  result.set_remote_revision(remote_revision_);
  return result;
}

VersionVector VersionVector::Invert() const {
  return VersionVector(remote_revision_, local_revision_);
}

}  // namespace helium
}  // namespace blimp
