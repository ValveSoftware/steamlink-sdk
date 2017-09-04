// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_VERSION_VECTOR_H_
#define BLIMP_HELIUM_VERSION_VECTOR_H_

#include <stdint.h>

#include "blimp/common/proto/helium.pb.h"
#include "blimp/helium/blimp_helium_export.h"

namespace blimp {
namespace helium {

// Version Vectors are used to store revisions of both client and engine
// objects. It will allow determining partial ordering between 2 VersionVectors.
//
// It's similar to a vector clock, but it different according to the Wikipedia
// definition specially when handling merge. Vector Clocks specifies you should
// advance your local version, whereas VersionVectors you won't advance your
// local version (Unless you have to handle a merge conflict and requires to
// send data to the other side).

typedef uint64_t Revision;

class BLIMP_HELIUM_EXPORT VersionVector {
 public:
  enum class Comparison { LessThan, EqualTo, GreaterThan, Conflict };

  VersionVector(Revision local_revision, Revision remote_revision);
  VersionVector();
  VersionVector(const VersionVector&) = default;

  // Compares two vector clocks. There are 4 possibilities for the result:
  // * LessThan: One revision is equal and for the other is smaller.
  // (1,0).CompareTo((2, 0));
  // * EqualTo: Both revisions are the same.
  // * GreaterThan: One revision is equal and for the other is greater.
  // (2,0).CompareTo((1, 0));
  // * Conflict: Both revisions are different. (1,0).CompareTo(0,1)
  Comparison CompareTo(const VersionVector& other) const;

  // Merges two vector clocks. This function should be used at synchronization
  // points. i.e. when client receives data from the server or vice versa.
  VersionVector MergeWith(const VersionVector& other) const;

  // Increments local_revision_ by one. This is used when something changes
  // in the local state like setting a property or applying a change set with
  // a conflict that requires sending state to the other peer.
  void IncrementLocal();

  Revision local_revision() const { return local_revision_; }

  void set_local_revision(Revision local_revision) {
    local_revision_ = local_revision;
  }

  Revision remote_revision() const { return remote_revision_; }

  void set_remote_revision(Revision remote_revision) {
    remote_revision_ = remote_revision;
  }

  // Create the proto message corresponding to this object.
  proto::VersionVectorMessage ToProto() const;

  // Inverts the local and remote components respectively.
  // Used when we send VersionVector across the wire. The local becomes
  // remote and vice versa.
  VersionVector Invert() const;

 private:
  Revision local_revision_ = 0;
  Revision remote_revision_ = 0;
};

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_VERSION_VECTOR_H_
