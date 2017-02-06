// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CONSTANTS_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CONSTANTS_H_

#include "base/files/file_path.h"

namespace history {

// filenames
extern const base::FilePath::CharType kFaviconsFilename[];
extern const base::FilePath::CharType kHistoryFilename[];
extern const base::FilePath::CharType kTopSitesFilename[];

// The maximum size of the list returned by history::HistoryService::TopHosts().
extern const int kMaxTopHosts;

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CONSTANTS_H_
