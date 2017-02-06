// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/files_test_base.h"

#include <utility>

#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "components/filesystem/public/interfaces/types.mojom.h"
#include "services/shell/public/cpp/connector.h"

namespace filesystem {

FilesTestBase::FilesTestBase() : ShellTest("exe:filesystem_service_unittests") {
}

FilesTestBase::~FilesTestBase() {
}

void FilesTestBase::SetUp() {
  ShellTest::SetUp();
  connector()->ConnectToInterface("mojo:filesystem", &files_);
}

void FilesTestBase::GetTemporaryRoot(mojom::DirectoryPtr* directory) {
  mojom::FileError error = mojom::FileError::FAILED;
  files()->OpenTempDirectory(GetProxy(directory), Capture(&error));
  ASSERT_TRUE(files().WaitForIncomingResponse());
  ASSERT_EQ(mojom::FileError::OK, error);
}

}  // namespace filesystem
