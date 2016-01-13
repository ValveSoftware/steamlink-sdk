// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_STATIC_HTTP_USER_AGENT_SETTINGS_H_
#define NET_URL_REQUEST_STATIC_HTTP_USER_AGENT_SETTINGS_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/url_request/http_user_agent_settings.h"

namespace net {

// An implementation of |HttpUserAgentSettings| that always provides the
// same constant values for the HTTP Accept-Language and User-Agent headers.
class NET_EXPORT StaticHttpUserAgentSettings : public HttpUserAgentSettings {
 public:
  StaticHttpUserAgentSettings(const std::string& accept_language,
                              const std::string& user_agent);
  virtual ~StaticHttpUserAgentSettings();

  // HttpUserAgentSettings implementation
  virtual std::string GetAcceptLanguage() const OVERRIDE;
  virtual std::string GetUserAgent() const OVERRIDE;

 private:
  const std::string accept_language_;
  const std::string user_agent_;

  DISALLOW_COPY_AND_ASSIGN(StaticHttpUserAgentSettings);
};

}  // namespace net

#endif  // NET_URL_REQUEST_STATIC_HTTP_USER_AGENT_SETTINGS_H_

