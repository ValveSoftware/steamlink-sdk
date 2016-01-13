// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/dump_cache/url_utilities.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace net {

std::string UrlUtilities::GetUrlHost(const std::string& url) {
  size_t b = url.find("//");
  if (b == std::string::npos)
    b = 0;
  else
    b += 2;
  size_t next_slash = url.find_first_of('/', b);
  size_t next_colon = url.find_first_of(':', b);
  if (next_slash != std::string::npos
      && next_colon != std::string::npos
      && next_colon < next_slash) {
    return std::string(url, b, next_colon - b);
  }
  if (next_slash == std::string::npos) {
    if (next_colon != std::string::npos) {
      return std::string(url, b, next_colon - b);
    } else {
      next_slash = url.size();
    }
  }
  return std::string(url, b, next_slash - b);
}

std::string UrlUtilities::GetUrlHostPath(const std::string& url) {
  size_t b = url.find("//");
  if (b == std::string::npos)
    b = 0;
  else
    b += 2;
  return std::string(url, b);
}

std::string UrlUtilities::GetUrlPath(const std::string& url) {
  size_t b = url.find("//");
  if (b == std::string::npos)
    b = 0;
  else
    b += 2;
  b = url.find("/", b);
  if (b == std::string::npos)
    return "/";

  size_t e = url.find("#", b+1);
  if (e != std::string::npos)
    return std::string(url, b, (e - b));
  return std::string(url, b);
}

namespace {

// Parsing states for UrlUtilities::Unescape
enum UnescapeState {
  NORMAL,   // We are not in the middle of parsing an escape.
  ESCAPE1,  // We just parsed % .
  ESCAPE2   // We just parsed %X for some hex digit X.
};

}  // namespace

std::string UrlUtilities::Unescape(const std::string& escaped_url) {
  std::string unescaped_url, escape_text;
  int escape_value;
  UnescapeState state = NORMAL;
  std::string::const_iterator iter = escaped_url.begin();
  while (iter < escaped_url.end()) {
    char c = *iter;
    switch (state) {
      case NORMAL:
        if (c == '%') {
          escape_text.clear();
          state = ESCAPE1;
        } else {
          unescaped_url.push_back(c);
        }
        ++iter;
        break;
      case ESCAPE1:
        if (IsHexDigit(c)) {
          escape_text.push_back(c);
          state = ESCAPE2;
          ++iter;
        } else {
          // Unexpected, % followed by non-hex chars, pass it through.
          unescaped_url.push_back('%');
          state = NORMAL;
        }
        break;
      case ESCAPE2:
        if (IsHexDigit(c)) {
          escape_text.push_back(c);
          bool ok = base::HexStringToInt(escape_text, &escape_value);
          DCHECK(ok);
          unescaped_url.push_back(static_cast<unsigned char>(escape_value));
          state = NORMAL;
          ++iter;
        } else {
          // Unexpected, % followed by non-hex chars, pass it through.
          unescaped_url.push_back('%');
          unescaped_url.append(escape_text);
          state = NORMAL;
        }
        break;
    }
  }
  // Unexpected, % followed by end of string, pass it through.
  if (state == ESCAPE1 || state == ESCAPE2) {
    unescaped_url.push_back('%');
    unescaped_url.append(escape_text);
  }
  return unescaped_url;
}

}  // namespace net

