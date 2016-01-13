// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/mock_allow_url_security_manager.h"

namespace net {

MockAllowURLSecurityManager::MockAllowURLSecurityManager() {}

MockAllowURLSecurityManager::~MockAllowURLSecurityManager() {}

bool MockAllowURLSecurityManager::CanUseDefaultCredentials(
    const GURL& auth_origin) const {
  return true;
}

bool MockAllowURLSecurityManager::CanDelegate(const GURL& auth_origin) const {
  return true;
}

}  // namespace net
