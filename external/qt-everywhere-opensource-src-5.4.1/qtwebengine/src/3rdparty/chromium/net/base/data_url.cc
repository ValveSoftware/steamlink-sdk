// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: based loosely on mozilla's nsDataChannel.cpp

#include <algorithm>

#include "net/base/data_url.h"

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "url/gurl.h"

namespace net {

// static
bool DataURL::Parse(const GURL& url, std::string* mime_type,
                    std::string* charset, std::string* data) {
  DCHECK(mime_type->empty());
  DCHECK(charset->empty());
  std::string::const_iterator begin = url.spec().begin();
  std::string::const_iterator end = url.spec().end();

  std::string::const_iterator after_colon = std::find(begin, end, ':');
  if (after_colon == end)
    return false;
  ++after_colon;

  std::string::const_iterator comma = std::find(after_colon, end, ',');
  if (comma == end)
    return false;

  std::vector<std::string> meta_data;
  std::string unparsed_meta_data(after_colon, comma);
  base::SplitString(unparsed_meta_data, ';', &meta_data);

  std::vector<std::string>::iterator iter = meta_data.begin();
  if (iter != meta_data.end()) {
    mime_type->swap(*iter);
    StringToLowerASCII(mime_type);
    ++iter;
  }

  static const char kBase64Tag[] = "base64";
  static const char kCharsetTag[] = "charset=";
  const size_t kCharsetTagLength = arraysize(kCharsetTag) - 1;

  bool base64_encoded = false;
  for (; iter != meta_data.end(); ++iter) {
    if (!base64_encoded && *iter == kBase64Tag) {
      base64_encoded = true;
    } else if (charset->empty() &&
               iter->compare(0, kCharsetTagLength, kCharsetTag) == 0) {
      charset->assign(iter->substr(kCharsetTagLength));
    }
  }

  if (mime_type->empty()) {
    // fallback to defaults if nothing specified in the URL:
    mime_type->assign("text/plain");
  } else if (!ParseMimeTypeWithoutParameter(*mime_type, NULL, NULL)) {
    return false;
  }
  if (charset->empty())
    charset->assign("US-ASCII");

  // The caller may not be interested in receiving the data.
  if (!data)
    return true;

  // Preserve spaces if dealing with text or xml input, same as mozilla:
  //   https://bugzilla.mozilla.org/show_bug.cgi?id=138052
  // but strip them otherwise:
  //   https://bugzilla.mozilla.org/show_bug.cgi?id=37200
  // (Spaces in a data URL should be escaped, which is handled below, so any
  // spaces now are wrong. People expect to be able to enter them in the URL
  // bar for text, and it can't hurt, so we allow it.)
  std::string temp_data = std::string(comma + 1, end);

  // For base64, we may have url-escaped whitespace which is not part
  // of the data, and should be stripped. Otherwise, the escaped whitespace
  // could be part of the payload, so don't strip it.
  if (base64_encoded) {
    temp_data = UnescapeURLComponent(temp_data,
        UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS |
        UnescapeRule::CONTROL_CHARS);
  }

  // Strip whitespace.
  if (base64_encoded || !(mime_type->compare(0, 5, "text/") == 0 ||
                          mime_type->find("xml") != std::string::npos)) {
    temp_data.erase(std::remove_if(temp_data.begin(), temp_data.end(),
                                   IsAsciiWhitespace<wchar_t>),
                    temp_data.end());
  }

  if (!base64_encoded) {
    temp_data = UnescapeURLComponent(temp_data,
        UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS |
        UnescapeRule::CONTROL_CHARS);
  }

  if (base64_encoded) {
    size_t length = temp_data.length();
    size_t padding_needed = 4 - (length % 4);
    // If the input wasn't padded, then we pad it as necessary until we have a
    // length that is a multiple of 4 as required by our decoder. We don't
    // correct if the input was incorrectly padded. If |padding_needed| == 3,
    // then the input isn't well formed and decoding will fail with or without
    // padding.
    if ((padding_needed == 1 || padding_needed == 2) &&
        temp_data[length - 1] != '=') {
      temp_data.resize(length + padding_needed, '=');
    }
    return base::Base64Decode(temp_data, data);
  }

  temp_data.swap(*data);
  return true;
}

}  // namespace net
