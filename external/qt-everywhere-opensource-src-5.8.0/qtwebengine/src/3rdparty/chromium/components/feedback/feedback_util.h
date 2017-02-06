// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_UTIL_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "base/sys_info.h"
#elif defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

class Profile;

namespace content {
class WebContents;
}

namespace chrome {
extern const char kAppLauncherCategoryTag[];
}  // namespace chrome

namespace feedback {
class FeedbackData;
}

namespace feedback_util {

  void SendReport(scoped_refptr<feedback::FeedbackData> data);
  bool ZipString(const base::FilePath& filename,
                 const std::string& data, std::string* compressed_data);

}  // namespace feedback_util

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_UTIL_H_
