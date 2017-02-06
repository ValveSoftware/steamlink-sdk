// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/url_request_content_job.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_file_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace content {

namespace {

// A URLRequestJobFactory that will return URLRequestContentJobWithCallbacks
// instances for content:// scheme URLs.
class CallbacksJobFactory : public net::URLRequestJobFactory {
 public:
  class JobObserver {
   public:
    virtual ~JobObserver() {}
    virtual void OnJobCreated() = 0;
  };

  CallbacksJobFactory(const base::FilePath& path, JobObserver* observer)
      : path_(path), observer_(observer) {
  }

  ~CallbacksJobFactory() override {}

  net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    URLRequestContentJob* job =
        new URLRequestContentJob(
            request,
            network_delegate,
            path_,
            const_cast<base::MessageLoop*>(&message_loop_)->task_runner());
    observer_->OnJobCreated();
    return job;
  }

  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) const override {
    return nullptr;
  }

  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return nullptr;
  }

  bool IsHandledProtocol(const std::string& scheme) const override {
    return scheme == "content";
  }

  bool IsHandledURL(const GURL& url) const override {
    return IsHandledProtocol(url.scheme());
  }

  bool IsSafeRedirectTarget(const GURL& location) const override {
    return false;
  }

 private:
  base::MessageLoop message_loop_;
  base::FilePath path_;
  JobObserver* observer_;
};

class JobObserverImpl : public CallbacksJobFactory::JobObserver {
 public:
  JobObserverImpl() : num_jobs_created_(0) {}
  ~JobObserverImpl() override {}

  void OnJobCreated() override { ++num_jobs_created_; }

  int num_jobs_created() const { return num_jobs_created_; }

 private:
  int num_jobs_created_;
};

// A simple holder for start/end used in http range requests.
struct Range {
  int start;
  int end;

  explicit Range(int start, int end) {
    this->start = start;
    this->end = end;
  }
};

// A superclass for tests of the OnSeekComplete / OnReadComplete functions of
// URLRequestContentJob.
class URLRequestContentJobTest : public testing::Test {
 public:
  URLRequestContentJobTest();

 protected:
  // This inserts an image file into the android MediaStore and retrieves the
  // content URI. Then creates and runs a URLRequestContentJob to get the
  // contents out of it. If a Range is provided, this function will add the
  // appropriate Range http header to the request and verify the bytes
  // retrieved.
  void RunRequest(const Range* range);

  JobObserverImpl observer_;
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
};

URLRequestContentJobTest::URLRequestContentJobTest() {}

void URLRequestContentJobTest::RunRequest(const Range* range) {
  base::FilePath test_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &test_dir);
  test_dir = test_dir.AppendASCII("content");
  test_dir = test_dir.AppendASCII("test");
  test_dir = test_dir.AppendASCII("data");
  test_dir = test_dir.AppendASCII("android");
  ASSERT_TRUE(base::PathExists(test_dir));
  base::FilePath image_file = test_dir.Append(FILE_PATH_LITERAL("red.png"));

  // Insert the image into MediaStore. MediaStore will do some conversions, and
  // return the content URI.
  base::FilePath path = base::InsertImageIntoMediaStore(image_file);
  EXPECT_TRUE(path.IsContentUri());
  EXPECT_TRUE(base::PathExists(path));
  int64_t file_size;
  EXPECT_TRUE(base::GetFileSize(path, &file_size));
  EXPECT_LT(0, file_size);
  CallbacksJobFactory factory(path, &observer_);
  context_.set_job_factory(&factory);

  std::unique_ptr<net::URLRequest> request(context_.CreateRequest(
      GURL(path.value()), net::DEFAULT_PRIORITY, &delegate_));
  int expected_length = file_size;
  if (range) {
    ASSERT_GE(range->start, 0);
    ASSERT_GE(range->end, 0);
    ASSERT_LE(range->start, range->end);
    std::string range_value =
        base::StringPrintf("bytes=%d-%d", range->start, range->end);
    request->SetExtraRequestHeaderByName(
        net::HttpRequestHeaders::kRange, range_value, true /*overwrite*/);
    if (range->start <= file_size) {
      expected_length = std::min(range->end, static_cast<int>(file_size - 1)) -
                        range->start + 1;
    } else {
      expected_length = 0;
    }
  }
  request->Start();

  base::RunLoop loop;
  loop.Run();

  EXPECT_FALSE(delegate_.request_failed());
  ASSERT_EQ(1, observer_.num_jobs_created());
  EXPECT_EQ(expected_length, delegate_.bytes_received());
}

TEST_F(URLRequestContentJobTest, ContentURIWithoutRange) {
  RunRequest(NULL);
}

TEST_F(URLRequestContentJobTest, ContentURIWithSmallRange) {
  Range range(1, 10);
  RunRequest(&range);
}

TEST_F(URLRequestContentJobTest, ContentURIWithLargeRange) {
  Range range(1, 100000);
  RunRequest(&range);
}

TEST_F(URLRequestContentJobTest, ContentURIWithZeroRange) {
  Range range(0, 0);
  RunRequest(&range);
}

}  // namespace

}  // namespace content
