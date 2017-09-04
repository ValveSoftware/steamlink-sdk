// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkFuzzerTestSupport_h
#define BlinkFuzzerTestSupport_h

namespace blink {

// InitializeBlinkFuzzTest will spin up an environment similar to
// webkit_unit_tests. It should be called in LLVMFuzzerInitialize.
void InitializeBlinkFuzzTest(int* argc, char*** argv);

}  // namespace blink

#endif  // BlinkFuzzerTestSupport_h
