// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/fetcher_pool.h"

#include <algorithm>
#include <array>
#include <functional>
#include <list>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace precache {

namespace {

using net::FakeURLFetcher;
using net::HTTP_OK;
using net::URLFetcher;
using net::URLRequestStatus;
using ::testing::_;
using ::testing::Invoke;

class MockURLFetcherDelegate : public net::URLFetcherDelegate {
 public:
  MockURLFetcherDelegate() {};
  virtual ~MockURLFetcherDelegate() {};

  MOCK_METHOD1(OnURLFetchComplete, void(const URLFetcher*));
  MOCK_METHOD3(OnURLFetchDownloadProgress,
               void(const URLFetcher* source, int64_t current, int64_t total));
  MOCK_METHOD3(OnURLFetchUploadProgress,
               void(const URLFetcher* source, int64_t current, int64_t total));
};

TEST(FetcherPoolTest, AddDelete) {
  // It also tests IsAvailable.
  base::MessageLoop loop;
  MockURLFetcherDelegate delegate;
  std::unique_ptr<URLFetcher> url_fetcher(
      new FakeURLFetcher(GURL("http://a.com"), &delegate, "irrelevant", HTTP_OK,
                         URLRequestStatus::SUCCESS));
  URLFetcher* url_fetcher_ptr = url_fetcher.get();

  FetcherPool<URLFetcher> pool(1);
  EXPECT_TRUE(pool.IsAvailable());
  EXPECT_TRUE(pool.IsEmpty());
  pool.Add(std::move(url_fetcher));
  url_fetcher_ptr->Start();
  EXPECT_FALSE(pool.IsAvailable());
  EXPECT_FALSE(pool.IsEmpty());
  EXPECT_CALL(delegate, OnURLFetchComplete(url_fetcher_ptr));

  base::RunLoop().RunUntilIdle();

  pool.Delete(*url_fetcher_ptr);
  EXPECT_TRUE(pool.IsEmpty());
  EXPECT_TRUE(pool.IsAvailable());
}

TEST(FetcherPoolTest, Delete) {
  const size_t kSize = 42;
  base::MessageLoop loop;
  MockURLFetcherDelegate delegate;
  std::unique_ptr<URLFetcher> url_fetcher(
      new FakeURLFetcher(GURL("http://a.com"), &delegate, "irrelevant", HTTP_OK,
                         URLRequestStatus::SUCCESS));
  URLFetcher* url_fetcher_ptr = url_fetcher.get();

  FetcherPool<URLFetcher> pool(kSize);
  pool.Add(std::move(url_fetcher));
  url_fetcher_ptr->Start();
  pool.Delete(*url_fetcher_ptr);

  EXPECT_TRUE(pool.IsEmpty());

  EXPECT_CALL(delegate, OnURLFetchComplete(_)).Times(0);
  base::RunLoop().RunUntilIdle();
}

TEST(FetcherPoolTest, ParallelURLFetchers) {
  // It also tests IsEmpty.
  const size_t kSize = 42;
  base::MessageLoop loop;
  MockURLFetcherDelegate delegate;
  FetcherPool<URLFetcher> pool(kSize);
  std::string urls[] = {"http://a.com", "http://b.com", "http://c.com"};
  // To make sure that nothing slip through while setting the expectations.
  EXPECT_CALL(delegate, OnURLFetchComplete(_)).Times(0);
  int num_requests_in_flight = 0;
  for (const auto& url : urls) {
    std::unique_ptr<URLFetcher> url_fetcher(
        new FakeURLFetcher(GURL(url), &delegate, "irrelevant", HTTP_OK,
                           URLRequestStatus::SUCCESS));
    num_requests_in_flight++;
    url_fetcher->Start();
    pool.Add(std::move(url_fetcher));
    EXPECT_FALSE(pool.IsEmpty());
    EXPECT_TRUE(pool.IsAvailable());
  }
  EXPECT_CALL(delegate, OnURLFetchComplete(_))
      .Times(3)
      .WillRepeatedly(Invoke([&pool](const URLFetcher* fetcher) {
        EXPECT_TRUE(fetcher);
        pool.Delete(*fetcher);
      }));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pool.IsEmpty());
  EXPECT_TRUE(pool.IsAvailable());
}

TEST(FetcherPoolTest, DeleteAll) {
  const size_t kSize = 42;
  base::MessageLoop loop;
  MockURLFetcherDelegate delegate;
  FetcherPool<URLFetcher> pool(kSize);
  std::string urls[] = {"http://a.com", "http://b.com", "http://c.com"};
  EXPECT_CALL(delegate, OnURLFetchComplete(_)).Times(0);
  for (const auto& url : urls) {
    std::unique_ptr<URLFetcher> url_fetcher(
        new FakeURLFetcher(GURL(url), &delegate, "irrelevant", HTTP_OK,
                           URLRequestStatus::SUCCESS));
    url_fetcher->Start();
    pool.Add(std::move(url_fetcher));
  }

  pool.DeleteAll();

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pool.IsEmpty());
  EXPECT_TRUE(pool.IsAvailable());
}

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)

TEST(FetcherPoolTest, AddTooManyURLFetchers) {
  MockURLFetcherDelegate delegate;
  FetcherPool<URLFetcher> pool(0);
  std::unique_ptr<URLFetcher> url_fetcher(
      new FakeURLFetcher(GURL("http://queso.es"), &delegate, "irrelevant",
                         HTTP_OK, URLRequestStatus::SUCCESS));
  EXPECT_DEBUG_DEATH(pool.Add(std::move(url_fetcher)),
                     "FetcherPool size exceeded");
}

TEST(FetcherPoolTest, AddNullURLFetcher) {
  FetcherPool<URLFetcher> pool(1);
  std::unique_ptr<URLFetcher> null_ptr;
  EXPECT_DEBUG_DEATH(pool.Add(std::move(null_ptr)), "cannot be null");
}

TEST(FetcherPoolTest, DeleteUnregisteredURLFetcher) {
  MockURLFetcherDelegate delegate;
  FetcherPool<URLFetcher> pool(1);
  FakeURLFetcher url_fetcher(GURL("http://queso.es"), &delegate, "irrelevant",
                             HTTP_OK, URLRequestStatus::SUCCESS);
  EXPECT_DEBUG_DEATH(pool.Delete(url_fetcher),
                     "doesn't contain the given element");
}

#endif  // GTEST_HAS_DEATH_TEST && !defined(NDEBUG)

TEST(FetcherPoolTest, ExampleUsage) {
  base::MessageLoop loop;
  FetcherPool<URLFetcher> pool(2);
  MockURLFetcherDelegate delegate;

  std::list<GURL> pending_urls{
      {GURL("http://a.com"), GURL("http://b.com"), GURL("http://c.com")}};

  std::function<void()> start_next_batch = [&pending_urls, &pool, &delegate]() {
    while (!pending_urls.empty() && pool.IsAvailable()) {
      // Called CreateAndStartUrlFetcher in the documentation.
      std::unique_ptr<URLFetcher> fetcher(
          new FakeURLFetcher(GURL(pending_urls.front()), &delegate,
                             "irrelevant", HTTP_OK, URLRequestStatus::SUCCESS));
      fetcher->Start();
      pending_urls.pop_front();
      pool.Add(std::move(fetcher));
    }
  };

  EXPECT_CALL(delegate, OnURLFetchComplete(_)).Times(0);  // 3 and no more.
  EXPECT_CALL(delegate, OnURLFetchComplete(_))
      .Times(pending_urls.size())
      .WillRepeatedly(
          Invoke([&pool, &start_next_batch](const URLFetcher* fetcher) {
            EXPECT_TRUE(fetcher);
            pool.Delete(*fetcher);
            start_next_batch();
          }));

  start_next_batch();
  EXPECT_FALSE(pool.IsEmpty());
  EXPECT_FALSE(pool.IsAvailable());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pool.IsEmpty());
  EXPECT_TRUE(pool.IsAvailable());
}

}  // namespace

}  // namespace precache
