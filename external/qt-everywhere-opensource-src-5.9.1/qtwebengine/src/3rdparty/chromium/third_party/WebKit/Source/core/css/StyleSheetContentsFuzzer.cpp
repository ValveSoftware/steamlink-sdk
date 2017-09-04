// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/StyleSheetContents.h"

#include "platform/testing/BlinkFuzzerTestSupport.h"
#include "wtf/text/WTFString.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  blink::CSSParserContext context(blink::HTMLStandardMode, nullptr);
  blink::StyleSheetContents* styleSheet =
      blink::StyleSheetContents::create(context);
  styleSheet->parseString(String::fromUTF8WithLatin1Fallback(
      reinterpret_cast<const char*>(data), size));
  return 0;
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  blink::InitializeBlinkFuzzTest(argc, argv);
  return 0;
}
