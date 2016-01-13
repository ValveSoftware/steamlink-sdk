// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mock_download_file.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Return;

namespace content {
namespace {

void SuccessRun(const DownloadFile::InitializeCallback& callback) {
  callback.Run(DOWNLOAD_INTERRUPT_REASON_NONE);
}

}  // namespace

MockDownloadFile::MockDownloadFile() {
  // This is here because |Initialize()| is normally called right after
  // construction.
  ON_CALL(*this, Initialize(_))
      .WillByDefault(::testing::Invoke(SuccessRun));
}

MockDownloadFile::~MockDownloadFile() {
}

}  // namespace content
