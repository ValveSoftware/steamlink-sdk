// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_INFO_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_INFO_H_

#include <string>

// Information about a specific account.
struct AccountInfo {
  AccountInfo();
  AccountInfo(const AccountInfo& other);
  ~AccountInfo();

  std::string account_id;  // The account ID used by OAuth2TokenService.
  std::string gaia;
  std::string email;
  std::string full_name;
  std::string given_name;
  std::string hosted_domain;
  std::string locale;
  std::string picture_url;
  bool is_child_account;

  bool IsValid() const;
  // Updates the empty fields of |this| with |other|. Returns whether at least
  // one field was updated.
  bool UpdateWith(const AccountInfo& other);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_INFO_H_
