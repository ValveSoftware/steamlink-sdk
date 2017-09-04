// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util_win.h"

#include <windows.h>

#include "base/win/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

typedef PlatformTest L10nUtilWinTest;

TEST_F(L10nUtilWinTest, TestDPIScaling) {
  // Baseline font for comparison.
  NONCLIENTMETRICS_XP metrics;
  base::win::GetNonClientMetrics(&metrics);
  LOGFONT lf = metrics.lfMessageFont;
  l10n_util::AdjustUIFont(&lf);
  int size = lf.lfHeight;
  float rounding = size < 0 ? -0.5f : 0.5f;

  // Test that font size is properly normalized for DIP. In high-DPI mode, the
  // font metrics are scaled based on the DPI scale factor. For Windows 8, 140%
  // and 180% font scaling are supported. Simulate size normalization for a DPI-
  // aware process by manually scaling up the font and checking that it returns
  // to the expected size.
  lf.lfHeight = static_cast<int>(1.4 * size + rounding);
  l10n_util::AdjustUIFontForDIP(1.4f, &lf);
  EXPECT_NEAR(size, lf.lfHeight, 1);

  lf.lfHeight = static_cast<int>(1.8 * size + rounding);
  l10n_util::AdjustUIFontForDIP(1.8f, &lf);
  EXPECT_NEAR(size, lf.lfHeight, 1);
}
