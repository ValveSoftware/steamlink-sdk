// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SITE_ISOLATION_POLICY_H_
#define CONTENT_COMMON_SITE_ISOLATION_POLICY_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// A centralized place for making policy decisions about out-of-process iframes,
// site isolation, --site-per-process, and related features.
//
// This is currently static because all these modes are controlled by command-
// line flags.
//
// These methods can be called from any thread.
class CONTENT_EXPORT SiteIsolationPolicy {
 public:
  // Returns true if the current process model might allow the use of cross-
  // process iframes. This should typically used to avoid executing codepaths
  // that only matter for cross-process iframes, to protect the default
  // behavior.
  //
  // Note: Since cross-process frames will soon be possible by default, usage
  // should be limited to temporary stop-gaps.
  //
  // Instead of calling this method, prefer to examine object state to see
  // whether a particular frame happens to have a cross-process relationship
  // with another, or to consult DoesSiteRequireDedicatedProcess() to see if a
  // particular site merits protection.
  static bool AreCrossProcessFramesPossible();

  // Returns true if every site should be placed in a dedicated process.
  static bool UseDedicatedProcessesForAllSites();

  // Returns true if third-party subframes of a page should be kept in a
  // different process from the main frame.
  static bool IsTopDocumentIsolationEnabled();

  // Returns true if navigation and history code should maintain per-frame
  // navigation entries. This is an in-progress feature related to site
  // isolation, so the return value is currently tied to --site-per-process.
  // TODO(creis, avi): Make this the default, and eliminate this.
  static bool UseSubframeNavigationEntries();

 private:
  SiteIsolationPolicy();  // Not instantiable.

  DISALLOW_COPY_AND_ASSIGN(SiteIsolationPolicy);
};

}  // namespace content

#endif  // CONTENT_COMMON_SITE_ISOLATION_POLICY_H_
