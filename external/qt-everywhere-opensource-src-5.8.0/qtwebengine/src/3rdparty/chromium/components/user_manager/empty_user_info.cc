// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/empty_user_info.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/signin/core/account_id/account_id.h"

namespace user_manager {

EmptyUserInfo::EmptyUserInfo() {
}

EmptyUserInfo::~EmptyUserInfo() {
}

base::string16 EmptyUserInfo::GetDisplayName() const {
  NOTIMPLEMENTED();
  return base::string16();
}

base::string16 EmptyUserInfo::GetGivenName() const {
  NOTIMPLEMENTED();
  return base::string16();
}

std::string EmptyUserInfo::GetEmail() const {
  NOTIMPLEMENTED();
  return std::string();
}

const AccountId& EmptyUserInfo::GetAccountId() const {
  NOTIMPLEMENTED();
  return EmptyAccountId();
}

const gfx::ImageSkia& EmptyUserInfo::GetImage() const {
  NOTIMPLEMENTED();
  // To make the compiler happy.
  return null_image_;
}

}  // namespace user_manager
