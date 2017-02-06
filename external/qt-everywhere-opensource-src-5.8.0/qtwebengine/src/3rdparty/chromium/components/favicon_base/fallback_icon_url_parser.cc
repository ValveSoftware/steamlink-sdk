// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/fallback_icon_url_parser.h"

#include <algorithm>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/skia/include/utils/SkParse.h"
#include "ui/gfx/favicon_size.h"

namespace {

// List of sizes corresponding to RGB, ARGB, RRGGBB, AARRGGBB.
const size_t kValidHexColorSizes[] = {3, 4, 6, 8};

// Returns whether |color_str| is a valid CSS color in hex format if we prepend
// '#', i.e., whether |color_str| matches /^[0-9A-Fa-f]{3,4,6,8}$/.
bool IsHexColorString(const std::string& color_str) {
  size_t len = color_str.length();
  const size_t* end = kValidHexColorSizes + arraysize(kValidHexColorSizes);
  if (std::find(kValidHexColorSizes, end, len) == end)
    return false;
  for (auto ch : color_str) {
    if (!base::IsHexDigit(ch))
      return false;
  }
  return true;
}

}  // namespace

namespace chrome {

ParsedFallbackIconPath::ParsedFallbackIconPath()
    : size_in_pixels_(gfx::kFaviconSize) {
}

ParsedFallbackIconPath::~ParsedFallbackIconPath() {
}

bool ParsedFallbackIconPath::Parse(const std::string& path) {
  if (path.empty())
    return false;

  size_t slash = path.find("/", 0);
  if (slash == std::string::npos)
    return false;
  std::string spec_str = path.substr(0, slash);
  if (!ParseSpecs(spec_str, &size_in_pixels_, &style_))
    return false;  // Parse failed.

  // Need to store the index of the URL field, so Instant Extended can translate
  // fallback icon URLs using advanced parameters.
  // Example:
  //   "chrome-search://fallback-icon/48/<renderer-id>/<most-visited-id>"
  // would be translated to:
  //   "chrome-search://fallback-icon/48/<most-visited-item-with-given-id>".
  path_index_ = slash + 1;
  url_string_ = path.substr(path_index_);
  return true;
}

// static
bool ParsedFallbackIconPath::ParseSpecs(
    const std::string& specs_str,
    int *size,
    favicon_base::FallbackIconStyle* style) {
  DCHECK(size);
  DCHECK(style);

  std::vector<std::string> tokens = base::SplitString(
      specs_str, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() != 5)  // Force "," for empty fields.
    return false;

  *size = gfx::kFaviconSize;
  if (!tokens[0].empty() && !base::StringToInt(tokens[0], size))
    return false;
  if (*size <= 0)
    return false;

  if (!tokens[1].empty() && !ParseColor(tokens[1], &style->background_color))
    return false;

  if (tokens[2].empty())
    favicon_base::MatchFallbackIconTextColorAgainstBackgroundColor(style);
  else if (!ParseColor(tokens[2], &style->text_color))
    return false;

  if (!tokens[3].empty() &&
      !base::StringToDouble(tokens[3], &style->font_size_ratio))
    return false;

  if (!tokens[4].empty() && !base::StringToDouble(tokens[4], &style->roundness))
    return false;

  return favicon_base::ValidateFallbackIconStyle(*style);
}

// static
bool ParsedFallbackIconPath::ParseColor(const std::string& color_str,
                                        SkColor* color) {
  DCHECK(color);
  // Exclude the empty case. Also disallow the '#' prefix, since we want color
  // to be part of an URL, but in URL '#' is used for ref fragment.
  if (color_str.empty() || color_str[0] == '#')
    return false;

  // If a valid color hex string is given, prepend '#' and parse (always works).
  // This is unambiguous since named color never only use leters 'a' to 'f'.
  if (IsHexColorString(color_str)) {
    // Default alpha to 0xFF since FindColor() preserves unspecified alpha.
    *color = SK_ColorWHITE;
    // Need temp variable to avoid use-after-free of returned pointer.
    std::string color_str_with_hash = "#" + color_str;
    const char* end = SkParse::FindColor(color_str_with_hash.c_str(), color);
    DCHECK(end && !*end);  // Call should succeed and consume string.
    return true;
  }

  // Default alpha to 0xFF.
  SkColor temp_color = SK_ColorWHITE;
  const char* end = SkParse::FindColor(color_str.c_str(), &temp_color);
  if (end && !*end) {  // Successful if call succeeds and string is consumed.
    *color = temp_color;
    return true;
  }
  return false;
}

}  // namespace chrome
