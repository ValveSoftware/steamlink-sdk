// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "encodings/compact_lang_det/string_byte_sink.h"

#include <string>

using std::string;

StringByteSink::StringByteSink(string* dest) : dest_(dest) {}

StringByteSink::~StringByteSink() {}

void StringByteSink::Append(const char* data, int32_t n) {
  dest_->append(data, n);
}
