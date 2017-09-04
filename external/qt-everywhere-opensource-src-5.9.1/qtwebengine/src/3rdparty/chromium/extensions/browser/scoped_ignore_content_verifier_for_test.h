// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SCOPED_IGNORE_CONTENT_VERIFIER_FOR_TEST_H_
#define EXTENSIONS_BROWSER_SCOPED_IGNORE_CONTENT_VERIFIER_FOR_TEST_H_

#include <string>

#include "extensions/browser/content_verify_job.h"

namespace extensions {

// A class for use in tests to make content verification failures be ignored
// during the lifetime of an instance of it. Note that only one instance should
// be alive at any given time, and that it is not compatible with other
// concurrent objects using the ContentVerifyJob::TestDelegate interface.
class ScopedIgnoreContentVerifierForTest
    : public ContentVerifyJob::TestDelegate {
 public:
  ScopedIgnoreContentVerifierForTest();
  ~ScopedIgnoreContentVerifierForTest();

  // ContentVerifyJob::TestDelegate interface
  ContentVerifyJob::FailureReason BytesRead(const std::string& extension_id,
                                            int count,
                                            const char* data) override;
  ContentVerifyJob::FailureReason DoneReading(
      const std::string& extension_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedIgnoreContentVerifierForTest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SCOPED_IGNORE_CONTENT_VERIFIER_FOR_TEST_H_
