// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_util.h"
#include "breakpad/src/common/linux/libcurl_wrapper.h"
#include "chromecast/base/scoped_temp_file.h"
#include "chromecast/crash/cast_crashdump_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

class MockLibcurlWrapper : public google_breakpad::LibcurlWrapper {
 public:
  MOCK_METHOD0(Init, bool());
  MOCK_METHOD2(SetProxy,
               bool(const std::string& proxy_host,
                    const std::string& proxy_userpwd));
  MOCK_METHOD2(AddFile,
               bool(const std::string& upload_file_path,
                    const std::string& basename));
  MOCK_METHOD5(SendRequest,
               bool(const std::string& url,
                    const std::map<std::string, std::string>& parameters,
                    int* http_status_code,
                    std::string* http_header_data,
                    std::string* http_response_data));
};

// Declared for the scope of this file to increase readability.
using testing::_;
using testing::Return;

TEST(CastCrashdumpUploaderTest, UploadFailsWhenInitFails) {
  testing::StrictMock<MockLibcurlWrapper> m;
  EXPECT_CALL(m, Init()).Times(1).WillOnce(Return(false));

  CastCrashdumpData data;
  data.product = "foobar";
  data.version = "1.0";
  data.guid = "AAA-BBB";
  data.email = "test@test.com";
  data.comments = "none";
  data.minidump_pathname = "/tmp/foo.dmp";
  data.crash_server = "http://foo.com";
  CastCrashdumpUploader uploader(data, &m);

  ASSERT_FALSE(uploader.Upload(nullptr));
}

TEST(CastCrashdumpUploaderTest, UploadSucceedsWithValidParameters) {
  testing::StrictMock<MockLibcurlWrapper> m;

  // Create a temporary file.
  ScopedTempFile minidump;

  EXPECT_CALL(m, Init()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(m, AddFile(minidump.path().value(), _)).WillOnce(Return(true));
  EXPECT_CALL(m, SendRequest("http://foo.com", _, _, _, _)).Times(1).WillOnce(
      Return(true));

  CastCrashdumpData data;
  data.product = "foobar";
  data.version = "1.0";
  data.guid = "AAA-BBB";
  data.email = "test@test.com";
  data.comments = "none";
  data.minidump_pathname = minidump.path().value();
  data.crash_server = "http://foo.com";
  CastCrashdumpUploader uploader(data, &m);

  ASSERT_TRUE(uploader.Upload(nullptr));
}

TEST(CastCrashdumpUploaderTest, UploadFailsWithInvalidPathname) {
  testing::StrictMock<MockLibcurlWrapper> m;
  EXPECT_CALL(m, Init()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(m, SendRequest(_, _, _, _, _)).Times(0);

  CastCrashdumpData data;
  data.product = "foobar";
  data.version = "1.0";
  data.guid = "AAA-BBB";
  data.email = "test@test.com";
  data.comments = "none";
  data.minidump_pathname = "/invalid/file/path";
  data.crash_server = "http://foo.com";
  CastCrashdumpUploader uploader(data, &m);

  ASSERT_FALSE(uploader.Upload(nullptr));
}

TEST(CastCrashdumpUploaderTest, UploadFailsWithoutAllRequiredParameters) {
  testing::StrictMock<MockLibcurlWrapper> m;

  // Create a temporary file.
  ScopedTempFile minidump;

  // Has all the require fields for a crashdump.
  CastCrashdumpData data;
  data.product = "foobar";
  data.version = "1.0";
  data.guid = "AAA-BBB";
  data.email = "test@test.com";
  data.comments = "none";
  data.minidump_pathname = minidump.path().value();
  data.crash_server = "http://foo.com";

  // Test with empty product name.
  data.product = "";
  EXPECT_CALL(m, Init()).Times(1).WillOnce(Return(true));
  CastCrashdumpUploader uploader_no_product(data, &m);
  ASSERT_FALSE(uploader_no_product.Upload(nullptr));
  data.product = "foobar";

  // Test with empty product version.
  data.version = "";
  EXPECT_CALL(m, Init()).Times(1).WillOnce(Return(true));
  CastCrashdumpUploader uploader_no_version(data, &m);
  ASSERT_FALSE(uploader_no_version.Upload(nullptr));
  data.version = "1.0";

  // Test with empty client GUID.
  data.guid = "";
  EXPECT_CALL(m, Init()).Times(1).WillOnce(Return(true));
  CastCrashdumpUploader uploader_no_guid(data, &m);
  ASSERT_FALSE(uploader_no_guid.Upload(nullptr));
}

TEST(CastCrashdumpUploaderTest, UploadFailsWithInvalidAttachment) {
  testing::StrictMock<MockLibcurlWrapper> m;

  // Create a temporary file.
  ScopedTempFile minidump;

  EXPECT_CALL(m, Init()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(m, AddFile(minidump.path().value(), _)).WillOnce(Return(true));

  CastCrashdumpData data;
  data.product = "foobar";
  data.version = "1.0";
  data.guid = "AAA-BBB";
  data.email = "test@test.com";
  data.comments = "none";
  data.minidump_pathname = minidump.path().value();
  data.crash_server = "http://foo.com";
  CastCrashdumpUploader uploader(data, &m);

  // Add a file that does not exist as an attachment.
  uploader.AddAttachment("label", "/path/does/not/exist");
  ASSERT_FALSE(uploader.Upload(nullptr));
}

TEST(CastCrashdumpUploaderTest, UploadSucceedsWithValidAttachment) {
  testing::StrictMock<MockLibcurlWrapper> m;

  // Create a temporary file.
  ScopedTempFile minidump;

  // Create a valid attachment.
  ScopedTempFile attachment;

  EXPECT_CALL(m, Init()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(m, AddFile(minidump.path().value(), _)).WillOnce(Return(true));
  EXPECT_CALL(m, AddFile(attachment.path().value(), _)).WillOnce(Return(true));
  EXPECT_CALL(m, SendRequest(_, _, _, _, _)).Times(1).WillOnce(Return(true));

  CastCrashdumpData data;
  data.product = "foobar";
  data.version = "1.0";
  data.guid = "AAA-BBB";
  data.email = "test@test.com";
  data.comments = "none";
  data.minidump_pathname = minidump.path().value();
  data.crash_server = "http://foo.com";
  CastCrashdumpUploader uploader(data, &m);

  // Add a valid file as an attachment.
  uploader.AddAttachment("label", attachment.path().value());
  ASSERT_TRUE(uploader.Upload(nullptr));
}

}  // namespace chromeceast
