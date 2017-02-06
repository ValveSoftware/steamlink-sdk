// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_ERROR_PAGE_SUPERVISED_USER_ERROR_PAGE_H_
#define COMPONENTS_SUPERVISED_USER_ERROR_PAGE_SUPERVISED_USER_ERROR_PAGE_H_

#include <string>

#include "base/strings/string16.h"

namespace supervised_user_error_page {

enum FilteringBehaviorReason {
  DEFAULT,
  ASYNC_CHECKER,
  BLACKLIST,
  MANUAL,
  WHITELIST
};

int GetBlockMessageID(
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool is_child_account,
    bool single_parent);

std::string BuildHtml(bool allow_access_requests,
                      const std::string& profile_image_url,
                      const std::string& profile_image_url2,
                      const base::string16& custodian,
                      const base::string16& custodian_email,
                      const base::string16& second_custodian,
                      const base::string16& second_custodian_email,
                      bool is_child_account,
                      FilteringBehaviorReason reason,
                      const std::string& app_locale);

}  //  namespace supervised_user_error_page

#endif // COMPONENTS_SUPERVISED_USER_ERROR_PAGE_SUPERVISED_USER_ERROR_PAGE_H_
