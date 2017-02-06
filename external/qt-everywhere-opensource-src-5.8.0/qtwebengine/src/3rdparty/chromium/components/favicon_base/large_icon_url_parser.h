// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_BASE_LARGE_ICON_URL_PARSER_H_
#define COMPONENTS_FAVICON_BASE_LARGE_ICON_URL_PARSER_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"

// A parser for parameters to the chrome://large-icon/ host.
class LargeIconUrlParser {
 public:
  LargeIconUrlParser();
  ~LargeIconUrlParser();

  std::string url_string() const { return url_string_; }

  int size_in_pixels() const { return size_in_pixels_; }

  size_t path_index() const { return path_index_; }

  // Parses |path|, which should be in the format described at the top of the
  // file "chrome/browser/ui/webui/large_icon_source.h". Note that |path| does
  // not have leading '/'.
  bool Parse(base::StringPiece path);

 private:
  friend class LargeIconUrlParserTest;

  // The page URL string of the requested large icon.
  std::string url_string_;

  // The size of the requested large icon in pixels.
  int size_in_pixels_;

  // The index of the first character (relative to the path) where the the URL
  // from which the large icon is being requested is located.
  size_t path_index_;

  DISALLOW_COPY_AND_ASSIGN(LargeIconUrlParser);
};

#endif  // COMPONENTS_FAVICON_BASE_LARGE_ICON_URL_PARSER_H_
