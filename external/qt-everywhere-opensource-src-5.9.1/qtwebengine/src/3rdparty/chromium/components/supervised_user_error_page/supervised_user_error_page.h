// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_ERROR_PAGE_SUPERVISED_USER_ERROR_PAGE_H_
#define COMPONENTS_SUPERVISED_USER_ERROR_PAGE_SUPERVISED_USER_ERROR_PAGE_H_

#include <string>

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
                      const std::string& custodian,
                      const std::string& custodian_email,
                      const std::string& second_custodian,
                      const std::string& second_custodian_email,
                      bool is_child_account,
                      FilteringBehaviorReason reason,
                      const std::string& app_locale);

}  //  namespace supervised_user_error_page

#endif // COMPONENTS_SUPERVISED_USER_ERROR_PAGE_SUPERVISED_USER_ERROR_PAGE_H_
