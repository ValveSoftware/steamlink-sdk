// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_utils.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"
#include "url/gurl.h"

namespace content {

// static
bool ServiceWorkerUtils::IsFeatureEnabled() {
  static bool enabled = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableServiceWorker);
  return enabled;
}

// static
bool ServiceWorkerUtils::ScopeMatches(const GURL& scope, const GURL& url) {
  DCHECK(!scope.has_ref());
  DCHECK(!url.has_ref());
  const std::string& scope_spec = scope.spec();
  const std::string& url_spec = url.spec();

  size_t len = scope_spec.size();
  if (len > 0 && scope_spec[len - 1] == '*')
    return scope_spec.compare(0, len - 1, url_spec, 0, len - 1) == 0;
  return scope_spec == url_spec;
}

bool LongestScopeMatcher::MatchLongest(const GURL& scope) {
  if (!ServiceWorkerUtils::ScopeMatches(scope, url_))
    return false;
  if (match_.is_empty()) {
    match_ = scope;
    return true;
  }

  const std::string match_spec = match_.spec();
  const std::string scope_spec = scope.spec();
  if (match_spec.size() < scope_spec.size()) {
    match_ = scope;
    return true;
  }

  // If |scope| has the same length with |match_|, they are compared as strings.
  // For example:
  //   1) for a document "/foo", "/foo" is prioritized over "/fo*".
  //   2) for a document "/f(1)", "/f(1*" is prioritized over "/f(1)".
  // TODO(nhiroki): This isn't in the spec.
  // (https://github.com/slightlyoff/ServiceWorker/issues/287)
  if (match_spec.size() == scope_spec.size() && match_spec < scope_spec) {
    match_ = scope;
    return true;
  }

  return false;
}

}  // namespace content
