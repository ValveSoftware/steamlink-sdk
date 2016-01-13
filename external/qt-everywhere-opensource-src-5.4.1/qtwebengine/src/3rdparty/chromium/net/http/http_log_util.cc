// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_log_util.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_auth_challenge_tokenizer.h"

namespace net {

namespace {

bool ShouldRedactChallenge(HttpAuthChallengeTokenizer* challenge) {
  // Ignore lines with commas, as they may contain lists of schemes, and
  // the information we want to hide is Base64 encoded, so has no commas.
  if (challenge->challenge_text().find(',') != std::string::npos)
    return false;

  std::string scheme = StringToLowerASCII(challenge->scheme());
  // Invalid input.
  if (scheme.empty())
    return false;

  // Ignore Basic and Digest authentication challenges, as they contain
  // public information.
  if (scheme == "basic" || scheme == "digest")
    return false;

  return true;
}

}  // namespace

std::string ElideHeaderValueForNetLog(NetLog::LogLevel log_level,
                                      const std::string& header,
                                      const std::string& value) {
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  if (!base::strcasecmp(header.c_str(), "proxy-authorization") ||
      !base::strcasecmp(header.c_str(), "proxy-authenticate")) {
    return "[elided]";
  }
#endif

  if (log_level < NetLog::LOG_STRIP_PRIVATE_DATA)
    return value;

  // Note: this logic should be kept in sync with stripCookiesAndLoginInfo in
  // chrome/browser/resources/net_internals/log_view_painter.js.

  std::string::const_iterator redact_begin = value.begin();
  std::string::const_iterator redact_end = value.begin();
  if (!base::strcasecmp(header.c_str(), "set-cookie") ||
      !base::strcasecmp(header.c_str(), "set-cookie2") ||
      !base::strcasecmp(header.c_str(), "cookie") ||
      !base::strcasecmp(header.c_str(), "authorization") ||
      !base::strcasecmp(header.c_str(), "proxy-authorization")) {
    redact_begin = value.begin();
    redact_end = value.end();
  } else if (!base::strcasecmp(header.c_str(), "www-authenticate") ||
             !base::strcasecmp(header.c_str(), "proxy-authenticate")) {
    // Look for authentication information from data received from the server in
    // multi-round Negotiate authentication.
    HttpAuthChallengeTokenizer challenge(value.begin(), value.end());
    if (ShouldRedactChallenge(&challenge)) {
      redact_begin = challenge.params_begin();
      redact_end = challenge.params_end();
    }
  }

  if (redact_begin == redact_end)
    return value;

  return std::string(value.begin(), redact_begin) +
      base::StringPrintf("[%ld bytes were stripped]",
                         static_cast<long>(redact_end - redact_begin)) +
      std::string(redact_end, value.end());
}

}  // namespace net
