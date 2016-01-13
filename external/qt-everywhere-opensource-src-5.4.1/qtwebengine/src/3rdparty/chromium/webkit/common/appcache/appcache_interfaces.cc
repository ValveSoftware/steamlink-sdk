// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/appcache/appcache_interfaces.h"

#include <set>

#include "base/strings/string_util.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace appcache {

const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const char kDevToolsScheme[] = "chrome-devtools";
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

Namespace::Namespace()
    : type(APPCACHE_FALLBACK_NAMESPACE),
      is_pattern(false),
      is_executable(false) {
}

Namespace::Namespace(
    AppCacheNamespaceType type, const GURL& url, const GURL& target,
    bool is_pattern)
    : type(type),
      namespace_url(url),
      target_url(target),
      is_pattern(is_pattern),
      is_executable(false) {
}

Namespace::Namespace(
    AppCacheNamespaceType type, const GURL& url, const GURL& target,
    bool is_pattern, bool is_executable)
    : type(type),
      namespace_url(url),
      target_url(target),
      is_pattern(is_pattern),
      is_executable(is_executable) {
}

Namespace::~Namespace() {
}

bool Namespace::IsMatch(const GURL& url) const {
  if (is_pattern) {
    // We have to escape '?' characters since MatchPattern also treats those
    // as wildcards which we don't want here, we only do '*'s.
    std::string pattern = namespace_url.spec();
    if (namespace_url.has_query())
      ReplaceSubstringsAfterOffset(&pattern, 0, "?", "\\?");
    return MatchPattern(url.spec(), pattern);
  }
  return StartsWithASCII(url.spec(), namespace_url.spec(), true);
}

bool IsSchemeSupported(const GURL& url) {
  bool supported = url.SchemeIs(kHttpScheme) || url.SchemeIs(kHttpsScheme) ||
      url.SchemeIs(kDevToolsScheme);

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

bool IsMethodSupported(const std::string& method) {
  return (method == kHttpGETMethod) || (method == kHttpHEADMethod);
}

bool IsSchemeAndMethodSupported(const net::URLRequest* request) {
  return IsSchemeSupported(request->url()) &&
         IsMethodSupported(request->method());
}

}  // namespace appcache
