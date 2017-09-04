// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_CONSTANTS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_CONSTANTS_H_

#include "base/files/file_path.h"

namespace subresource_filter {

// The name of the top-level directory under the user data directory that
// contains all files and subdirectories related to the subresource filter.
extern const base::FilePath::CharType kTopLevelDirectoryName[];

// Paths under |kTopLevelDirectoryName|
// ------------------------------------

// The name of the subdirectory under the top-level directory that stores
// versions of indexed rulesets. Files that belong to an IndexedRulesetVersion
// are stored under /format_version/content_version/.
extern const base::FilePath::CharType kIndexedRulesetBaseDirectoryName[];

// The name of the subdirectory under the top-level directory that stores
// versions of unindexed rulesets downloaded through the component updater.
extern const base::FilePath::CharType kUnindexedRulesetBaseDirectoryName[];

// Paths under IndexedRulesetVersion::GetSubdirectoryPathForVersion
// ----------------------------------------------------------------

// The name of the file that actually stores the ruleset contents.
extern const base::FilePath::CharType kRulesetDataFileName[];

// The name of the applicable license file, if any, stored next to the ruleset.
extern const base::FilePath::CharType kLicenseFileName[];

// The name of the sentinel file that is temporarily stored to indicate that the
// ruleset is being indexed.
extern const base::FilePath::CharType kSentinelFileName[];

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_CONSTANTS_H_
