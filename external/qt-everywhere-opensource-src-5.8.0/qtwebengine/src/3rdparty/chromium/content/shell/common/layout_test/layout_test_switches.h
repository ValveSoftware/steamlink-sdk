// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "layout_test" command-line switches.

#ifndef CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_SWITCHES_H_
#define CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_SWITCHES_H_

#include <string>
#include <vector>

namespace switches {

extern const char kAllowExternalPages[];
extern const char kCheckLayoutTestSysDeps[];
extern const char kCrashOnFailure[];
extern const char kEnableAccelerated2DCanvas[];
extern const char kEnableFontAntialiasing[];
extern const char kAlwaysUseComplexText[];
extern const char kEnableLeakDetection[];
extern const char kEncodeBinary[];
extern const char kRunLayoutTest[];
extern const char kStableReleaseMode[];

}  // namespace switches

#endif  // CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_SWITCHES_H_
