// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_utils.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/public/common/origin_util.h"

namespace content {

namespace {

bool PathContainsDisallowedCharacter(const GURL& url) {
  std::string path = url.path();
  DCHECK(base::IsStringUTF8(path));

  // We should avoid these escaped characters in the path component because
  // these can be handled differently depending on server implementation.
  if (path.find("%2f") != std::string::npos ||
      path.find("%2F") != std::string::npos) {
    return true;
  }
  if (path.find("%5c") != std::string::npos ||
      path.find("%5C") != std::string::npos) {
    return true;
  }
  return false;
}

bool AllOriginsMatch(const GURL& url_a, const GURL& url_b, const GURL& url_c) {
  return url_a.GetOrigin() == url_b.GetOrigin() &&
         url_a.GetOrigin() == url_c.GetOrigin();
}

}  // namespace

// static
bool ServiceWorkerUtils::ScopeMatches(const GURL& scope, const GURL& url) {
  DCHECK(!scope.has_ref());
  return base::StartsWith(url.spec(), scope.spec(),
                          base::CompareCase::SENSITIVE);
}

// static
bool ServiceWorkerUtils::IsPathRestrictionSatisfied(
    const GURL& scope,
    const GURL& script_url,
    const std::string* service_worker_allowed_header_value,
    std::string* error_message) {
  DCHECK(scope.is_valid());
  DCHECK(!scope.has_ref());
  DCHECK(script_url.is_valid());
  DCHECK(!script_url.has_ref());
  DCHECK(error_message);

  if (ContainsDisallowedCharacter(scope, script_url, error_message))
    return false;

  std::string max_scope_string;
  if (service_worker_allowed_header_value) {
    GURL max_scope = script_url.Resolve(*service_worker_allowed_header_value);
    if (!max_scope.is_valid()) {
      *error_message = "An invalid Service-Worker-Allowed header value ('";
      error_message->append(*service_worker_allowed_header_value);
      error_message->append("') was received when fetching the script.");
      return false;
    }
    max_scope_string = max_scope.path();
  } else {
    max_scope_string = script_url.Resolve(".").path();
  }

  std::string scope_string = scope.path();
  if (!base::StartsWith(scope_string, max_scope_string,
                        base::CompareCase::SENSITIVE)) {
    *error_message = "The path of the provided scope ('";
    error_message->append(scope_string);
    error_message->append("') is not under the max scope allowed (");
    if (service_worker_allowed_header_value)
      error_message->append("set by Service-Worker-Allowed: ");
    error_message->append("'");
    error_message->append(max_scope_string);
    error_message->append(
        "'). Adjust the scope, move the Service Worker script, or use the "
        "Service-Worker-Allowed HTTP header to allow the scope.");
    return false;
  }
  return true;
}

// static
bool ServiceWorkerUtils::ContainsDisallowedCharacter(
    const GURL& scope,
    const GURL& script_url,
    std::string* error_message) {
  if (PathContainsDisallowedCharacter(scope) ||
      PathContainsDisallowedCharacter(script_url)) {
    *error_message = "The provided scope ('";
    error_message->append(scope.spec());
    error_message->append("') or scriptURL ('");
    error_message->append(script_url.spec());
    error_message->append("') includes a disallowed escape character.");
    return true;
  }
  return false;
}

// static
bool ServiceWorkerUtils::CanRegisterServiceWorker(const GURL& context_url,
                                                  const GURL& pattern,
                                                  const GURL& script_url) {
  DCHECK(context_url.is_valid());
  DCHECK(pattern.is_valid());
  DCHECK(script_url.is_valid());
  return AllOriginsMatch(context_url, pattern, script_url) &&
         OriginCanAccessServiceWorkers(context_url) &&
         OriginCanAccessServiceWorkers(pattern) &&
         OriginCanAccessServiceWorkers(script_url);
}

bool LongestScopeMatcher::MatchLongest(const GURL& scope) {
  if (!ServiceWorkerUtils::ScopeMatches(scope, url_))
    return false;
  if (match_.is_empty() || match_.spec().size() < scope.spec().size()) {
    match_ = scope;
    return true;
  }
  return false;
}

}  // namespace content
