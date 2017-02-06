// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/large_icon_url_parser.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/skia/include/utils/SkParse.h"
#include "ui/gfx/favicon_size.h"

LargeIconUrlParser::LargeIconUrlParser() : size_in_pixels_(48) {
}

LargeIconUrlParser::~LargeIconUrlParser() {
}

bool LargeIconUrlParser::Parse(base::StringPiece path) {
  if (path.empty())
    return false;

  size_t slash = path.find("/", 0);  // |path| does not start with '/'.
  if (slash == base::StringPiece::npos)
    return false;
  base::StringPiece size_str = path.substr(0, slash);
  // Disallow empty, non-numeric, or non-positive sizes.
  if (size_str.empty() ||
      !base::StringToInt(size_str, &size_in_pixels_) ||
      size_in_pixels_ <= 0)
    return false;

  // Need to store the index of the URL field, so Instant Extended can translate
  // large icon URLs using advanced parameters.
  // Example:
  //   "chrome-search://large-icon/48/<renderer-id>/<most-visited-id>"
  // would be translated to:
  //   "chrome-search://large-icon/48/<most-visited-item-with-given-id>".
  path_index_ = slash + 1;
  url_string_ = path.substr(path_index_).as_string();
  return true;
}
