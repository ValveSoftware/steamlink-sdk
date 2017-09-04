// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/compound_syncable.h"

#include <algorithm>
#include <bitset>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"

namespace blimp {
namespace helium {

CompoundSyncable::CompoundSyncable() {}

CompoundSyncable::~CompoundSyncable() {}

void CompoundSyncable::SetLocalUpdateCallback(
    base::Closure local_update_callback) {
  for (const auto& member : members_) {
    member->SetLocalUpdateCallback(local_update_callback);
  }
}

void CompoundSyncable::CreateChangesetToCurrent(
    Revision from,
    google::protobuf::io::CodedOutputStream* changeset) {
  // Write out a bitarray representing the modified state, where for each
  // modified member |members_[i]|, the bit modified[i] is set to 1.
  // The bits for unmodified members_ are left as zero.
  DCHECK_LE(members_.size(), 64u);
  std::bitset<64> modified;
  for (size_t i = 0; i < members_.size(); ++i) {
    if (members_[i]->GetRevision() >= from) {
      modified[i] = true;
    }
  }
  DCHECK_GT(modified.count(), 0u);

  changeset->WriteVarint64(modified.to_ullong());
  for (size_t i = 0; i < members_.size(); ++i) {
    if (modified[i]) {
      int prev_bytes_written = changeset->ByteCount();

      members_[i]->CreateChangesetToCurrent(from, changeset);

      // Ensure all "modified" children have serialized non-empty payloads.
      DCHECK_GT(changeset->ByteCount(), prev_bytes_written);
    }
  }
}

Result CompoundSyncable::ApplyChangeset(
    google::protobuf::io::CodedInputStream* changeset) {
  uint64_t modified_header;
  if (!changeset->ReadVarint64(&modified_header)) {
    DLOG(FATAL) << "Couldn't read modification header.";
    return Result::ERR_PROTOCOL_ERROR;
  }
  if (modified_header > (1u << members_.size())) {
    DLOG(FATAL) << "Invalid modification header.";
    return Result::ERR_PROTOCOL_ERROR;
  }

  std::bitset<64> modified(modified_header);

  for (size_t member_id = 0u; member_id < members_.size(); ++member_id) {
    if (!modified[member_id]) {
      continue;
    }

    Result child_result = members_[member_id]->ApplyChangeset(changeset);
    if (child_result != Result::SUCCESS) {
      return child_result;
    }
  }

  return Result::SUCCESS;
}

Revision CompoundSyncable::GetRevision() const {
  Revision merged = 0u;
  for (const auto& member : members_) {
    merged = std::max(merged, member->GetRevision());
  }
  return merged;
}

void CompoundSyncable::ReleaseBefore(Revision from) {
  for (const auto& next_child : members_) {
    next_child->ReleaseBefore(from);
  }
}

}  // namespace helium
}  // namespace blimp
