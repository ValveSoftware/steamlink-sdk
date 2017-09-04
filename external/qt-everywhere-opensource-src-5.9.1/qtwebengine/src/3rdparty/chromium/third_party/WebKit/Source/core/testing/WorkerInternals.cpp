// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/WorkerInternals.h"

#include "core/testing/OriginTrialsTest.h"

namespace blink {

WorkerInternals::~WorkerInternals() {}

WorkerInternals::WorkerInternals() {}

OriginTrialsTest* WorkerInternals::originTrialsTest() const {
  return OriginTrialsTest::create();
}

}  // namespace blink
