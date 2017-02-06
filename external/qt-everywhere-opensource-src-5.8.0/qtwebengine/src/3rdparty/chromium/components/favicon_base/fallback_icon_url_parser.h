// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_BASE_FALLBACK_ICON_URL_PARSER_H_
#define COMPONENTS_FAVICON_BASE_FALLBACK_ICON_URL_PARSER_H_

#include <stddef.h>

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "third_party/skia/include/core/SkColor.h"

namespace chrome {

class ParsedFallbackIconPath {
 public:
  ParsedFallbackIconPath();
  ~ParsedFallbackIconPath();

  const std::string& url_string() const { return url_string_; }

  int size_in_pixels() const { return size_in_pixels_; }

  const favicon_base::FallbackIconStyle& style() const { return style_; }

  size_t path_index() const { return path_index_; }

  // Parses |path|, which should be in the format described at the top of the
  // file "chrome/browser/ui/webui/fallback_icon_source.h".
  bool Parse(const std::string& path);

 private:
  // Parses |specs_str|, which should be the comma-separated value portion
  // in the format described at the top of the file
  // "chrome/browser/ui/webui/fallback_icon_source.h".
  static bool ParseSpecs(const std::string& specs_str,
                         int *size,
                         favicon_base::FallbackIconStyle* style);

  // Helper to parse color string (e.g., "red", "#f00", "#aB0137"). On success,
  // returns true and writes te result to |*color|.
  static bool ParseColor(const std::string& color_str, SkColor* color);

  friend class FallbackIconUrlParserTest;
  FRIEND_TEST_ALL_PREFIXES(FallbackIconUrlParserTest, ParseColorSuccess);
  FRIEND_TEST_ALL_PREFIXES(FallbackIconUrlParserTest, ParseColorFailure);
  FRIEND_TEST_ALL_PREFIXES(FallbackIconUrlParserTest, ParseSpecsEmpty);
  FRIEND_TEST_ALL_PREFIXES(FallbackIconUrlParserTest, ParseSpecsPartial);
  FRIEND_TEST_ALL_PREFIXES(FallbackIconUrlParserTest, ParseSpecsFull);
  FRIEND_TEST_ALL_PREFIXES(FallbackIconUrlParserTest, ParseSpecsFailure);

  // The page URL string the fallback icon is requested for.
  std::string url_string_;

  // The size of the requested fallback icon in pixels.
  int size_in_pixels_;

  // Styling specifications of fallback icon.
  favicon_base::FallbackIconStyle style_;

  // The index of the first character (relative to the path) where the the URL
  // from which the fallback icon is being requested is located.
  size_t path_index_;

  DISALLOW_COPY_AND_ASSIGN(ParsedFallbackIconPath);
};

}  // namespace chrome

#endif  // COMPONENTS_FAVICON_BASE_FALLBACK_ICON_URL_PARSER_H_
