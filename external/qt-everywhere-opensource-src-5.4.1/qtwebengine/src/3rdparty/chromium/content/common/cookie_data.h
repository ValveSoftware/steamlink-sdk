// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_COOKIE_DATA_H_
#define CONTENT_COMMON_COOKIE_DATA_H_

#include <string>

#include "content/common/content_export.h"

namespace net {
class CanonicalCookie;
}

namespace content {

struct CONTENT_EXPORT CookieData {
  CookieData();
  explicit CookieData(const net::CanonicalCookie& c);
  ~CookieData();

  // Cookie name.
  std::string name;

  // Cookie value.
  std::string value;

  // Cookie domain.
  std::string domain;

  // Cookie path.
  std::string path;

  // Cookie expires param if any.
  double expires;

  // Cookie HTTPOnly param.
  bool http_only;

  // Cookie secure param.
  bool secure;

  // Session cookie flag.
  bool session;
};

}  // namespace content

#endif  // CONTENT_COMMON_COOKIE_DATA_H_
