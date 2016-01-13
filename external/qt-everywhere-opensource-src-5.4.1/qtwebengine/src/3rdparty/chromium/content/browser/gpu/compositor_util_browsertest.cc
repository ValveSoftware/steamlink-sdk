// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/compositor_util.h"
#include "content/public/test/content_browser_test.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#elif defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

typedef ContentBrowserTest CompositorUtilTest;

// Test that threaded compositing and FCM are in the expected mode on the bots
// for all platforms.
IN_PROC_BROWSER_TEST_F(CompositorUtilTest, CompositingModeAsExpected) {
  enum CompositingMode {
    DISABLED,
    ENABLED,
    THREADED,  // Implies FCM
    DELEGATED,  // Implies threaded
  } expected_mode = DISABLED;
#if defined(USE_AURA)
  expected_mode = DELEGATED;
#elif defined(OS_ANDROID)
  expected_mode = DELEGATED;
#elif defined(OS_MACOSX)
  expected_mode = THREADED;
  // Lion and SnowLeopard have compositing blacklisted when using the Apple
  // software renderer, so results will vary depending if this test is being
  // run in a VM versus actual hardware.
  // http://crbug.com/230931
  if (base::mac::IsOSLionOrEarlier())
    return;
#elif defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_VISTA)
    expected_mode = THREADED;
#endif

  EXPECT_EQ(expected_mode == THREADED ||
            expected_mode == DELEGATED,
            IsThreadedCompositingEnabled());
  EXPECT_EQ(expected_mode == DELEGATED,
            IsDelegatedRendererEnabled());
}

}
