// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace user_manager {

namespace {

// Returns account name portion of an email.
std::string GetUserName(const std::string& email) {
  std::string::size_type i = email.find('@');
  if (i == 0 || i == std::string::npos) {
    return email;
  }
  return email.substr(0, i);
}

}  // namespace

// static
bool User::TypeHasGaiaAccount(UserType user_type) {
  return user_type == USER_TYPE_REGULAR ||
         user_type == USER_TYPE_CHILD;
}

const std::string& User::email() const {
  return account_id_.GetUserEmail();
}

// Also used for regular supervised users.
class RegularUser : public User {
 public:
  explicit RegularUser(const AccountId& account_id);
  ~RegularUser() override;

  // Overridden from User:
  UserType GetType() const override;
  bool CanSyncImage() const override;
  void SetIsChild(bool is_child) override;

 private:
  bool is_child_ = false;

  DISALLOW_COPY_AND_ASSIGN(RegularUser);
};

class GuestUser : public User {
 public:
  explicit GuestUser(const AccountId& guest_account_id);
  ~GuestUser() override;

  // Overridden from User:
  UserType GetType() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestUser);
};

class DeviceLocalAccountUserBase : public User {
 public:
  // User:
  bool IsAffiliated() const override;

 protected:
  explicit DeviceLocalAccountUserBase(const AccountId& account_id);
  ~DeviceLocalAccountUserBase() override;
  // User:
  void SetAffiliation(bool) override;
  bool IsDeviceLocalAccount() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountUserBase);
};

class KioskAppUser : public DeviceLocalAccountUserBase {
 public:
  explicit KioskAppUser(const AccountId& kiosk_app_account_id);
  ~KioskAppUser() override;

  // Overridden from User:
  UserType GetType() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(KioskAppUser);
};

class SupervisedUser : public User {
 public:
  explicit SupervisedUser(const AccountId& account_id);
  ~SupervisedUser() override;

  // Overridden from User:
  UserType GetType() const override;
  std::string display_email() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUser);
};

class PublicAccountUser : public DeviceLocalAccountUserBase {
 public:
  explicit PublicAccountUser(const AccountId& account_id);
  ~PublicAccountUser() override;

  // Overridden from User:
  UserType GetType() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PublicAccountUser);
};

User::User(const AccountId& account_id)
    : account_id_(account_id), user_image_(new UserImage) {}

User::~User() {}

std::string User::GetEmail() const {
  return display_email();
}

base::string16 User::GetDisplayName() const {
  // Fallback to the email account name in case display name haven't been set.
  return display_name_.empty() ? base::UTF8ToUTF16(GetAccountName(true))
                               : display_name_;
}

base::string16 User::GetGivenName() const {
  return given_name_;
}

const gfx::ImageSkia& User::GetImage() const {
  return user_image_->image();
}

const AccountId& User::GetAccountId() const {
  return account_id_;
}

void User::SetIsChild(bool is_child) {
  VLOG(1) << "Ignoring SetIsChild call with param " << is_child;
  if (is_child) {
    NOTREACHED() << "Calling SetIsChild(true) for base User class."
                 << "Base class cannot be set as child";
  }
}

bool User::HasGaiaAccount() const {
  return TypeHasGaiaAccount(GetType());
}

bool User::IsSupervised() const {
  UserType type = GetType();
  return  type == USER_TYPE_SUPERVISED ||
          type == USER_TYPE_CHILD;
}

std::string User::GetAccountName(bool use_display_email) const {
  if (use_display_email && !display_email_.empty())
    return GetUserName(display_email_);
  else
    return GetUserName(account_id_.GetUserEmail());
}

bool User::HasDefaultImage() const {
  return UserManager::Get()->IsValidDefaultUserImageId(image_index_);
}

bool User::CanSyncImage() const {
  return false;
}

std::string User::display_email() const {
  return display_email_;
}

bool User::can_lock() const {
  return can_lock_;
}

std::string User::username_hash() const {
  return username_hash_;
}

bool User::is_logged_in() const {
  return is_logged_in_;
}

bool User::is_active() const {
  return is_active_;
}

bool User::IsAffiliated() const {
  return is_affiliated_;
}

void User::SetAffiliation(bool is_affiliated) {
  is_affiliated_ = is_affiliated;
}

bool User::IsDeviceLocalAccount() const {
  return false;
}

User* User::CreateRegularUser(const AccountId& account_id) {
  return new RegularUser(account_id);
}

User* User::CreateGuestUser(const AccountId& guest_account_id) {
  return new GuestUser(guest_account_id);
}

User* User::CreateKioskAppUser(const AccountId& kiosk_app_account_id) {
  return new KioskAppUser(kiosk_app_account_id);
}

User* User::CreateSupervisedUser(const AccountId& account_id) {
  return new SupervisedUser(account_id);
}

User* User::CreatePublicAccountUser(const AccountId& account_id) {
  return new PublicAccountUser(account_id);
}

void User::SetAccountLocale(const std::string& resolved_account_locale) {
  account_locale_.reset(new std::string(resolved_account_locale));
}

void User::SetImage(std::unique_ptr<UserImage> user_image, int image_index) {
  user_image_ = std::move(user_image);
  image_index_ = image_index;
  image_is_stub_ = false;
  image_is_loading_ = false;
  DCHECK(HasDefaultImage() || user_image_->has_image_bytes());
}

void User::SetImageURL(const GURL& image_url) {
  user_image_->set_url(image_url);
}

void User::SetStubImage(std::unique_ptr<UserImage> stub_user_image,
                        int image_index,
                        bool is_loading) {
  user_image_ = std::move(stub_user_image);
  image_index_ = image_index;
  image_is_stub_ = true;
  image_is_loading_ = is_loading;
}

RegularUser::RegularUser(const AccountId& account_id) : User(account_id) {
  set_can_lock(true);
  set_display_email(account_id.GetUserEmail());
}

RegularUser::~RegularUser() {
}

UserType RegularUser::GetType() const {
  return is_child_ ? user_manager::USER_TYPE_CHILD :
                     user_manager::USER_TYPE_REGULAR;
}

bool RegularUser::CanSyncImage() const {
  return true;
}

void RegularUser::SetIsChild(bool is_child) {
  VLOG(1) << "Setting user is child to " << is_child;
  is_child_ = is_child;
}

GuestUser::GuestUser(const AccountId& guest_account_id)
    : User(guest_account_id) {
  set_display_email(std::string());
}

GuestUser::~GuestUser() {
}

UserType GuestUser::GetType() const {
  return user_manager::USER_TYPE_GUEST;
}

DeviceLocalAccountUserBase::DeviceLocalAccountUserBase(
    const AccountId& account_id) : User(account_id) {
}

DeviceLocalAccountUserBase::~DeviceLocalAccountUserBase() {
}

bool DeviceLocalAccountUserBase::IsAffiliated() const {
  return true;
}

void DeviceLocalAccountUserBase::SetAffiliation(bool) {
  // Device local accounts are always affiliated. No affiliation modification
  // must happen.
  NOTREACHED();
}

bool DeviceLocalAccountUserBase::IsDeviceLocalAccount() const {
  return true;
}

KioskAppUser::KioskAppUser(const AccountId& kiosk_app_account_id)
    : DeviceLocalAccountUserBase(kiosk_app_account_id) {
  set_display_email(kiosk_app_account_id.GetUserEmail());
}

KioskAppUser::~KioskAppUser() {
}

UserType KioskAppUser::GetType() const {
  return user_manager::USER_TYPE_KIOSK_APP;
}

SupervisedUser::SupervisedUser(const AccountId& account_id) : User(account_id) {
  set_can_lock(true);
}

SupervisedUser::~SupervisedUser() {
}

UserType SupervisedUser::GetType() const {
  return user_manager::USER_TYPE_SUPERVISED;
}

std::string SupervisedUser::display_email() const {
  return base::UTF16ToUTF8(display_name());
}

PublicAccountUser::PublicAccountUser(const AccountId& account_id)
    : DeviceLocalAccountUserBase(account_id) {}

PublicAccountUser::~PublicAccountUser() {
}

UserType PublicAccountUser::GetType() const {
  return user_manager::USER_TYPE_PUBLIC_ACCOUNT;
}

bool User::has_gaia_account() const {
  static_assert(user_manager::NUM_USER_TYPES == 7,
                "NUM_USER_TYPES should equal 7");
  switch (GetType()) {
    case user_manager::USER_TYPE_REGULAR:
    case user_manager::USER_TYPE_CHILD:
      return true;
    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_SUPERVISED:
    case user_manager::USER_TYPE_KIOSK_APP:
      return false;
    default:
      NOTREACHED();
  }
  return false;
}

}  // namespace user_manager
