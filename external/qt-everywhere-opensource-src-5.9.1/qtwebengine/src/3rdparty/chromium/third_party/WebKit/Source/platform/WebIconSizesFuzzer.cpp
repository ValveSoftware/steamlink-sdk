// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/BlinkFuzzerTestSupport.h"
#include "public/platform/WebIconSizesParser.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebString.h"

namespace blink {

// Fuzzer for blink::MHTMLParser.
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  WebString string =
      WebString::fromUTF8(reinterpret_cast<const char*>(data), size);
  WebIconSizesParser::parseIconSizes(string);
  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  blink::InitializeBlinkFuzzTest(argc, argv);
  return 0;
}
