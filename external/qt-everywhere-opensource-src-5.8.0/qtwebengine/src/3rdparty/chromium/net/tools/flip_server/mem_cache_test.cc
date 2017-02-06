// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/mem_cache.h"

#include "net/tools/balsa/balsa_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class MemoryCacheWithFakeReadToString : public MemoryCache {
 public:
  ~MemoryCacheWithFakeReadToString() override {}

  void ReadToString(const char* filename, std::string* output) override {
    *output = data_map_[filename];
  }

  std::map<std::string, std::string> data_map_;
};

class FlipMemoryCacheTest : public ::testing::Test {
 public:
  FlipMemoryCacheTest() : mem_cache_(new MemoryCacheWithFakeReadToString) {}

 protected:
  std::unique_ptr<MemoryCacheWithFakeReadToString> mem_cache_;
};

TEST_F(FlipMemoryCacheTest, EmptyCache) {
  MemCacheIter mci;
  mci.stream_id = 0;
  ASSERT_EQ(NULL, mem_cache_->GetFileData("./foo"));
  ASSERT_EQ(NULL, mem_cache_->GetFileData("./bar"));
  ASSERT_FALSE(mem_cache_->AssignFileData("./hello", &mci));
}

TEST_F(FlipMemoryCacheTest, ReadAndStoreFileContents) {
  FileData* foo;
  FileData* hello;

  mem_cache_->data_map_["./foo"] = "bar";
  mem_cache_->data_map_["./hello"] =
      "HTTP/1.0 200 OK\r\n"
      "key1: value1\r\n"
      "key2: value2\r\n\r\n"
      "body: body\r\n";
  mem_cache_->ReadAndStoreFileContents("./foo");
  mem_cache_->ReadAndStoreFileContents("./hello");

  foo = mem_cache_->GetFileData("foo");
  hello = mem_cache_->GetFileData("hello");

  // "./foo" content is broken.
  ASSERT_EQ(NULL, foo);
  ASSERT_FALSE(NULL == hello);
  ASSERT_EQ(hello, mem_cache_->GetFileData("hello"));

  // "HTTP/1.0" is rewritten to "HTTP/1.1".
  ASSERT_EQ("HTTP/1.1", hello->headers()->response_version());
  ASSERT_EQ("200", hello->headers()->response_code());
  ASSERT_EQ("OK", hello->headers()->response_reason_phrase());
  ASSERT_EQ(4,
            std::distance(hello->headers()->header_lines_begin(),
                          hello->headers()->header_lines_end()));
  ASSERT_TRUE(hello->headers()->HasHeader("key1"));
  ASSERT_TRUE(hello->headers()->HasHeader("key2"));
  ASSERT_TRUE(hello->headers()->HasHeader("transfer-encoding"));
  ASSERT_TRUE(hello->headers()->HasHeader("connection"));
  ASSERT_EQ("value1", hello->headers()->GetHeaderPosition("key1")->second);
  ASSERT_EQ("value2", hello->headers()->GetHeaderPosition("key2")->second);
  ASSERT_EQ("chunked",
            hello->headers()->GetHeaderPosition("transfer-encoding")->second);
  ASSERT_EQ("keep-alive",
            hello->headers()->GetHeaderPosition("connection")->second);
  ASSERT_EQ("body: body\r\n", hello->body());
  ASSERT_EQ("hello", hello->filename());
}

TEST_F(FlipMemoryCacheTest, GetFileDataForHtmlFile) {
  FileData* hello_html;

  mem_cache_->data_map_["./hello.http"] =
      "HTTP/1.0 200 OK\r\n"
      "key1: value1\r\n"
      "key2: value2\r\n\r\n"
      "body: body\r\n";

  mem_cache_->ReadAndStoreFileContents("./hello.http");
  hello_html = mem_cache_->GetFileData("hello.html");
  ASSERT_FALSE(NULL == hello_html);
  ASSERT_EQ(hello_html, mem_cache_->GetFileData("hello.http"));
}

}  // namespace

}  // namespace net
