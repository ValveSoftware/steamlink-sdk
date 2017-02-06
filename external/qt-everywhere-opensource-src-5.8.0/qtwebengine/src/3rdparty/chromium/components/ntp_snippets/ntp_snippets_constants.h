// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONSTANTS_H_
#define COMPONENTS_NTP_SNIPPETS_CONSTANTS_H_

#include "base/files/file_path.h"

namespace ntp_snippets {

// Name of the variation parameters study to configure NTP snippets.
extern const char kStudyName[];

// Name of the folder where the snippets database should be stored. This is only
// the name of the folder, not a full path - it must be appended to e.g. the
// profile path.
extern const base::FilePath::CharType kDatabaseFolder[];

}  // namespace ntp_snippets

#endif
