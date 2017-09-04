// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_FEATURE_LIST_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_FEATURE_LIST_H_

namespace safe_browsing {

// Exposes methods to check whether a particular feature has been enabled
// through Finch.
namespace V4FeatureList {

// Is the PVer4 database manager enabled? Should be true if either of those
// below are true.
bool IsLocalDatabaseManagerEnabled();

// Is the PVer4 database being checked for resource reputation? If this returns
// true, use PVer4 database for CheckBrowseUrl, otherwise use PVer3.
bool IsV4HybridEnabled();

// Is only the PVer4 database being checked for resource reputation? If this
// returns true, use PVer4 database for all SafeBrowsing operations, and don't
// update the PVer3 database at all.  This is the launch configuration.
bool IsV4OnlyEnabled();

}  // namespace V4FeatureList

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_FEATURE_LIST_H_
