// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/account_id/account_id.h"

#include <functional>
#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/singleton.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

// Known account types.
const char kGoogle[] = "google";

// Serialization keys
const char kGaiaIdKey[] = "gaia_id";
const char kEmailKey[] = "email";

// Prefix for GetGaiaIdKey().
const char kKeyGaiaIdPrefix[] = "g-";

struct GoogleStringSingleton {
  GoogleStringSingleton() : google(kGoogle) {}

  static GoogleStringSingleton* GetInstance() {
    return base::Singleton<GoogleStringSingleton>::get();
  }

  const std::string google;
};

}  // anonymous namespace

struct AccountId::EmptyAccountId {
  EmptyAccountId() : user_id() {}
  const AccountId user_id;

  static EmptyAccountId* GetInstance() {
    return base::Singleton<EmptyAccountId>::get();
  }
};

AccountId::AccountId() {}

AccountId::AccountId(const std::string& gaia_id, const std::string& user_email)
    : gaia_id_(gaia_id), user_email_(user_email) {
  // Fail if e-mail looks similar to GaiaIdKey.
  LOG_ASSERT(!base::StartsWith(user_email, kKeyGaiaIdPrefix,
                               base::CompareCase::SENSITIVE) ||
             user_email.find('@') != std::string::npos)
      << "Bad e-mail: '" << user_email << "' with gaia_id='" << gaia_id << "'";

  // TODO(alemate): DCHECK(!email.empty());
  // TODO(alemate): check gaia_id is not empty once it is required.
}

AccountId::AccountId(const AccountId& other)
    : gaia_id_(other.gaia_id_), user_email_(other.user_email_) {}

bool AccountId::operator==(const AccountId& other) const {
  return (this == &other) ||
         (gaia_id_ == other.gaia_id_ && user_email_ == other.user_email_) ||
         (!gaia_id_.empty() && gaia_id_ == other.gaia_id_) ||
         (!user_email_.empty() && user_email_ == other.user_email_);
}

bool AccountId::operator!=(const AccountId& other) const {
  return !operator==(other);
}

bool AccountId::operator<(const AccountId& right) const {
  // TODO(alemate): update this once all AccountId members are filled.
  return user_email_ < right.user_email_;
}

bool AccountId::empty() const {
  return gaia_id_.empty() && user_email_.empty();
}

bool AccountId::is_valid() const {
  return /* !gaia_id_.empty() && */ !user_email_.empty();
}

void AccountId::clear() {
  gaia_id_.clear();
  user_email_.clear();
}

const std::string& AccountId::GetAccountType() const {
  return GoogleStringSingleton::GetInstance()->google;
}

const std::string& AccountId::GetGaiaId() const {
  return gaia_id_;
}

const std::string& AccountId::GetUserEmail() const {
  return user_email_;
}

const std::string AccountId::GetGaiaIdKey() const {
#ifdef NDEBUG
  if (gaia_id_.empty())
    LOG(FATAL) << "GetGaiaIdKey(): no gaia id for " << Serialize();

#else
  CHECK(!gaia_id_.empty());
#endif

  return std::string(kKeyGaiaIdPrefix) + gaia_id_;
}

void AccountId::SetGaiaId(const std::string& gaia_id) {
  DCHECK(!gaia_id.empty());
  gaia_id_ = gaia_id;
}

void AccountId::SetUserEmail(const std::string& email) {
  DCHECK(!email.empty());
  user_email_ = email;
}

// static
AccountId AccountId::FromUserEmail(const std::string& email) {
  // TODO(alemate): DCHECK(!email.empty());
  return AccountId(std::string() /* gaia_id */, email);
}

AccountId AccountId::FromGaiaId(const std::string& gaia_id) {
  DCHECK(!gaia_id.empty());
  return AccountId(gaia_id, std::string() /* email */);
}

// static
AccountId AccountId::FromUserEmailGaiaId(const std::string& email,
                                         const std::string& gaia_id) {
  DCHECK(!(email.empty() && gaia_id.empty()));
  return AccountId(gaia_id, email);
}

std::string AccountId::Serialize() const {
  base::DictionaryValue value;
  value.SetString(kGaiaIdKey, gaia_id_);
  value.SetString(kEmailKey, user_email_);

  std::string serialized;
  base::JSONWriter::Write(value, &serialized);
  return serialized;
}

// static
bool AccountId::Deserialize(const std::string& serialized,
                            AccountId* account_id) {
  base::JSONReader reader;
  std::unique_ptr<const base::Value> value(reader.Read(serialized));
  const base::DictionaryValue* dictionary_value = NULL;

  if (!value || !value->GetAsDictionary(&dictionary_value))
    return false;

  std::string gaia_id;
  std::string user_email;

  const bool found_gaia_id = dictionary_value->GetString(kGaiaIdKey, &gaia_id);
  const bool found_user_email =
      dictionary_value->GetString(kEmailKey, &user_email);

  if (!found_gaia_id)
    LOG(ERROR) << "gaia_id is not found in '" << serialized << "'";

  if (!found_user_email)
    LOG(ERROR) << "user_email is not found in '" << serialized << "'";

  if (!found_gaia_id && !found_user_email)
    return false;

  *account_id = FromUserEmailGaiaId(user_email, gaia_id);

  return true;
}

const AccountId& EmptyAccountId() {
  return AccountId::EmptyAccountId::GetInstance()->user_id;
}

namespace BASE_HASH_NAMESPACE {

std::size_t hash<AccountId>::operator()(const AccountId& user_id) const {
  return hash<std::string>()(user_id.GetUserEmail());
}

}  // namespace BASE_HASH_NAMESPACE
