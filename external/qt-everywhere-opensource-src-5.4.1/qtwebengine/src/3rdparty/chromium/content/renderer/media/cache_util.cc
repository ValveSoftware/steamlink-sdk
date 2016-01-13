// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/cache_util.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/http/http_util.h"
#include "net/http/http_version.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

using base::Time;
using base::TimeDelta;
using net::HttpVersion;
using blink::WebURLResponse;

namespace content {

enum { kHttpOK = 200, kHttpPartialContent = 206 };

uint32 GetReasonsForUncacheability(const WebURLResponse& response) {
  uint32 reasons = 0;
  const int code = response.httpStatusCode();
  const int version = response.httpVersion();
  const HttpVersion http_version =
      version == WebURLResponse::HTTP_1_1 ? HttpVersion(1, 1) :
      version == WebURLResponse::HTTP_1_0 ? HttpVersion(1, 0) :
      version == WebURLResponse::HTTP_0_9 ? HttpVersion(0, 9) :
      HttpVersion();
  if (code != kHttpOK && code != kHttpPartialContent)
    reasons |= kNoData;
  if (http_version < HttpVersion(1, 1) && code == kHttpPartialContent)
    reasons |= kPre11PartialResponse;
  if (code == kHttpPartialContent &&
      !net::HttpUtil::HasStrongValidators(
          http_version,
          response.httpHeaderField("etag").utf8(),
          response.httpHeaderField("Last-Modified").utf8(),
          response.httpHeaderField("Date").utf8())) {
    reasons |= kNoStrongValidatorOnPartialResponse;
  }

  std::string cache_control_header =
      response.httpHeaderField("cache-control").utf8();
  StringToLowerASCII(&cache_control_header);
  if (cache_control_header.find("no-cache") != std::string::npos)
    reasons |= kNoCache;
  if (cache_control_header.find("no-store") != std::string::npos)
    reasons |= kNoStore;
  if (cache_control_header.find("must-revalidate") != std::string::npos)
    reasons |= kHasMustRevalidate;

  const TimeDelta kMinimumAgeForUsefulness =
      TimeDelta::FromSeconds(3600);  // Arbitrary value.

  const char kMaxAgePrefix[] = "max-age=";
  const size_t kMaxAgePrefixLen = arraysize(kMaxAgePrefix) - 1;
  if (cache_control_header.substr(0, kMaxAgePrefixLen) == kMaxAgePrefix) {
    int64 max_age_seconds;
    base::StringToInt64(
        base::StringPiece(cache_control_header.begin() + kMaxAgePrefixLen,
                          cache_control_header.end()),
        &max_age_seconds);
    if (TimeDelta::FromSeconds(max_age_seconds) < kMinimumAgeForUsefulness)
      reasons |= kShortMaxAge;
  }

  Time date;
  Time expires;
  if (Time::FromString(response.httpHeaderField("Date").utf8().data(), &date) &&
      Time::FromString(response.httpHeaderField("Expires").utf8().data(),
                       &expires) &&
      date > Time() && expires > Time() &&
      (expires - date) < kMinimumAgeForUsefulness) {
    reasons |= kExpiresTooSoon;
  }

  return reasons;
}

}  // namespace content
