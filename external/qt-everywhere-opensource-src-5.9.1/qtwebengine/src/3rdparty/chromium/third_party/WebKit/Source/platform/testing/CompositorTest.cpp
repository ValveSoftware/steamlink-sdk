// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/CompositorTest.h"

namespace blink {

CompositorTest::CompositorTest()
    : m_runner(new base::TestMockTimeTaskRunner), m_runnerHandle(m_runner) {}

CompositorTest::~CompositorTest() {}

}  // namespace blink
