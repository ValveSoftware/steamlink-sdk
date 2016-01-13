// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ENCODINGS_COMPACT_LANG_DET_STRING_BYTE_SINK_H_
#define ENCODINGS_COMPACT_LANG_DET_STRING_BYTE_SINK_H_

#include <string>

#include <unicode/unistr.h>

// Implementation of a string byte sink needed when ICU is compiled without
// support for std::string which is the case on Android.
class StringByteSink : public icu::ByteSink {
 public:
  // Constructs a ByteSink that will append bytes to the dest string.
  explicit StringByteSink(std::string* dest);
  virtual ~StringByteSink();

  virtual void Append(const char* data, int32_t n);

 private:
  std::string* const dest_;
};

#endif  // ENCODINGS_COMPACT_LANG_DET_STRING_BYTE_SINK_H_
