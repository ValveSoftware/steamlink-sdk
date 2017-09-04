// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/image_data_fetcher.h"

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kImageURL[] = "http://www.example.com/image";
const char kURLResponseData[] = "EncodedImageData";

}  // namespace

namespace image_fetcher {

class ImageDataFetcherTest : public testing::Test {
 public:
  ImageDataFetcherTest()
      : test_request_context_getter_(
            new net::TestURLRequestContextGetter(message_loop_.task_runner())),
        image_data_fetcher_(test_request_context_getter_.get()) {}
  ~ImageDataFetcherTest() override {}

  MOCK_METHOD1(OnImageDataFetched, void(const std::string&));

  MOCK_METHOD1(OnImageDataFetchedFailedRequest, void(const std::string&));

  MOCK_METHOD1(OnImageDataFetchedMultipleRequests, void(const std::string&));

 protected:
  base::MessageLoop message_loop_;

  scoped_refptr<net::URLRequestContextGetter> test_request_context_getter_;

  ImageDataFetcher image_data_fetcher_;

  net::TestURLFetcherFactory fetcher_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageDataFetcherTest);
};

TEST_F(ImageDataFetcherTest, FetchImageData) {
  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::Bind(&ImageDataFetcherTest::OnImageDataFetched,
                 base::Unretained(this)));
  EXPECT_CALL(*this, OnImageDataFetched(std::string(kURLResponseData)));

  // Get and configure the TestURLFetcher.
  net::TestURLFetcher* test_url_fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_NE(nullptr, test_url_fetcher);
  test_url_fetcher->set_status(
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, net::OK));
  test_url_fetcher->SetResponseString(kURLResponseData);

  // Call the URLFetcher delegate to continue the test.
  test_url_fetcher->delegate()->OnURLFetchComplete(test_url_fetcher);
}

TEST_F(ImageDataFetcherTest, FetchImageData_FailedRequest) {
  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::Bind(&ImageDataFetcherTest::OnImageDataFetchedFailedRequest,
                 base::Unretained(this)));
  EXPECT_CALL(*this, OnImageDataFetchedFailedRequest(std::string()));

  // Get and configure the TestURLFetcher.
  net::TestURLFetcher* test_url_fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_NE(nullptr, test_url_fetcher);
  test_url_fetcher->set_status(
      net::URLRequestStatus(net::URLRequestStatus::FAILED,
                            net::ERR_INVALID_URL));

  // Call the URLFetcher delegate to continue the test.
  test_url_fetcher->delegate()->OnURLFetchComplete(test_url_fetcher);
}

TEST_F(ImageDataFetcherTest, FetchImageData_MultipleRequests) {
  ImageDataFetcher::ImageDataFetcherCallback callback =
      base::Bind(&ImageDataFetcherTest::OnImageDataFetchedMultipleRequests,
                 base::Unretained(this));
  EXPECT_CALL(*this, OnImageDataFetchedMultipleRequests(testing::_)).Times(2);

  image_data_fetcher_.FetchImageData(GURL(kImageURL), callback);
  image_data_fetcher_.FetchImageData(GURL(kImageURL), callback);

  // Multiple calls to FetchImageData for the same URL will result in
  // multiple URLFetchers being created.
  net::TestURLFetcher* test_url_fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_NE(nullptr, test_url_fetcher);
  test_url_fetcher->delegate()->OnURLFetchComplete(test_url_fetcher);

  test_url_fetcher = fetcher_factory_.GetFetcherByID(1);
  ASSERT_NE(nullptr, test_url_fetcher);
  test_url_fetcher->delegate()->OnURLFetchComplete(test_url_fetcher);
}

}  // namespace image_fetcher
