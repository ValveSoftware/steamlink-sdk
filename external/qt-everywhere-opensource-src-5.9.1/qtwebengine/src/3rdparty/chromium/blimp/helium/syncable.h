// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_SYNCABLE_H_
#define BLIMP_HELIUM_SYNCABLE_H_

#include <stdint.h>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "blimp/common/mandatory_callback.h"
#include "blimp/helium/result.h"
#include "blimp/helium/version_vector.h"

namespace google {
namespace protobuf {
namespace io {
class CodedInputStream;
class CodedOutputStream;
}  // namespace io
}  // namespace protobuf
}  // namespace google

namespace blimp {
namespace helium {

namespace proto {
class ChangesetMessage;
}

// Syncable is something that supports creating and restoring Changesets.
// These objects exchange Changesets between the client and server to keep
// their peer counterparts eventually synchronized.
//
// It provides the following things:
// 1. Mapping: There is a one-to-one relationship between instances on the
// client and instances on the engine.
// 2. Consistency: The values stored in client-engine pairs should eventually be
// equal.
//
// Syncable is a base interface that is used for both self contained
// objects (i.e. Simple register) and objects which are disconnected replicas
// of external state.
class Syncable {
 public:
  virtual ~Syncable() {}

  // Sets the callback that the Syncable should invoke immediately after being
  // modified locally.
  virtual void SetLocalUpdateCallback(base::Closure local_update_callback) = 0;

  // Emits a byte stream representation a changeset comprised of the changes
  // between |from| and the current revision.
  //
  // The Sync layer will construct the changeset with details since |from|,
  // but the Syncable is responsible for including any revision information
  // additional to that expressed by the VersionVectors, that is necessary to
  // detect and resolve conflicts.
  virtual void CreateChangesetToCurrent(
      Revision from,
      google::protobuf::io::CodedOutputStream* output_stream) = 0;

  // Reads and applies a stream of changes originating from this Syncable. The
  // unconsumed bytes of |input_stream| are left intact.
  // The Syncable is responsible for including sufficient revision data in the
  // changeset in order to detect change conflicts.
  virtual Result ApplyChangeset(
      google::protobuf::io::CodedInputStream* input_stream) = 0;

  // Gives a chance for the Syncable to delete any old data prior to
  // |checkpoint|.
  virtual void ReleaseBefore(Revision checkpoint) = 0;

  // Returns the VersionVector reflecting the last modified state of |this|.
  virtual Revision GetRevision() const = 0;
};

// Extends the Syncable interface by adding support to asynchronously replicate
// state with some
// external entity.
//
// TwoPhaseSyncable name derives from the fact that the state is both created
// and applied in two stages:
//
// 1. Creation
// 1.1) PreCreateChangesetToCurrent is called which retrieves the state
// from an external state and saves locally.
// 1.2) CreateChangesetToCurrent is called to actually create the changeset.
//
// 2. Updating
// 2.1) ApplyChangeset is called which updates the local state and propagates
//      changes to external state.
class TwoPhaseSyncable : public Syncable {
 public:
  ~TwoPhaseSyncable() override {}

  // This is before calling CreateChangesetToCurrent to give a change for the
  // TwoPhaseSyncable to pull the latest changes into its local state.
  //
  // The callback |done| should be called once the local instance is ready
  // to accept the call to CreateChangesetToCurrent.
  virtual void PreCreateChangesetToCurrent(Revision from,
                                           MandatoryClosure&& done) = 0;
};
}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_SYNCABLE_H_
