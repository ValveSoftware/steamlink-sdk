// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_LEAK_DETECTION_RESULT_H_
#define CONTENT_SHELL_COMMON_LEAK_DETECTION_RESULT_H_

#include <string>

namespace content {

struct LeakDetectionResult {
  bool leaked;
  std::string detail;
};

}  // namespace content

#endif  // CONTENT_SHELL_COMMON_LEAK_DETECTION_RESULT_H_
