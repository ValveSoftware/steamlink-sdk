// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_ACCOUNT_ID_ACCOUNT_ID_H_
#define COMPONENTS_SIGNIN_CORE_ACCOUNT_ID_ACCOUNT_ID_H_

#include <stddef.h>

#include <string>
#include "base/containers/hash_tables.h"

// Type that contains enough information to identify user.
//
// TODO(alemate): we are in the process of moving away from std::string as a
// type for storing user identifier to AccountId. At this time GaiaId is mostly
// empty, so this type is used as a replacement for e-mail string.
// But in near future AccountId will become full feature user identifier.
class AccountId {
 public:
  struct EmptyAccountId;

  AccountId(const AccountId& other);

  bool operator==(const AccountId& other) const;
  bool operator!=(const AccountId& other) const;
  bool operator<(const AccountId& right) const;

  // empty() is deprecated. Use !is_valid() instead.
  bool empty() const;
  bool is_valid() const;
  void clear();

  const std::string& GetAccountType() const;
  const std::string& GetGaiaId() const;
  const std::string& GetUserEmail() const;

  // This returns prefixed GaiaId string to be used as a storage key.
  const std::string GetGaiaIdKey() const;

  void SetGaiaId(const std::string& gaia_id);
  void SetUserEmail(const std::string& email);

  // This method is to be used during transition period only.
  static AccountId FromUserEmail(const std::string& user_email);
  // This method is to be used during transition period only.
  static AccountId FromGaiaId(const std::string& gaia_id);
  // This method is the preferred way to construct AccountId if you have
  // full account information.
  static AccountId FromUserEmailGaiaId(const std::string& user_email,
                                       const std::string& gaia_id);

  // These are (for now) unstable and cannot be used to store serialized data to
  // persistent storage. Only in-memory storage is safe.
  // Serialize() returns JSON dictionary,
  // Deserialize() restores AccountId after serialization.
  std::string Serialize() const;
  static bool Deserialize(const std::string& serialized,
                          AccountId* out_account_id);

 private:
  AccountId();
  AccountId(const std::string& gaia_id, const std::string& user_email);

  std::string gaia_id_;
  std::string user_email_;
};

// Returns a reference to a singleton.
const AccountId& EmptyAccountId();

namespace BASE_HASH_NAMESPACE {

// Implement hashing of AccountId, so it can be used as a key in STL containers.
template <>
struct hash<AccountId> {
  std::size_t operator()(const AccountId& user_id) const;
};

}  // namespace BASE_HASH_NAMESPACE

#endif  // COMPONENTS_SIGNIN_CORE_ACCOUNT_ID_ACCOUNT_ID_H_
