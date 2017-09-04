// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/fallback_icon_url_parser.h"

#include <stddef.h>

#include "base/macros.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

using chrome::ParsedFallbackIconPath;
using favicon_base::FallbackIconStyle;

namespace chrome {

namespace {

// Default values for FallbackIconStyle, from
// /components/favicon_base/fallback_icon_style.h
const SkColor kDefaultBackgroundColor = SkColorSetRGB(0x78, 0x78, 0x78);
const SkColor kDefaultTextColorDark = SK_ColorBLACK;
const SkColor kDefaultTextColorLight = SK_ColorWHITE;
const double kDefaultFontSizeRatio = 0.44;
const double kDefaultRoundness = 0;

const char kTestUrlStr[] = "https://www.google.ca/imghp?hl=en&tab=wi";

}  // namespace

class FallbackIconUrlParserTest : public testing::Test {
 public:
  FallbackIconUrlParserTest() {
  }

  bool ParseSpecs(const std::string& specs_str,
                  int *size,
                  favicon_base::FallbackIconStyle* style) {
    return ParsedFallbackIconPath::ParseSpecs(specs_str, size, style);
  }

  bool ParseColor(const std::string& color_str, SkColor* color) {
    return ParsedFallbackIconPath::ParseColor(color_str, color);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FallbackIconUrlParserTest);
};

TEST_F(FallbackIconUrlParserTest, ParseColorSuccess) {
  SkColor c;
  EXPECT_TRUE(ParseColor("31aBf0f4", &c));
  EXPECT_EQ(SkColorSetARGB(0x31, 0xAB, 0xF0, 0xF4), c);
  EXPECT_TRUE(ParseColor("01aBf0", &c));
  EXPECT_EQ(SkColorSetRGB(0x01, 0xAB, 0xF0), c);
  EXPECT_TRUE(ParseColor("501a", &c));
  EXPECT_EQ(SkColorSetARGB(0x55, 0x00, 0x11, 0xAA), c);
  EXPECT_TRUE(ParseColor("01a", &c));
  EXPECT_EQ(SkColorSetRGB(0x00, 0x11, 0xAA), c);
  EXPECT_TRUE(ParseColor("000000", &c));
  EXPECT_EQ(SkColorSetARGB(0xFF, 0x00, 0x00, 0x00), c);
  EXPECT_TRUE(ParseColor("red", &c));
  EXPECT_EQ(SkColorSetARGB(0xFF, 0xFF, 0x00, 0x00), c);
}

TEST_F(FallbackIconUrlParserTest, ParseColorFailure) {
  const char* test_cases[] = {
    "",
    "00000",
    "000000000",
    " 000000",
    "ABCDEFG",
    "#000",
    "#000000",
    "000000 ",
    "ABCDEFH",
    "#ABCDEF",
  };
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    SkColor c;
    EXPECT_FALSE(ParseColor(test_cases[i], &c))
         << "test_cases[" << i << "]";
  }
}

TEST_F(FallbackIconUrlParserTest, ParseSpecsEmpty) {
  int size;
  FallbackIconStyle style;
  EXPECT_TRUE(ParseSpecs(",,,,", &size, &style));
  EXPECT_EQ(16, size);
  EXPECT_EQ(kDefaultBackgroundColor, style.background_color);
  EXPECT_TRUE(style.is_default_background_color);
  EXPECT_EQ(kDefaultTextColorLight, style.text_color);
  EXPECT_EQ(kDefaultFontSizeRatio, style.font_size_ratio);
  EXPECT_EQ(kDefaultRoundness, style.roundness);
}

TEST_F(FallbackIconUrlParserTest, ParseSpecsPartial) {
  int size;
  FallbackIconStyle style;
  EXPECT_TRUE(ParseSpecs(",,aCE,,0.1", &size, &style));
  EXPECT_EQ(16, size);
  EXPECT_EQ(kDefaultBackgroundColor, style.background_color);
  EXPECT_TRUE(style.is_default_background_color);
  EXPECT_EQ(SkColorSetRGB(0xAA, 0xCC, 0xEE), style.text_color);
  EXPECT_EQ(kDefaultFontSizeRatio, style.font_size_ratio);
  EXPECT_EQ(0.1, style.roundness);
}

TEST_F(FallbackIconUrlParserTest, ParseSpecsFull) {
  int size;

  {
    FallbackIconStyle style;
    EXPECT_TRUE(ParseSpecs("16,000,f01,0.75,0.25", &size, &style));
    EXPECT_EQ(16, size);
    EXPECT_EQ(SkColorSetRGB(0x00, 0x00, 0x00), style.background_color);
    EXPECT_FALSE(style.is_default_background_color);
    EXPECT_EQ(SkColorSetRGB(0xff, 0x00, 0x11), style.text_color);
    EXPECT_EQ(0.75, style.font_size_ratio);
    EXPECT_EQ(0.25, style.roundness);
  }

  {
    FallbackIconStyle style;
    EXPECT_TRUE(ParseSpecs("48,black,123456,0.5,0.3", &size, &style));
    EXPECT_EQ(48, size);
    EXPECT_EQ(SkColorSetRGB(0x00, 0x00, 0x00), style.background_color);
    EXPECT_FALSE(style.is_default_background_color);
    EXPECT_EQ(SkColorSetRGB(0x12, 0x34, 0x56), style.text_color);
    EXPECT_EQ(0.5, style.font_size_ratio);
    EXPECT_EQ(0.3, style.roundness);
  }

  {
    FallbackIconStyle style;
    EXPECT_TRUE(ParseSpecs("1,000,red,0,0", &size, &style));
    EXPECT_EQ(1, size);
    EXPECT_EQ(SkColorSetRGB(0x00, 0x00, 0x00), style.background_color);
    EXPECT_FALSE(style.is_default_background_color);
    EXPECT_EQ(SkColorSetRGB(0xFF, 0x00, 0x00), style.text_color);
    EXPECT_EQ(0, style.font_size_ratio);
    EXPECT_EQ(0, style.roundness);
  }
}

TEST_F(FallbackIconUrlParserTest, ParseSpecsDefaultTextColor) {
  int size;

  {
    // Dark background -> Light text.
    FallbackIconStyle style;
    EXPECT_TRUE(ParseSpecs(",000,,,", &size, &style));
    EXPECT_EQ(kDefaultTextColorLight, style.text_color);
  }

  {
    // Light background -> Dark text.
    FallbackIconStyle style;
    EXPECT_TRUE(ParseSpecs(",fff,,,", &size, &style));
    EXPECT_EQ(kDefaultTextColorDark, style.text_color);
  }

  {
    // Light background -> Dark text, more params don't matter.
    FallbackIconStyle style;
    EXPECT_TRUE(ParseSpecs("107,fff,,0.3,0.5", &size, &style));
    EXPECT_EQ(kDefaultTextColorDark, style.text_color);
  }
}

TEST_F(FallbackIconUrlParserTest, ParseSpecsFailure) {
  const char* test_cases[] = {
    // Need exactly 5 params.
    "",
    "16",
    "16,black",
    "16,black,fff",
    "16,black,fff,0.75",
    ",,,"
    ",,,,,",
    "16,black,fff,0.75,0.25,junk",
    // Don't allow any space.
    "16,black,fff, 0.75,0.25",
    "16,black ,fff,0.75,0.25",
    "16,black,fff,0.75,0.25 ",
    // Adding junk text.
    "16,black,fff,0.75,0.25junk",
    "junk,black,fff,0.75,0.25",
    "16,#junk,fff,0.75,0.25",
    "16,black,junk,0.75,0.25",
    "16,black,fff,junk,0.25",
    "16,black,fff,0.75,junk",
    // Out of bound.
    "0,black,fff,0.75,0.25",  // size.
    "4294967296,black,fff,0.75,0.25",  // size.
    "-1,black,fff,0.75,0.25",  // size.
    "16,black,fff,-0.1,0.25",  // font_size_ratio.
    "16,black,fff,1.1,0.25",  // font_size_ratio.
    "16,black,fff,0.75,-0.1",  // roundness.
    "16,black,fff,0.75,1.1",  // roundness.
  };
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    int size;
    FallbackIconStyle style;
    EXPECT_FALSE(ParseSpecs(test_cases[i], &size, &style))
        << "test_cases[" << i << "]";

  }
}

TEST_F(FallbackIconUrlParserTest, ParseFallbackIconPathSuccess) {
  const std::string specs = "31,black,fff,0.75,0.25";

  // Everything populated.
  {
    chrome::ParsedFallbackIconPath parsed;
    EXPECT_TRUE(parsed.Parse(specs + "/" + kTestUrlStr));
    EXPECT_EQ(31, parsed.size_in_pixels());
    const favicon_base::FallbackIconStyle& style = parsed.style();
    EXPECT_EQ(SkColorSetRGB(0x00, 0x00, 0x00), style.background_color);
    EXPECT_FALSE(style.is_default_background_color);
    EXPECT_EQ(SkColorSetRGB(0xFF, 0xFF, 0xFF), style.text_color);
    EXPECT_EQ(0.75, style.font_size_ratio);
    EXPECT_EQ(0.25, style.roundness);
    EXPECT_EQ(GURL(kTestUrlStr), GURL(parsed.url_string()));
    EXPECT_EQ(specs.length() + 1, parsed.path_index());
  }

  // Empty URL.
  {
    chrome::ParsedFallbackIconPath parsed;
    EXPECT_TRUE(parsed.Parse(specs + "/"));
    EXPECT_EQ(31, parsed.size_in_pixels());
    const favicon_base::FallbackIconStyle& style = parsed.style();
    EXPECT_EQ(SkColorSetRGB(0x00, 0x00, 0x00), style.background_color);
    EXPECT_FALSE(style.is_default_background_color);
    EXPECT_EQ(SkColorSetRGB(0xFF, 0xFF, 0xFF), style.text_color);
    EXPECT_EQ(0.75, style.font_size_ratio);
    EXPECT_EQ(0.25, style.roundness);
    EXPECT_EQ(GURL(), GURL(parsed.url_string()));
    EXPECT_EQ(specs.length() + 1, parsed.path_index());
  }

  // Tolerate invalid URL.
  {
    chrome::ParsedFallbackIconPath parsed;
    EXPECT_TRUE(parsed.Parse(specs + "/NOT A VALID URL"));
    EXPECT_EQ(31, parsed.size_in_pixels());
    const favicon_base::FallbackIconStyle& style = parsed.style();
    EXPECT_EQ(SkColorSetRGB(0x00, 0x00, 0x00), style.background_color);
    EXPECT_FALSE(style.is_default_background_color);
    EXPECT_EQ(SkColorSetRGB(0xFF, 0xFF, 0xFF), style.text_color);
    EXPECT_EQ(0.75, style.font_size_ratio);
    EXPECT_EQ(0.25, style.roundness);
    EXPECT_EQ("NOT A VALID URL", parsed.url_string());
    EXPECT_EQ(specs.length() + 1, parsed.path_index());
  }

  // Size and style are default.
  {
    std::string specs2 = ",,,,";
    chrome::ParsedFallbackIconPath parsed;
    EXPECT_TRUE(parsed.Parse(specs2 + "/" + kTestUrlStr));
    EXPECT_EQ(gfx::kFaviconSize, parsed.size_in_pixels());
    const favicon_base::FallbackIconStyle& style = parsed.style();
    EXPECT_EQ(kDefaultBackgroundColor, style.background_color);
    EXPECT_TRUE(style.is_default_background_color);
    EXPECT_EQ(kDefaultTextColorLight, style.text_color);
    EXPECT_EQ(kDefaultFontSizeRatio, style.font_size_ratio);
    EXPECT_EQ(kDefaultRoundness, style.roundness);
    EXPECT_EQ(GURL(kTestUrlStr), GURL(parsed.url_string()));
    EXPECT_EQ(specs2.length() + 1, parsed.path_index());
  }
}

TEST_F(FallbackIconUrlParserTest, ParseFallbackIconPathFailure) {
  const char* test_cases[] = {
    // Bad size.
    "-1,000,fff,0.75,0.25/http://www.google.com/",
    // Bad specs.
    "32,#junk,fff,0.75,0.25/http://www.google.com/",
  };
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    chrome::ParsedFallbackIconPath parsed;
    EXPECT_FALSE(parsed.Parse(test_cases[i])) << "test_cases[" << i << "]";
  }
}

}  // namespace chrome
