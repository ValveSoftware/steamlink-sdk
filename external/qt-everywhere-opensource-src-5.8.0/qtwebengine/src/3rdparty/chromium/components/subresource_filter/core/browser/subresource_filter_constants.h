// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_CONSTANTS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_CONSTANTS_H_

#include "base/files/file_path.h"

namespace subresource_filter {

// The name of the directory under the user data directory, where filtering
// rules are stored.
extern const base::FilePath::CharType kRulesetBaseDirectoryName[];

// The name of the file that actually stores the ruleset contents.
extern const base::FilePath::CharType kRulesetDataFileName[];

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_CONSTANTS_H_
