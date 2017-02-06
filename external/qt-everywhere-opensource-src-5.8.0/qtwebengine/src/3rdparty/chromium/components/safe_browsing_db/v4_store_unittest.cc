// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/safe_browsing_db/v4_store.h"
#include "components/safe_browsing_db/v4_store.pb.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class V4StoreTest : public PlatformTest {
 public:
  V4StoreTest() : task_runner_(new base::TestSimpleTaskRunner) {}

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_path_ = temp_dir_.path().AppendASCII("V4StoreTest.store");
    DVLOG(1) << "store_path_: " << store_path_.value();
  }

  void TearDown() override {
    base::DeleteFile(store_path_, false);
    PlatformTest::TearDown();
  }

  void WriteFileFormatProtoToFile(uint32_t magic,
                                  uint32_t version = 0,
                                  ListUpdateResponse* response = nullptr) {
    V4StoreFileFormat file_format;
    file_format.set_magic_number(magic);
    file_format.set_version_number(version);
    if (response != nullptr) {
      ListUpdateResponse* list_update_response =
          file_format.mutable_list_update_response();
      *list_update_response = *response;
    }

    std::string file_format_string;
    file_format.SerializeToString(&file_format_string);
    base::WriteFile(store_path_, file_format_string.data(),
                    file_format_string.size());
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath store_path_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(V4StoreTest, TestReadFromEmptyFile) {
  base::CloseFile(base::OpenFile(store_path_, "wb+"));

  EXPECT_EQ(FILE_EMPTY_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromAbsentFile) {
  EXPECT_EQ(FILE_UNREADABLE_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromInvalidContentsFile) {
  const char kInvalidContents[] = "Chromium";
  base::WriteFile(store_path_, kInvalidContents, strlen(kInvalidContents));
  EXPECT_EQ(PROTO_PARSING_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromFileWithUnknownProto) {
  Checksum checksum;
  checksum.set_sha256("checksum");
  std::string checksum_string;
  checksum.SerializeToString(&checksum_string);
  base::WriteFile(store_path_, checksum_string.data(), checksum_string.size());

  // Even though we wrote a completely different proto to file, the proto
  // parsing method does not fail. This shows the importance of a magic number.
  EXPECT_EQ(UNEXPECTED_MAGIC_NUMBER_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromUnexpectedMagicFile) {
  WriteFileFormatProtoToFile(111);
  EXPECT_EQ(UNEXPECTED_MAGIC_NUMBER_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromLowVersionFile) {
  WriteFileFormatProtoToFile(0x600D71FE, 2);
  EXPECT_EQ(FILE_VERSION_INCOMPATIBLE_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromNoHashPrefixInfoFile) {
  WriteFileFormatProtoToFile(0x600D71FE, 9);
  EXPECT_EQ(HASH_PREFIX_INFO_MISSING_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromNoHashPrefixesFile) {
  ListUpdateResponse list_update_response;
  list_update_response.set_platform_type(LINUX_PLATFORM);
  WriteFileFormatProtoToFile(0x600D71FE, 9, &list_update_response);
  EXPECT_EQ(READ_SUCCESS, V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestWriteNoResponseType) {
  EXPECT_EQ(INVALID_RESPONSE_TYPE_FAILURE,
            V4Store(task_runner_, store_path_)
                .WriteToDisk(base::WrapUnique(new ListUpdateResponse)));
}

TEST_F(V4StoreTest, TestWritePartialResponseType) {
  std::unique_ptr<ListUpdateResponse> list_update_response(
      new ListUpdateResponse);
  list_update_response->set_response_type(ListUpdateResponse::PARTIAL_UPDATE);
  EXPECT_EQ(INVALID_RESPONSE_TYPE_FAILURE,
            V4Store(task_runner_, store_path_)
                .WriteToDisk(std::move(list_update_response)));
}

TEST_F(V4StoreTest, TestWriteFullResponseType) {
  std::unique_ptr<ListUpdateResponse> list_update_response(
      new ListUpdateResponse);
  list_update_response->set_response_type(ListUpdateResponse::FULL_UPDATE);
  list_update_response->set_new_client_state("test_client_state");
  EXPECT_EQ(WRITE_SUCCESS, V4Store(task_runner_, store_path_)
                               .WriteToDisk(std::move(list_update_response)));

  std::unique_ptr<V4Store> read_store(new V4Store(task_runner_, store_path_));
  EXPECT_EQ(READ_SUCCESS, read_store->ReadFromDisk());
  EXPECT_EQ("test_client_state", read_store->state_);
}

}  // namespace safe_browsing
