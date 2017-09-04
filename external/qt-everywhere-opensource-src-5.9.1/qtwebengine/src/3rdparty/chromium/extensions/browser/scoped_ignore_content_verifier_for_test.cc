// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"

namespace extensions {

ScopedIgnoreContentVerifierForTest::ScopedIgnoreContentVerifierForTest() {
  ContentVerifyJob::SetDelegateForTests(this);
}

ScopedIgnoreContentVerifierForTest::~ScopedIgnoreContentVerifierForTest() {
  ContentVerifyJob::SetDelegateForTests(nullptr);
}

ContentVerifyJob::FailureReason ScopedIgnoreContentVerifierForTest::BytesRead(
    const std::string& extension_id,
    int count,
    const char* data) {
  return ContentVerifyJob::NONE;
}

ContentVerifyJob::FailureReason ScopedIgnoreContentVerifierForTest::DoneReading(
    const std::string& extension_id) {
  return ContentVerifyJob::NONE;
}

}  // namespace extensions
