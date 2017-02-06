// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_FILE_H_
#define CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_FILE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/browser/download/download_file.h"
#include "content/public/browser/download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
struct DownloadCreateInfo;

class MockDownloadFile : public DownloadFile {
 public:
  MockDownloadFile();
  virtual ~MockDownloadFile();

  // DownloadFile functions.
  MOCK_METHOD1(Initialize, void(const InitializeCallback&));
  MOCK_METHOD2(AppendDataToFile, DownloadInterruptReason(
      const char* data, size_t data_len));
  MOCK_METHOD1(Rename, DownloadInterruptReason(
      const base::FilePath& full_path));
  MOCK_METHOD2(RenameAndUniquify,
               void(const base::FilePath& full_path,
                    const RenameCompletionCallback& callback));
  MOCK_METHOD5(RenameAndAnnotate,
               void(const base::FilePath& full_path,
                    const std::string& client_guid,
                    const GURL& source_url,
                    const GURL& referrer_url,
                    const RenameCompletionCallback& callback));
  MOCK_METHOD0(Detach, void());
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD0(Finish, void());
  MOCK_CONST_METHOD0(FullPath, const base::FilePath&());
  MOCK_CONST_METHOD0(InProgress, bool());
  MOCK_CONST_METHOD0(BytesSoFar, int64_t());
  MOCK_CONST_METHOD0(CurrentSpeed, int64_t());
  MOCK_METHOD1(GetHash, bool(std::string* hash));
  MOCK_METHOD0(SendUpdate, void());
  MOCK_CONST_METHOD0(Id, int());
  MOCK_METHOD0(GetDownloadManager, DownloadManager*());
  MOCK_CONST_METHOD0(DebugString, std::string());
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_FILE_H_
