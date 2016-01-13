// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "net/tools/balsa/balsa_headers.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/test_tools/quic_in_memory_cache_peer.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::IntToString;
using base::StringPiece;

namespace net {
namespace tools {
namespace test {

class QuicInMemoryCacheTest : public ::testing::Test {
 protected:
  QuicInMemoryCacheTest() {
    base::FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("net").AppendASCII("data")
        .AppendASCII("quic_in_memory_cache_data");
    // The file path is known to be an ascii string.
    FLAGS_quic_in_memory_cache_dir = path.MaybeAsASCII();
  }

  void CreateRequest(std::string host,
                     std::string path,
                     net::BalsaHeaders* headers) {
    headers->SetRequestFirstlineFromStringPieces("GET", path, "HTTP/1.1");
    headers->ReplaceOrAppendHeader("host", host);
  }

  virtual void SetUp() OVERRIDE {
    QuicInMemoryCachePeer::ResetForTests();
  }

  // This method was copied from end_to_end_test.cc in this directory.
  void AddToCache(const StringPiece& method,
                  const StringPiece& path,
                  const StringPiece& version,
                  const StringPiece& response_code,
                  const StringPiece& response_detail,
                  const StringPiece& body) {
    BalsaHeaders request_headers, response_headers;
    request_headers.SetRequestFirstlineFromStringPieces(method,
                                                        path,
                                                        version);
    response_headers.SetRequestFirstlineFromStringPieces(version,
                                                         response_code,
                                                         response_detail);
    response_headers.AppendHeader("content-length",
                                  base::IntToString(body.length()));

    // Check if response already exists and matches.
    QuicInMemoryCache* cache = QuicInMemoryCache::GetInstance();
    const QuicInMemoryCache::Response* cached_response =
        cache->GetResponse(request_headers);
    if (cached_response != NULL) {
      std::string cached_response_headers_str, response_headers_str;
      cached_response->headers().DumpToString(&cached_response_headers_str);
      response_headers.DumpToString(&response_headers_str);
      CHECK_EQ(cached_response_headers_str, response_headers_str);
      CHECK_EQ(cached_response->body(), body);
      return;
    }
    cache->AddResponse(request_headers, response_headers, body);
  }
};

TEST_F(QuicInMemoryCacheTest, AddResponseGetResponse) {
  std::string response_body("hello response");
  AddToCache("GET", "https://www.google.com/bar",
             "HTTP/1.1", "200", "OK", response_body);
  net::BalsaHeaders request_headers;
  CreateRequest("www.google.com", "/bar", &request_headers);
  QuicInMemoryCache* cache = QuicInMemoryCache::GetInstance();
  const QuicInMemoryCache::Response* response =
      cache->GetResponse(request_headers);
  ASSERT_TRUE(response);
  EXPECT_EQ("200", response->headers().response_code());
  EXPECT_EQ(response_body.size(), response->body().length());

  CreateRequest("", "https://www.google.com/bar", &request_headers);
  response = cache->GetResponse(request_headers);
  ASSERT_TRUE(response);
  EXPECT_EQ("200", response->headers().response_code());
  EXPECT_EQ(response_body.size(), response->body().length());
}

TEST_F(QuicInMemoryCacheTest, ReadsCacheDir) {
  net::BalsaHeaders request_headers;
  CreateRequest("quic.test.url", "/index.html", &request_headers);

  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse(request_headers);
  ASSERT_TRUE(response);
  std::string value;
  response->headers().GetAllOfHeaderAsString("Connection", &value);
  EXPECT_EQ("200", response->headers().response_code());
  EXPECT_EQ("Keep-Alive", value);
  EXPECT_LT(0U, response->body().length());
}

TEST_F(QuicInMemoryCacheTest, ReadsCacheDirHttp) {
  net::BalsaHeaders request_headers;
  CreateRequest("", "http://quic.test.url/index.html", &request_headers);

  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse(request_headers);
  ASSERT_TRUE(response);
  std::string value;
  response->headers().GetAllOfHeaderAsString("Connection", &value);
  EXPECT_EQ("200", response->headers().response_code());
  EXPECT_EQ("Keep-Alive", value);
  EXPECT_LT(0U, response->body().length());
}

TEST_F(QuicInMemoryCacheTest, GetResponseNoMatch) {
  net::BalsaHeaders request_headers;
  CreateRequest("www.google.com", "/index.html", &request_headers);

  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse(request_headers);
  ASSERT_FALSE(response);
}

}  // namespace test
}  // namespace tools
}  // namespace net
