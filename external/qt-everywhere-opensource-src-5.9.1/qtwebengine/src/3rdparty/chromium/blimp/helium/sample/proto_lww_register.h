// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_SAMPLE_PROTO_LWW_REGISTER_H_
#define BLIMP_HELIUM_SAMPLE_PROTO_LWW_REGISTER_H_

#include "base/memory/ptr_util.h"
#include "blimp/helium/blimp_helium_export.h"
#include "blimp/helium/result.h"
#include "blimp/helium/revision_generator.h"
#include "blimp/helium/sample/proto_syncable.h"
#include "blimp/helium/sample/proto_value_converter.h"
#include "blimp/helium/sample/sample.pb.h"
#include "blimp/helium/syncable_common.h"
#include "blimp/helium/version_vector.h"

namespace blimp {
namespace helium {

template <class RegisterType>
class BLIMP_HELIUM_EXPORT ProtoLwwRegister
    : public ProtoSyncable<LwwRegisterChangesetMessage> {
 public:
  ProtoLwwRegister(Peer bias, Peer running_as);
  ~ProtoLwwRegister() = default;

  void Set(const RegisterType& value);

  const RegisterType& Get() const;

  // ProtoSyncable implementation.
  std::unique_ptr<LwwRegisterChangesetMessage> CreateChangesetToCurrent(
      Revision from) override;
  Result ApplyChangeset(
      Revision to,
      std::unique_ptr<LwwRegisterChangesetMessage> changeset) override;
  void ReleaseBefore(Revision checkpoint) override;
  VersionVector GetVersionVector() const override;

 private:
  VersionVector last_modified_;
  bool locally_owned_;
  RegisterType value_;
  bool value_set_ = false;

  DISALLOW_COPY_AND_ASSIGN(ProtoLwwRegister);
};

template <class RegisterType>
ProtoLwwRegister<RegisterType>::ProtoLwwRegister(Peer bias, Peer running_as)
    : locally_owned_(bias == running_as) {}

template <class RegisterType>
void ProtoLwwRegister<RegisterType>::Set(const RegisterType& value) {
  value_ = value;
  value_set_ = true;
  last_modified_.set_local_revision(GetNextRevision());
}

template <class RegisterType>
const RegisterType& ProtoLwwRegister<RegisterType>::Get() const {
  DCHECK(value_set_);
  return value_;
}

template <class RegisterType>
VersionVector ProtoLwwRegister<RegisterType>::GetVersionVector() const {
  return last_modified_;
}

template <class RegisterType>
std::unique_ptr<LwwRegisterChangesetMessage>
ProtoLwwRegister<RegisterType>::CreateChangesetToCurrent(Revision from) {
  std::unique_ptr<LwwRegisterChangesetMessage> changeset =
      base::MakeUnique<LwwRegisterChangesetMessage>();
  ProtoValueConverter::ToMessage(last_modified_,
                                 changeset->mutable_last_modified());
  ProtoValueConverter::ToMessage(value_, changeset->mutable_value());
  return changeset;
}

template <class RegisterType>
Result ProtoLwwRegister<RegisterType>::ApplyChangeset(
    Revision to,
    std::unique_ptr<LwwRegisterChangesetMessage> changeset) {
  VersionVector remote;
  if (!ProtoValueConverter::FromMessage(changeset->last_modified(), &remote)) {
    return Result::ERR_PROTOCOL_ERROR;
  }
  remote = remote.Invert();

  VersionVector::Comparison cmp = last_modified_.CompareTo(remote);
  if (cmp == VersionVector::Comparison::LessThan ||
      (cmp == VersionVector::Comparison::Conflict && !locally_owned_)) {
    if (!ProtoValueConverter::FromMessage(changeset->value(), &value_)) {
      return Result::ERR_PROTOCOL_ERROR;
    }
    value_set_ = true;
  }
  last_modified_ = last_modified_.MergeWith(remote);

  return Result::SUCCESS;
}

template <class RegisterType>
void ProtoLwwRegister<RegisterType>::ReleaseBefore(Revision checkpoint) {
  // no-op
}

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_SAMPLE_PROTO_LWW_REGISTER_H_
