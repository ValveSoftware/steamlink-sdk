// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_COMPOUND_SYNCABLE_H_
#define BLIMP_HELIUM_COMPOUND_SYNCABLE_H_

#include <stdint.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "blimp/helium/blimp_helium_export.h"
#include "blimp/helium/syncable.h"

namespace blimp {
namespace helium {

// Handles the creation and tracking of member Syncable objects.
// Is the exclusive creator of RegisteredMember container objects, which
// enforces the convention that all child Syncables must be created with, and
// therefore known to, a MemberRegistry.

// Base class for composite Syncable objects.
// Manages the fan-out of all Syncable operations to registered member
// Syncables. Member syncables are created by calling
// CreateAndRegister(<ctor args>).
class BLIMP_HELIUM_EXPORT CompoundSyncable : public Syncable {
 public:
  CompoundSyncable();
  ~CompoundSyncable() override;

  // Syncable implementation.
  void SetLocalUpdateCallback(base::Closure local_update_callback) override;
  void CreateChangesetToCurrent(
      Revision from,
      google::protobuf::io::CodedOutputStream* output_stream) override;
  Result ApplyChangeset(
      google::protobuf::io::CodedInputStream* changeset) override;
  void ReleaseBefore(Revision from) override;
  Revision GetRevision() const override;

 protected:
  template <typename T>
  class RegisteredMember {
   public:
    ~RegisteredMember() = default;
    RegisteredMember(RegisteredMember<T>&& other) = default;

    T* get() const { return member_.get(); }
    T* operator->() const { return member_.get(); }

   private:
    friend class CompoundSyncable;

    // Make class only constructable by CompoundSyncable, so that there is a
    // strong guarantee that a RegisteredMember was created by
    // MemberRegistry::CreateAndRegister().
    explicit RegisteredMember(std::unique_ptr<T> member)
        : member_(std::move(member)) {}

    std::unique_ptr<T> member_;

    DISALLOW_COPY_AND_ASSIGN(RegisteredMember<T>);
  };

  template <typename T, typename... ConstructorArgs>
  RegisteredMember<T> CreateAndRegister(ConstructorArgs... args) {
    std::unique_ptr<T> new_member =
        base::MakeUnique<T>(std::forward<ConstructorArgs>(args)...);
    members_.push_back(new_member.get());
    return RegisteredMember<T>(std::move(new_member));
  }

 private:
  // Tracks all Syncables* which have been created with CreateAndRegister().
  std::vector<Syncable*> members_;

  DISALLOW_COPY_AND_ASSIGN(CompoundSyncable);
};

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_COMPOUND_SYNCABLE_H_
