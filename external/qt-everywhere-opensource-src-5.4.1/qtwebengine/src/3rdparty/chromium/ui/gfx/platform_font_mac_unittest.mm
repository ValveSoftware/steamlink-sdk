// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "ui/gfx/font.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PlatformFontMacTest, DeriveFont) {
  // Use a base font that support all traits.
  gfx::Font base_font("Helvetica", 13);

  // Bold
  gfx::Font bold_font(base_font.Derive(0, gfx::Font::BOLD));
  NSFontTraitMask traits = [[NSFontManager sharedFontManager]
      traitsOfFont:bold_font.GetNativeFont()];
  EXPECT_EQ(NSBoldFontMask, traits);

  // Italic
  gfx::Font italic_font(base_font.Derive(0, gfx::Font::ITALIC));
  traits = [[NSFontManager sharedFontManager]
      traitsOfFont:italic_font.GetNativeFont()];
  EXPECT_EQ(NSItalicFontMask, traits);

  // Bold italic
  gfx::Font bold_italic_font(base_font.Derive(
      0, gfx::Font::BOLD | gfx::Font::ITALIC));
  traits = [[NSFontManager sharedFontManager]
      traitsOfFont:bold_italic_font.GetNativeFont()];
  EXPECT_EQ(static_cast<NSFontTraitMask>(NSBoldFontMask | NSItalicFontMask),
            traits);
}

TEST(PlatformFontMacTest, ConstructFromNativeFont) {
  gfx::Font normal_font([NSFont fontWithName:@"Helvetica" size:12]);
  EXPECT_EQ(12, normal_font.GetFontSize());
  EXPECT_EQ("Helvetica", normal_font.GetFontName());
  EXPECT_EQ(gfx::Font::NORMAL, normal_font.GetStyle());

  gfx::Font bold_font([NSFont fontWithName:@"Helvetica-Bold" size:14]);
  EXPECT_EQ(14, bold_font.GetFontSize());
  EXPECT_EQ("Helvetica", bold_font.GetFontName());
  EXPECT_EQ(gfx::Font::BOLD, bold_font.GetStyle());

  gfx::Font italic_font([NSFont fontWithName:@"Helvetica-Oblique" size:14]);
  EXPECT_EQ(14, italic_font.GetFontSize());
  EXPECT_EQ("Helvetica", italic_font.GetFontName());
  EXPECT_EQ(gfx::Font::ITALIC, italic_font.GetStyle());

  gfx::Font bold_italic_font(
      [NSFont fontWithName:@"Helvetica-BoldOblique" size:14]);
  EXPECT_EQ(14, bold_italic_font.GetFontSize());
  EXPECT_EQ("Helvetica", bold_italic_font.GetFontName());
  EXPECT_EQ(gfx::Font::BOLD | gfx::Font::ITALIC, bold_italic_font.GetStyle());
}
