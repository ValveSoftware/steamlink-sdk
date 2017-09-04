// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/SharedBuffer.h"
#include "platform/heap/Handle.h"
#include "platform/mhtml/ArchiveResource.h"
#include "platform/mhtml/MHTMLParser.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "wtf/Assertions.h"
#include "wtf/Compiler.h"
#include <memory>
#include <stddef.h>
#include <stdint.h>

namespace blink {

// Fuzzer for blink::MHTMLParser.
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  MHTMLParser mhtmlParser(SharedBuffer::create(data, size));
  HeapVector<Member<ArchiveResource>> mhtmlArchives =
      mhtmlParser.parseArchive();
  mhtmlArchives.clear();
  ThreadState::current()->collectAllGarbage();

  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  // Intentional leak - no need to do cleanup as explained in
  // "Initialization/Cleanup" section of testing/libfuzzer/efficient_fuzzer.md
  DEFINE_STATIC_LOCAL(blink::ScopedUnittestsEnvironmentSetup, testSetup,
                      (*argc, *argv));
  ALLOW_UNUSED_LOCAL(testSetup);

  return 0;
}
