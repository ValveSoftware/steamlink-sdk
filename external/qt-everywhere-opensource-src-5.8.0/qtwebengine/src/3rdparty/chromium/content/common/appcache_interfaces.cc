// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/appcache_interfaces.h"

#include <set>

#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "content/public/common/url_constants.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

const char kHttpGETMethod[] = "GET";
const char kHttpHEADMethod[] = "HEAD";

const char kEnableExecutableHandlers[] = "enable-appcache-executable-handlers";

const base::FilePath::CharType kAppCacheDatabaseName[] =
    FILE_PATH_LITERAL("Index");

AppCacheInfo::AppCacheInfo()
    : cache_id(kAppCacheNoCacheId),
      group_id(0),
      status(APPCACHE_STATUS_UNCACHED),
      size(0),
      is_complete(false) {
}

AppCacheInfo::AppCacheInfo(const AppCacheInfo& other) = default;

AppCacheInfo::~AppCacheInfo() {
}

AppCacheResourceInfo::AppCacheResourceInfo()
    : url(),
      size(0),
      is_master(false),
      is_manifest(false),
      is_intercept(false),
      is_fallback(false),
      is_foreign(false),
      is_explicit(false),
      response_id(kAppCacheNoResponseId) {
}

AppCacheResourceInfo::AppCacheResourceInfo(const AppCacheResourceInfo& other) =
    default;

AppCacheResourceInfo::~AppCacheResourceInfo() {
}

AppCacheErrorDetails::AppCacheErrorDetails()
    : message(),
      reason(APPCACHE_UNKNOWN_ERROR),
      url(),
      status(0),
      is_cross_origin(false) {}

AppCacheErrorDetails::AppCacheErrorDetails(
    std::string in_message,
    AppCacheErrorReason in_reason,
    GURL in_url,
    int in_status,
    bool in_is_cross_origin)
    : message(in_message),
      reason(in_reason),
      url(in_url),
      status(in_status),
      is_cross_origin(in_is_cross_origin) {}

AppCacheErrorDetails::~AppCacheErrorDetails() {}

AppCacheNamespace::AppCacheNamespace()
    : type(APPCACHE_FALLBACK_NAMESPACE),
      is_pattern(false),
      is_executable(false) {
}

AppCacheNamespace::AppCacheNamespace(
    AppCacheNamespaceType type, const GURL& url, const GURL& target,
    bool is_pattern)
    : type(type),
      namespace_url(url),
      target_url(target),
      is_pattern(is_pattern),
      is_executable(false) {
}

AppCacheNamespace::AppCacheNamespace(
    AppCacheNamespaceType type, const GURL& url, const GURL& target,
    bool is_pattern, bool is_executable)
    : type(type),
      namespace_url(url),
      target_url(target),
      is_pattern(is_pattern),
      is_executable(is_executable) {
}

AppCacheNamespace::~AppCacheNamespace() {
}

bool AppCacheNamespace::IsMatch(const GURL& url) const {
  if (is_pattern) {
    // We have to escape '?' characters since MatchPattern also treats those
    // as wildcards which we don't want here, we only do '*'s.
    std::string pattern = namespace_url.spec();
    if (namespace_url.has_query())
      base::ReplaceSubstringsAfterOffset(&pattern, 0, "?", "\\?");
    return base::MatchPattern(url.spec(), pattern);
  }
  return base::StartsWith(url.spec(), namespace_url.spec(),
                          base::CompareCase::SENSITIVE);
}

bool IsSchemeSupportedForAppCache(const GURL& url) {
  bool supported = url.SchemeIs(url::kHttpScheme) ||
                   url.SchemeIs(url::kHttpsScheme) ||
                   url.SchemeIs(kChromeDevToolsScheme);

#ifndef NDEBUG
  // TODO(michaeln): It would be really nice if this could optionally work for
  // file and filesystem urls too to help web developers experiment and test
  // their apps, perhaps enabled via a cmd line flag or some other developer
  // tool setting.  Unfortunately file scheme net::URLRequests don't produce the
  // same signalling (200 response codes, headers) as http URLRequests, so this
  // doesn't work just yet.
  // supported |= url.SchemeIsFile();
#endif
  return supported;
}

bool IsMethodSupportedForAppCache(const std::string& method) {
  return (method == kHttpGETMethod) || (method == kHttpHEADMethod);
}

bool IsSchemeAndMethodSupportedForAppCache(const net::URLRequest* request) {
  return IsSchemeSupportedForAppCache(request->url()) &&
         IsMethodSupportedForAppCache(request->method());
}

}  // namespace content
