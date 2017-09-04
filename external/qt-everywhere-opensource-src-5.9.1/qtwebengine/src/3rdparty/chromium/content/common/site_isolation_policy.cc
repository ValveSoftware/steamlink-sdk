// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/site_isolation_policy.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {

// static
bool SiteIsolationPolicy::AreCrossProcessFramesPossible() {
  return UseDedicatedProcessesForAllSites() ||
         IsTopDocumentIsolationEnabled() ||
         GetContentClient()->IsSupplementarySiteIsolationModeEnabled() ||
         base::FeatureList::IsEnabled(::features::kGuestViewCrossProcessFrames);
}

// static
bool SiteIsolationPolicy::UseDedicatedProcessesForAllSites() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSitePerProcess);
}

// static
bool SiteIsolationPolicy::IsTopDocumentIsolationEnabled() {
  // --site-per-process trumps --top-document-isolation.
  if (UseDedicatedProcessesForAllSites())
    return false;

  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kTopDocumentIsolation);
}

// static
bool SiteIsolationPolicy::UseSubframeNavigationEntries() {
  return true;
}

}  // namespace content
