// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_list.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helper function for comparing fonts for equality.
std::string FontToString(const gfx::Font& font) {
  std::string font_string = font.GetFontName();
  font_string += "|";
  font_string += base::IntToString(font.GetFontSize());
  int style = font.GetStyle();
  if (style & gfx::Font::BOLD)
    font_string += "|bold";
  if (style & gfx::Font::ITALIC)
    font_string += "|italic";
  if (style & gfx::Font::UNDERLINE)
    font_string += "|underline";
  return font_string;
}

}  // namespace

namespace gfx {

TEST(FontListTest, FontDescString_FromDescString) {
  // Test init from font name style size string.
  FontList font_list = FontList("Droid Sans serif, Sans serif, 10px");
  EXPECT_EQ("Droid Sans serif, Sans serif, 10px",
            font_list.GetFontDescriptionString());
}

TEST(FontListTest, FontDescString_FromFontNamesStyleAndSize) {
  // Test init from font names, style and size.
  std::vector<std::string> font_names;
  font_names.push_back("Arial");
  font_names.push_back("Droid Sans serif");
  int font_style = Font::BOLD | Font::ITALIC | Font::UNDERLINE;
  int font_size = 11;
  FontList font_list = FontList(font_names, font_style, font_size);
  // "Underline" doesn't appear in the font description string.
  EXPECT_EQ("Arial,Droid Sans serif,Bold Italic 11px",
            font_list.GetFontDescriptionString());
}

TEST(FontListTest, FontDescString_FromFont) {
  // Test init from Font.
  Font font("Arial", 8);
  FontList font_list = FontList(font);
  EXPECT_EQ("Arial,8px", font_list.GetFontDescriptionString());
}

TEST(FontListTest, FontDescString_FromFontWithNonNormalStyle) {
  // Test init from Font with non-normal style.
  Font font("Arial", 8);
  FontList font_list = FontList(font.Derive(2, Font::BOLD));
  EXPECT_EQ("Arial,Bold 10px", font_list.GetFontDescriptionString());

  font_list = FontList(font.Derive(-2, Font::ITALIC));
  EXPECT_EQ("Arial,Italic 6px", font_list.GetFontDescriptionString());

  // "Underline" doesn't appear in the font description string.
  font_list = FontList(font.Derive(-4, Font::UNDERLINE));
  EXPECT_EQ("Arial,4px", font_list.GetFontDescriptionString());
}

TEST(FontListTest, FontDescString_FromFontVector) {
  // Test init from Font vector.
  Font font("Arial", 8);
  Font font_1("Sans serif", 10);
  std::vector<Font> fonts;
  fonts.push_back(font.Derive(0, Font::BOLD));
  fonts.push_back(font_1.Derive(-2, Font::BOLD));
  FontList font_list = FontList(fonts);
  EXPECT_EQ("Arial,Sans serif,Bold 8px", font_list.GetFontDescriptionString());
}

TEST(FontListTest, Fonts_FromDescString) {
  // Test init from font name size string.
  FontList font_list = FontList("serif,Sans serif, 13px");
  const std::vector<Font>& fonts = font_list.GetFonts();
  EXPECT_EQ(2U, fonts.size());
  EXPECT_EQ("serif|13", FontToString(fonts[0]));
  EXPECT_EQ("Sans serif|13", FontToString(fonts[1]));
}

TEST(FontListTest, Fonts_FromDescStringInFlexibleFormat) {
  // Test init from font name size string with flexible format.
  FontList font_list = FontList("  serif   ,   Sans serif ,   13px");
  const std::vector<Font>& fonts = font_list.GetFonts();
  EXPECT_EQ(2U, fonts.size());
  EXPECT_EQ("serif|13", FontToString(fonts[0]));
  EXPECT_EQ("Sans serif|13", FontToString(fonts[1]));
}

TEST(FontListTest, Fonts_FromDescStringWithStyleInFlexibleFormat) {
  // Test init from font name style size string with flexible format.
  FontList font_list = FontList("  serif  ,  Sans serif ,  Bold   "
                                "  Italic   13px");
  const std::vector<Font>& fonts = font_list.GetFonts();
  EXPECT_EQ(2U, fonts.size());
  EXPECT_EQ("serif|13|bold|italic", FontToString(fonts[0]));
  EXPECT_EQ("Sans serif|13|bold|italic", FontToString(fonts[1]));
}

TEST(FontListTest, Fonts_FromFont) {
  // Test init from Font.
  Font font("Arial", 8);
  FontList font_list = FontList(font);
  const std::vector<Font>& fonts = font_list.GetFonts();
  EXPECT_EQ(1U, fonts.size());
  EXPECT_EQ("Arial|8", FontToString(fonts[0]));
}

TEST(FontListTest, Fonts_FromFontWithNonNormalStyle) {
  // Test init from Font with non-normal style.
  Font font("Arial", 8);
  FontList font_list = FontList(font.Derive(2, Font::BOLD));
  std::vector<Font> fonts = font_list.GetFonts();
  EXPECT_EQ(1U, fonts.size());
  EXPECT_EQ("Arial|10|bold", FontToString(fonts[0]));

  font_list = FontList(font.Derive(-2, Font::ITALIC));
  fonts = font_list.GetFonts();
  EXPECT_EQ(1U, fonts.size());
  EXPECT_EQ("Arial|6|italic", FontToString(fonts[0]));
}

TEST(FontListTest, Fonts_FromFontVector) {
  // Test init from Font vector.
  Font font("Arial", 8);
  Font font_1("Sans serif", 10);
  std::vector<Font> input_fonts;
  input_fonts.push_back(font.Derive(0, Font::BOLD));
  input_fonts.push_back(font_1.Derive(-2, Font::BOLD));
  FontList font_list = FontList(input_fonts);
  const std::vector<Font>& fonts = font_list.GetFonts();
  EXPECT_EQ(2U, fonts.size());
  EXPECT_EQ("Arial|8|bold", FontToString(fonts[0]));
  EXPECT_EQ("Sans serif|8|bold", FontToString(fonts[1]));
}

TEST(FontListTest, Fonts_DescStringWithStyleInFlexibleFormat_RoundTrip) {
  // Test round trip from font description string to font vector to
  // font description string.
  FontList font_list = FontList("  serif  ,  Sans serif ,  Bold   "
                                "  Italic   13px");

  const std::vector<Font>& fonts = font_list.GetFonts();
  FontList font_list_1 = FontList(fonts);
  const std::string& desc_str = font_list_1.GetFontDescriptionString();

  EXPECT_EQ("serif,Sans serif,Bold Italic 13px", desc_str);
}

TEST(FontListTest, Fonts_FontVector_RoundTrip) {
  // Test round trip from font vector to font description string to font vector.
  Font font("Arial", 8);
  Font font_1("Sans serif", 10);
  std::vector<Font> input_fonts;
  input_fonts.push_back(font.Derive(0, Font::BOLD));
  input_fonts.push_back(font_1.Derive(-2, Font::BOLD));
  FontList font_list = FontList(input_fonts);

  const std::string& desc_string = font_list.GetFontDescriptionString();
  FontList font_list_1 = FontList(desc_string);
  const std::vector<Font>& round_trip_fonts = font_list_1.GetFonts();

  EXPECT_EQ(2U, round_trip_fonts.size());
  EXPECT_EQ("Arial|8|bold", FontToString(round_trip_fonts[0]));
  EXPECT_EQ("Sans serif|8|bold", FontToString(round_trip_fonts[1]));
}

TEST(FontListTest, FontDescString_GetStyle) {
  FontList font_list = FontList("Arial,Sans serif, 8px");
  EXPECT_EQ(Font::NORMAL, font_list.GetFontStyle());

  font_list = FontList("Arial,Sans serif,Bold 8px");
  EXPECT_EQ(Font::BOLD, font_list.GetFontStyle());

  font_list = FontList("Arial,Sans serif,Italic 8px");
  EXPECT_EQ(Font::ITALIC, font_list.GetFontStyle());

  font_list = FontList("Arial,Italic Bold 8px");
  EXPECT_EQ(Font::BOLD | Font::ITALIC, font_list.GetFontStyle());
}

TEST(FontListTest, Fonts_GetStyle) {
  std::vector<Font> fonts;
  fonts.push_back(gfx::Font("Arial", 8));
  fonts.push_back(gfx::Font("Sans serif", 8));
  FontList font_list = FontList(fonts);
  EXPECT_EQ(Font::NORMAL, font_list.GetFontStyle());
  fonts[0] = fonts[0].Derive(0, Font::ITALIC | Font::BOLD);
  fonts[1] = fonts[1].Derive(0, Font::ITALIC | Font::BOLD);
  font_list = FontList(fonts);
  EXPECT_EQ(Font::ITALIC | Font::BOLD, font_list.GetFontStyle());
}

TEST(FontListTest, FontDescString_Derive) {
  FontList font_list = FontList("Arial,Sans serif,Bold Italic 8px");

  FontList derived = font_list.Derive(10, Font::ITALIC | Font::UNDERLINE);
  EXPECT_EQ("Arial,Sans serif,Italic 18px", derived.GetFontDescriptionString());
  EXPECT_EQ(Font::ITALIC | Font::UNDERLINE, derived.GetFontStyle());

  // FontList has a special case for Font::UNDERLINE.  See if the handling of
  // Font::UNDERLINE in GetFonts() is okay or not.
  derived.GetFonts();
  EXPECT_EQ(Font::ITALIC | Font::UNDERLINE, derived.GetFontStyle());
}

TEST(FontListTest, Fonts_Derive) {
  std::vector<Font> fonts;
  fonts.push_back(gfx::Font("Arial", 8));
  fonts.push_back(gfx::Font("Sans serif", 8));
  FontList font_list = FontList(fonts);

  FontList derived = font_list.Derive(5, Font::BOLD | Font::UNDERLINE);
  const std::vector<Font>& derived_fonts = derived.GetFonts();

  EXPECT_EQ(2U, derived_fonts.size());
  EXPECT_EQ("Arial|13|bold|underline", FontToString(derived_fonts[0]));
  EXPECT_EQ("Sans serif|13|bold|underline", FontToString(derived_fonts[1]));
}

TEST(FontListTest, FontDescString_DeriveWithSizeDelta) {
  FontList font_list = FontList("Arial,Sans serif,Bold 18px");

  FontList derived = font_list.DeriveWithSizeDelta(-8);
  EXPECT_EQ("Arial,Sans serif,Bold 10px",
            derived.GetFontDescriptionString());
}

TEST(FontListTest, Fonts_DeriveWithSizeDelta) {
  std::vector<Font> fonts;
  fonts.push_back(gfx::Font("Arial", 18).Derive(0, Font::ITALIC));
  fonts.push_back(gfx::Font("Sans serif", 18).Derive(0, Font::ITALIC));
  FontList font_list = FontList(fonts);

  FontList derived = font_list.DeriveWithSizeDelta(-5);
  const std::vector<Font>& derived_fonts = derived.GetFonts();

  EXPECT_EQ(2U, derived_fonts.size());
  EXPECT_EQ("Arial|13|italic", FontToString(derived_fonts[0]));
  EXPECT_EQ("Sans serif|13|italic", FontToString(derived_fonts[1]));
}

TEST(FontListTest, Fonts_GetHeight_GetBaseline) {
  // If a font list has only one font, the height and baseline must be the same.
  Font font1("Arial", 16);
  ASSERT_EQ("arial", StringToLowerASCII(font1.GetActualFontNameForTesting()));
  FontList font_list1("Arial, 16px");
  EXPECT_EQ(font1.GetHeight(), font_list1.GetHeight());
  EXPECT_EQ(font1.GetBaseline(), font_list1.GetBaseline());

  // If there are two different fonts, the font list returns the max value
  // for ascent and descent.
  Font font2("Symbol", 16);
  ASSERT_EQ("symbol", StringToLowerASCII(font2.GetActualFontNameForTesting()));
  EXPECT_NE(font1.GetBaseline(), font2.GetBaseline());
  EXPECT_NE(font1.GetHeight() - font1.GetBaseline(),
            font2.GetHeight() - font2.GetBaseline());
  std::vector<Font> fonts;
  fonts.push_back(font1);
  fonts.push_back(font2);
  FontList font_list_mix(fonts);
  // ascent of FontList == max(ascent of Fonts)
  EXPECT_EQ(std::max(font1.GetHeight() - font1.GetBaseline(),
                     font2.GetHeight() - font2.GetBaseline()),
            font_list_mix.GetHeight() - font_list_mix.GetBaseline());
  // descent of FontList == max(descent of Fonts)
  EXPECT_EQ(std::max(font1.GetBaseline(), font2.GetBaseline()),
            font_list_mix.GetBaseline());
}

}  // namespace gfx
