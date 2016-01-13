// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/url_security_manager.h"

#include "net/http/http_auth_filter.h"

namespace net {

// static
URLSecurityManager* URLSecurityManager::Create(
    const HttpAuthFilter* whitelist_default,
    const HttpAuthFilter* whitelist_delegate) {
  return new URLSecurityManagerWhitelist(whitelist_default, whitelist_delegate);
}

}  //  namespace net
