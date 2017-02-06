// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_info_impl.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/signin/core/account_id/account_id.h"

namespace user_manager {

UserInfoImpl::UserInfoImpl()
    : account_id_(AccountId::FromUserEmail("stub-user@domain.com")) {}

UserInfoImpl::~UserInfoImpl() {
}

base::string16 UserInfoImpl::GetDisplayName() const {
  return base::UTF8ToUTF16("stub-user");
}

base::string16 UserInfoImpl::GetGivenName() const {
  return base::UTF8ToUTF16("Stub");
}

std::string UserInfoImpl::GetEmail() const {
  return account_id_.GetUserEmail();
}

const AccountId& UserInfoImpl::GetAccountId() const {
  return account_id_;
}

const gfx::ImageSkia& UserInfoImpl::GetImage() const {
  return user_image_;
}

}  // namespace user_manager
