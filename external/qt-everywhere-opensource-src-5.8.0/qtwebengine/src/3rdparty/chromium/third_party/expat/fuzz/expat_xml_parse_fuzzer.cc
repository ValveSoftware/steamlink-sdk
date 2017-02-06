// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "third_party/expat/files/lib/expat.h"

#include <array>

static void XMLCALL
startElement(void* userData, const char* name, const char** atts) {
  int* depthPtr = static_cast<int*>(userData);
  (void)atts;

  for (int i = 0; i < *depthPtr; i++)
    (void)name;

  *depthPtr += 1;
}


static void XMLCALL
endElement(void* userData, const char* name) {
  int* depthPtr = static_cast<int*>(userData);
  (void)name;

  *depthPtr -= 1;
}


std::array<const char*, 7> kEncodings = {{ "UTF-16", "UTF-8", "ISO_8859_1",
                                           "US_ASCII", "UTF_16BE", "UTF_16LE",
                                            nullptr }};


// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  for (auto enc : kEncodings) {
    XML_Parser parser = XML_ParserCreate(enc);
    if (!parser)
      return 0;

    int depth = 0;
    XML_SetUserData(parser, &depth);
    XML_SetElementHandler(parser, startElement, endElement);

    const char* dataPtr = reinterpret_cast<const char*>(data);

    // Feed the data with two different values of |isFinal| for better coverage.
    for (int isFinal = 0; isFinal <= 1; ++isFinal) {
      if (XML_Parse(parser, dataPtr, size, isFinal) == XML_STATUS_ERROR) {
        XML_ErrorString(XML_GetErrorCode(parser));
        XML_GetCurrentLineNumber(parser);
      }
    }

    XML_ParserFree(parser);
  }

  return 0;
}
