// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/url_to_filename_encoder.h"

#include <stdlib.h>

#include "base/logging.h"
#include "base/strings/string_util.h"

using std::string;

namespace {

#ifdef WIN32
#define strtoull _strtoui64
#endif

// A simple parser for long long values. Returns the parsed value if a
// valid integer is found; else returns deflt
// UInt64 and Int64 cannot handle decimal numbers with leading 0s.
uint64_t ParseLeadingHex64Value(const char* str, uint64_t deflt) {
  char* error = NULL;
  const uint64_t value = strtoull(str, &error, 16);
  return (error == str) ? deflt : value;
}

}  // namespace

namespace net {

// The escape character choice is made here -- all code and tests in this
// directory are based off of this constant.  However, our testdata
// has tons of dependencies on this, so it cannot be changed without
// re-running those tests and fixing them.
const char UrlToFilenameEncoder::kEscapeChar = ',';
const char UrlToFilenameEncoder::kTruncationChar = '-';
const size_t UrlToFilenameEncoder::kMaximumSubdirectoryLength = 128;

void UrlToFilenameEncoder::AppendSegment(string* segment, string* dest) {
  CHECK(!segment->empty());
  if ((*segment == ".") || (*segment == "..")) {
    dest->append(1, kEscapeChar);
    dest->append(*segment);
    segment->clear();
  } else {
    size_t segment_size = segment->size();
    if (segment_size > kMaximumSubdirectoryLength) {
      // We need to inject ",-" at the end of the segment to signify that
      // we are inserting an artificial '/'.  This means we have to chop
      // off at least two characters to make room.
      segment_size = kMaximumSubdirectoryLength - 2;

      // But we don't want to break up an escape sequence that happens to lie at
      // the end.  Escape sequences are at most 2 characters.
      if ((*segment)[segment_size - 1] == kEscapeChar) {
        segment_size -= 1;
      } else if ((*segment)[segment_size - 2] == kEscapeChar) {
        segment_size -= 2;
      }
      dest->append(segment->data(), segment_size);
      dest->append(1, kEscapeChar);
      dest->append(1, kTruncationChar);
      segment->erase(0, segment_size);

      // At this point, if we had segment_size=3, and segment="abcd",
      // then after this erase, we will have written "abc,-" and set segment="d"
    } else {
      dest->append(*segment);
      segment->clear();
    }
  }
}

void UrlToFilenameEncoder::EncodeSegment(const string& filename_prefix,
                                         const string& escaped_ending,
                                         char dir_separator,
                                         string* encoded_filename) {
  string filename_ending = UrlUtilities::Unescape(escaped_ending);

  char encoded[3];
  int encoded_len;
  string segment;

  // TODO(jmarantz): This code would be a bit simpler if we disallowed
  // Instaweb allowing filename_prefix to not end in "/".  We could
  // then change the is routine to just take one input string.
  size_t start_of_segment = filename_prefix.find_last_of(dir_separator);
  if (start_of_segment == string::npos) {
    segment = filename_prefix;
  } else {
    segment = filename_prefix.substr(start_of_segment + 1);
    *encoded_filename = filename_prefix.substr(0, start_of_segment + 1);
  }

  size_t index = 0;
  // Special case the first / to avoid adding a leading kEscapeChar.
  if (!filename_ending.empty() && (filename_ending[0] == dir_separator)) {
    encoded_filename->append(segment);
    segment.clear();
    encoded_filename->append(1, dir_separator);
    ++index;
  }

  for (; index < filename_ending.length(); ++index) {
    unsigned char ch = static_cast<unsigned char>(filename_ending[index]);

    // Note: instead of outputing an empty segment, we let the second slash
    // be escaped below.
    if ((ch == dir_separator) && !segment.empty()) {
      AppendSegment(&segment, encoded_filename);
      encoded_filename->append(1, dir_separator);
      segment.clear();
    } else {
      // After removing unsafe chars the only safe ones are _.=+- and alphanums.
      if ((ch == '_') || (ch == '.') || (ch == '=') || (ch == '+') ||
          (ch == '-') || (('0' <= ch) && (ch <= '9')) ||
          (('A' <= ch) && (ch <= 'Z')) || (('a' <= ch) && (ch <= 'z'))) {
        encoded[0] = ch;
        encoded_len = 1;
      } else {
        encoded[0] = kEscapeChar;
        encoded[1] = ch / 16;
        encoded[1] += (encoded[1] >= 10) ? 'A' - 10 : '0';
        encoded[2] = ch % 16;
        encoded[2] += (encoded[2] >= 10) ? 'A' - 10 : '0';
        encoded_len = 3;
      }
      segment.append(encoded, encoded_len);

      // If segment is too big, we must chop it into chunks.
      if (segment.size() > kMaximumSubdirectoryLength) {
        AppendSegment(&segment, encoded_filename);
        encoded_filename->append(1, dir_separator);
      }
    }
  }

  // Append "," to the leaf filename so the leaf can also be a branch., e.g.
  // allow http://a/b/c and http://a/b/c/d to co-exist as files "/a/b/c," and
  // /a/b/c/d".  So we will rename the "d" here to "d,".  If doing that pushed
  // us over the 128 char limit, then we will need to append "/" and the
  // remaining chars.
  segment += kEscapeChar;
  AppendSegment(&segment, encoded_filename);
  if (!segment.empty()) {
    // The last overflow segment is special, because we appended in
    // kEscapeChar above.  We won't need to check it again for size
    // or further escaping.
    encoded_filename->append(1, dir_separator);
    encoded_filename->append(segment);
  }
}

// Note: this decoder is not the exact inverse of the EncodeSegment above,
// because it does not take into account a prefix.
bool UrlToFilenameEncoder::Decode(const string& encoded_filename,
                                  char dir_separator,
                                  string* decoded_url) {
  enum State { kStart, kEscape, kFirstDigit, kTruncate, kEscapeDot };
  State state = kStart;
  char hex_buffer[3];
  hex_buffer[2] = '\0';
  for (size_t i = 0; i < encoded_filename.size(); ++i) {
    char ch = encoded_filename[i];
    switch (state) {
      case kStart:
        if (ch == kEscapeChar) {
          state = kEscape;
        } else if (ch == dir_separator) {
          decoded_url->append(1, '/');  // URLs only use '/' not '\\'
        } else {
          decoded_url->append(1, ch);
        }
        break;
      case kEscape:
        if (base::IsHexDigit(ch)) {
          hex_buffer[0] = ch;
          state = kFirstDigit;
        } else if (ch == kTruncationChar) {
          state = kTruncate;
        } else if (ch == '.') {
          decoded_url->append(1, '.');
          state = kEscapeDot;  // Look for at most one more dot.
        } else if (ch == dir_separator) {
          // Consider url "//x".  This was once encoded to "/,/x,".
          // This code is what skips the first Escape.
          decoded_url->append(1, '/');  // URLs only use '/' not '\\'
          state = kStart;
        } else {
          return false;
        }
        break;
      case kFirstDigit:
        if (base::IsHexDigit(ch)) {
          hex_buffer[1] = ch;
          uint64_t hex_value = ParseLeadingHex64Value(hex_buffer, 0);
          decoded_url->append(1, static_cast<char>(hex_value));
          state = kStart;
        } else {
          return false;
        }
        break;
      case kTruncate:
        if (ch == dir_separator) {
          // Skip this separator, it was only put in to break up long
          // path segments, but is not part of the URL.
          state = kStart;
        } else {
          return false;
        }
        break;
      case kEscapeDot:
        decoded_url->append(1, ch);
        state = kStart;
        break;
    }
  }

  // All legal encoded filenames end in kEscapeChar.
  return (state == kEscape);
}

// Escape the given input |path| and chop any individual components
// of the path which are greater than kMaximumSubdirectoryLength characters
// into two chunks.
//
// This legacy version has several issues with aliasing of different URLs,
// inability to represent both /a/b/c and /a/b/c/d, and inability to decode
// the filenames back into URLs.
//
// But there is a large body of slurped data which depends on this format,
// so leave it as the default for spdy_in_mem_edsm_server.
string UrlToFilenameEncoder::LegacyEscape(const string& path) {
  string output;

  // Note:  We also chop paths into medium sized 'chunks'.
  //        This is due to the incompetence of the windows
  //        filesystem, which still hasn't figured out how
  //        to deal with long filenames.
  int last_slash = 0;
  for (size_t index = 0; index < path.length(); index++) {
    char ch = path[index];
    if (ch == 0x5C)
      last_slash = index;
    if ((ch == 0x2D) ||                    // hyphen
        (ch == 0x5C) || (ch == 0x5F) ||    // backslash, underscore
        ((0x30 <= ch) && (ch <= 0x39)) ||  // Digits [0-9]
        ((0x41 <= ch) && (ch <= 0x5A)) ||  // Uppercase [A-Z]
        ((0x61 <= ch) && (ch <= 0x7A))) {  // Lowercase [a-z]
      output.append(&path[index], 1);
    } else {
      char encoded[3];
      encoded[0] = 'x';
      encoded[1] = ch / 16;
      encoded[1] += (encoded[1] >= 10) ? 'A' - 10 : '0';
      encoded[2] = ch % 16;
      encoded[2] += (encoded[2] >= 10) ? 'A' - 10 : '0';
      output.append(encoded, 3);
    }
    if (index - last_slash > kMaximumSubdirectoryLength) {
#ifdef WIN32
      char slash = '\\';
#else
      char slash = '/';
#endif
      output.append(&slash, 1);
      last_slash = index;
    }
  }
  return output;
}

}  // namespace net
