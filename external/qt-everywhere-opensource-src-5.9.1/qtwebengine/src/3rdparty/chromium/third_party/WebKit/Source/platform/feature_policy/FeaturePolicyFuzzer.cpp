// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/feature_policy/FeaturePolicy.h"

#include "platform/heap/Handle.h"
#include "platform/testing/BlinkFuzzerTestSupport.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>
#include <stddef.h>
#include <stdint.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  WTF::Vector<WTF::String> messages;
  RefPtr<blink::SecurityOrigin> origin =
      blink::SecurityOrigin::createFromString("https://example.com/");
  std::unique_ptr<blink::FeaturePolicy> policy =
      blink::FeaturePolicy::createFromParentPolicy(nullptr, origin);
  policy->setHeaderPolicy(WTF::String(data, size), messages);
  return 0;
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  blink::InitializeBlinkFuzzTest(argc, argv);
  return 0;
}
