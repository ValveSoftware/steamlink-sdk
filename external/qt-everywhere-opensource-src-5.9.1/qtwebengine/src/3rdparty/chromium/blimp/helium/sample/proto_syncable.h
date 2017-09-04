// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_SAMPLE_PROTO_SYNCABLE_H_
#define BLIMP_HELIUM_SAMPLE_PROTO_SYNCABLE_H_

#include <memory>

#include "blimp/helium/result.h"
#include "blimp/helium/version_vector.h"

namespace blimp {
namespace helium {

template <class ChangesetType>
class ProtoSyncable {
 public:
  virtual ~ProtoSyncable() {}

  virtual std::unique_ptr<ChangesetType> CreateChangesetToCurrent(
      Revision from) = 0;

  virtual Result ApplyChangeset(Revision to,
                                std::unique_ptr<ChangesetType> changeset) = 0;

  virtual void ReleaseBefore(Revision checkpoint) = 0;

  virtual VersionVector GetVersionVector() const = 0;
};

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_SAMPLE_PROTO_SYNCABLE_H_
