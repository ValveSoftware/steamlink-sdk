// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/strings/string_tokenizer.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) {
    return 0;
  }
  size_t pattern_size = data[0];
  if (pattern_size > size - 1) {
    return 0;
  }
  std::string pattern(reinterpret_cast<const char*>(data + 1), pattern_size);
  std::string input(reinterpret_cast<const char*>(data + 1 + pattern_size),
                    size - pattern_size - 1);
  base::StringTokenizer t(input, pattern);
  while (t.GetNext()) {
    (void)t.token();
  }
  return 0;
}
