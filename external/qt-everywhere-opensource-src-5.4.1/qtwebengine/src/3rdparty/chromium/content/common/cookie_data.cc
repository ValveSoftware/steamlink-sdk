// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cookie_data.h"

#include "net/cookies/canonical_cookie.h"

namespace content {

CookieData::CookieData()
    : expires(0),
      http_only(false),
      secure(false),
      session(false) {
}

CookieData::CookieData(const net::CanonicalCookie& c)
    : name(c.Name()),
      value(c.Value()),
      domain(c.Domain()),
      path(c.Path()),
      expires(c.ExpiryDate().ToDoubleT() * 1000),
      http_only(c.IsHttpOnly()),
      secure(c.IsSecure()),
      session(!c.IsPersistent()) {
}

CookieData::~CookieData() {
}

}  // namespace content
