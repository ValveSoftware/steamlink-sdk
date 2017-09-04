// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_LWW_REGISTER_H_
#define BLIMP_HELIUM_LWW_REGISTER_H_

#include "base/memory/ptr_util.h"
#include "blimp/helium/blimp_helium_export.h"
#include "blimp/helium/coded_value_serializer.h"
#include "blimp/helium/result.h"
#include "blimp/helium/revision_generator.h"
#include "blimp/helium/syncable.h"
#include "blimp/helium/syncable_common.h"
#include "blimp/helium/version_vector.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"

namespace blimp {
namespace helium {

// Provides a simple syncable and atomically-writable "register" holding
// contents of type |RegisterType|. When there is a write conflict, it is
// resolved by assuming the writer indicated by |bias| has the correct value.
template <class RegisterType>
class BLIMP_HELIUM_EXPORT LwwRegister : public Syncable {
 public:
  LwwRegister(Peer bias, Peer running_as);
  ~LwwRegister() = default;

  void Set(const RegisterType& value);

  const RegisterType& Get() const;

  // Syncable implementation.
  void SetLocalUpdateCallback(base::Closure local_update_callback) override;
  void CreateChangesetToCurrent(
      Revision from,
      google::protobuf::io::CodedOutputStream* output_stream) override;
  Result ApplyChangeset(
      google::protobuf::io::CodedInputStream* input_stream) override;
  void ReleaseBefore(Revision checkpoint) override;
  Revision GetRevision() const override;

 private:
  VersionVector last_modified_;
  bool locally_owned_;
  RegisterType value_;
  bool value_set_ = false;
  base::Closure local_update_callback_;

  DISALLOW_COPY_AND_ASSIGN(LwwRegister);
};

template <class RegisterType>
LwwRegister<RegisterType>::LwwRegister(Peer bias, Peer running_as)
    : last_modified_(0, 0), locally_owned_(bias == running_as) {}

template <class RegisterType>
void LwwRegister<RegisterType>::Set(const RegisterType& value) {
  value_ = value;
  value_set_ = true;
  last_modified_.set_local_revision(GetNextRevision());
  local_update_callback_.Run();
}

template <class RegisterType>
const RegisterType& LwwRegister<RegisterType>::Get() const {
  DCHECK(value_set_);
  return value_;
}

template <class RegisterType>
void LwwRegister<RegisterType>::SetLocalUpdateCallback(
    base::Closure local_update_callback) {
  local_update_callback_ = local_update_callback;
}

template <class RegisterType>
Revision LwwRegister<RegisterType>::GetRevision() const {
  return last_modified_.local_revision();
}

template <class RegisterType>
void LwwRegister<RegisterType>::CreateChangesetToCurrent(
    Revision from,
    google::protobuf::io::CodedOutputStream* output_stream) {
  CodedValueSerializer::Serialize(last_modified_, output_stream);
  CodedValueSerializer::Serialize(value_, output_stream);
}

template <class RegisterType>
Result LwwRegister<RegisterType>::ApplyChangeset(
    google::protobuf::io::CodedInputStream* input_stream) {
  VersionVector remote;
  if (!CodedValueSerializer::Deserialize(input_stream, &remote)) {
    return Result::ERR_PROTOCOL_ERROR;
  }
  remote = remote.Invert();
  VersionVector::Comparison cmp = last_modified_.CompareTo(remote);

  RegisterType input_value;
  if (!CodedValueSerializer::Deserialize(input_stream, &input_value)) {
    return Result::ERR_PROTOCOL_ERROR;
  }
  if (cmp == VersionVector::Comparison::LessThan ||
      (cmp == VersionVector::Comparison::Conflict && !locally_owned_)) {
    value_ = input_value;
    value_set_ = true;
  }

  last_modified_ = last_modified_.MergeWith(remote);
  return Result::SUCCESS;
}

template <class RegisterType>
void LwwRegister<RegisterType>::ReleaseBefore(Revision checkpoint) {
  // no-op
}

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_LWW_REGISTER_H_
