// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_win.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"

namespace gfx {

namespace {

// Returns a font based on |base_font| with height at most |target_height| and
// font size maximized. Returns |base_font| if height is already equal.
gfx::Font AdjustFontSizeForHeight(const gfx::Font& base_font,
                                  int target_height) {
  Font expected_font = base_font;
  if (base_font.GetHeight() < target_height) {
    // Increase size while height is <= |target_height|.
    Font larger_font = base_font.Derive(1, 0);
    while (larger_font.GetHeight() <= target_height) {
      expected_font = larger_font;
      larger_font = larger_font.Derive(1, 0);
    }
  } else if (expected_font.GetHeight() > target_height) {
    // Decrease size until height is <= |target_height|.
    do {
      expected_font = expected_font.Derive(-1, 0);
    } while (expected_font.GetHeight() > target_height);
  }
  return expected_font;
}

}  // namespace

TEST(PlatformFontWinTest, DeriveFontWithHeight) {
  const Font base_font;
  PlatformFontWin* platform_font =
      static_cast<PlatformFontWin*>(base_font.platform_font());

  for (int i = -10; i < 10; i++) {
    const int target_height = base_font.GetHeight() + i;
    Font expected_font = AdjustFontSizeForHeight(base_font, target_height);
    ASSERT_LE(expected_font.GetHeight(), target_height);

    Font derived_font = platform_font->DeriveFontWithHeight(target_height, 0);
    EXPECT_EQ(expected_font.GetFontName(), derived_font.GetFontName());
    EXPECT_EQ(expected_font.GetFontSize(), derived_font.GetFontSize());
    EXPECT_LE(expected_font.GetHeight(), target_height);
    EXPECT_EQ(0, derived_font.GetStyle());

    derived_font = platform_font->DeriveFontWithHeight(target_height,
                                                       Font::BOLD);
    EXPECT_EQ(expected_font.GetFontName(), derived_font.GetFontName());
    EXPECT_EQ(expected_font.GetFontSize(), derived_font.GetFontSize());
    EXPECT_LE(expected_font.GetHeight(), target_height);
    EXPECT_EQ(Font::BOLD, derived_font.GetStyle());

    // Test that deriving from the new font has the expected result.
    Font rederived_font = derived_font.Derive(1, 0);
    expected_font = Font(derived_font.GetFontName(),
                         derived_font.GetFontSize() + 1);
    EXPECT_EQ(expected_font.GetFontName(), rederived_font.GetFontName());
    EXPECT_EQ(expected_font.GetFontSize(), rederived_font.GetFontSize());
    EXPECT_EQ(expected_font.GetHeight(), rederived_font.GetHeight());
  }
}

// Callback function used by DeriveFontWithHeight_MinSize() below.
static int GetMinFontSize() {
  return 10;
}

TEST(PlatformFontWinTest, DeriveFontWithHeight_MinSize) {
  PlatformFontWin::GetMinimumFontSizeCallback old_callback =
    PlatformFontWin::get_minimum_font_size_callback;
  PlatformFontWin::get_minimum_font_size_callback = &GetMinFontSize;

  const Font base_font;
  const Font min_font(base_font.GetFontName(), GetMinFontSize());
  PlatformFontWin* platform_font =
      static_cast<PlatformFontWin*>(base_font.platform_font());

  const Font derived_font =
      platform_font->DeriveFontWithHeight(min_font.GetHeight() - 1, 0);
  EXPECT_EQ(min_font.GetFontSize(), derived_font.GetFontSize());
  EXPECT_EQ(min_font.GetHeight(), derived_font.GetHeight());

  PlatformFontWin::get_minimum_font_size_callback = old_callback;
}

TEST(PlatformFontWinTest, DeriveFontWithHeight_TooSmall) {
  const Font base_font;
  PlatformFontWin* platform_font =
      static_cast<PlatformFontWin*>(base_font.platform_font());

  const Font derived_font = platform_font->DeriveFontWithHeight(1, 0);
  EXPECT_GT(derived_font.GetHeight(), 1);
}

}  // namespace gfx
