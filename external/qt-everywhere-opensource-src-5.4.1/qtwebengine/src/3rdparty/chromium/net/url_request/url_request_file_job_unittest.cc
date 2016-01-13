// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_file_job.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "net/base/filename_util.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A URLRequestFileJob for testing OnSeekComplete / OnReadComplete callbacks.
class URLRequestFileJobWithCallbacks : public net::URLRequestFileJob {
 public:
  URLRequestFileJobWithCallbacks(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const base::FilePath& file_path,
      const scoped_refptr<base::TaskRunner>& file_task_runner)
      : URLRequestFileJob(request,
                          network_delegate,
                          file_path,
                          file_task_runner),
        seek_position_(0) {
  }

  int64 seek_position() { return seek_position_; }
  const std::vector<std::string>& data_chunks() { return data_chunks_; }

 protected:
  virtual ~URLRequestFileJobWithCallbacks() {}

  virtual void OnSeekComplete(int64 result) OVERRIDE {
    ASSERT_EQ(seek_position_, 0);
    seek_position_ = result;
  }

  virtual void OnReadComplete(net::IOBuffer* buf, int result) OVERRIDE {
    data_chunks_.push_back(std::string(buf->data(), result));
  }

  int64 seek_position_;
  std::vector<std::string> data_chunks_;
};

// A URLRequestJobFactory that will return URLRequestFileJobWithCallbacks
// instances for file:// scheme URLs.
class CallbacksJobFactory : public net::URLRequestJobFactory {
 public:
  class JobObserver {
   public:
    virtual void OnJobCreated(URLRequestFileJobWithCallbacks* job) = 0;
  };

  CallbacksJobFactory(const base::FilePath& path, JobObserver* observer)
      : path_(path), observer_(observer) {
  }

  virtual ~CallbacksJobFactory() {}

  virtual net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    URLRequestFileJobWithCallbacks* job = new URLRequestFileJobWithCallbacks(
        request,
        network_delegate,
        path_,
        base::MessageLoop::current()->message_loop_proxy());
    observer_->OnJobCreated(job);
    return job;
  }

  virtual bool IsHandledProtocol(const std::string& scheme) const OVERRIDE {
    return scheme == "file";
  }

  virtual bool IsHandledURL(const GURL& url) const OVERRIDE {
    return IsHandledProtocol(url.scheme());
  }

  virtual bool IsSafeRedirectTarget(const GURL& location) const OVERRIDE {
    return false;
  }

 private:
  base::FilePath path_;
  JobObserver* observer_;
};

// Helper function to create a file in |directory| filled with
// |content|. Returns true on succes and fills in |path| with the full path to
// the file.
bool CreateTempFileWithContent(const std::string& content,
                               const base::ScopedTempDir& directory,
                               base::FilePath* path) {
  if (!directory.IsValid())
    return false;

  if (!base::CreateTemporaryFileInDir(directory.path(), path))
    return false;

  return base::WriteFile(*path, content.c_str(), content.length());
}

class JobObserverImpl : public CallbacksJobFactory::JobObserver {
 public:
  virtual void OnJobCreated(URLRequestFileJobWithCallbacks* job) OVERRIDE {
    jobs_.push_back(job);
  }

  typedef std::vector<scoped_refptr<URLRequestFileJobWithCallbacks> > JobList;

  const JobList& jobs() { return jobs_; }

 protected:
  JobList jobs_;
};

// A simple holder for start/end used in http range requests.
struct Range {
  int start;
  int end;

  Range() {
    start = 0;
    end = 0;
  }

  Range(int start, int end) {
    this->start = start;
    this->end = end;
  }
};

// A superclass for tests of the OnSeekComplete / OnReadComplete functions of
// URLRequestFileJob.
class URLRequestFileJobEventsTest : public testing::Test {
 public:
  URLRequestFileJobEventsTest();

 protected:
  // This creates a file with |content| as the contents, and then creates and
  // runs a URLRequestFileJobWithCallbacks job to get the contents out of it,
  // and makes sure that the callbacks observed the correct bytes. If a Range
  // is provided, this function will add the appropriate Range http header to
  // the request and verify that only the bytes in that range (inclusive) were
  // observed.
  void RunRequest(const std::string& content, const Range* range);

  JobObserverImpl observer_;
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
};

URLRequestFileJobEventsTest::URLRequestFileJobEventsTest() {}

void URLRequestFileJobEventsTest::RunRequest(const std::string& content,
                                             const Range* range) {
  base::ScopedTempDir directory;
  ASSERT_TRUE(directory.CreateUniqueTempDir());
  base::FilePath path;
  ASSERT_TRUE(CreateTempFileWithContent(content, directory, &path));
  CallbacksJobFactory factory(path, &observer_);
  context_.set_job_factory(&factory);

  net::TestURLRequest request(net::FilePathToFileURL(path),
                              net::DEFAULT_PRIORITY,
                              &delegate_,
                              &context_);
  if (range) {
    ASSERT_GE(range->start, 0);
    ASSERT_GE(range->end, 0);
    ASSERT_LE(range->start, range->end);
    ASSERT_LT(static_cast<unsigned int>(range->end), content.length());
    std::string range_value =
        base::StringPrintf("bytes=%d-%d", range->start, range->end);
    request.SetExtraRequestHeaderByName(
        net::HttpRequestHeaders::kRange, range_value, true /*overwrite*/);
  }
  request.Start();

  base::RunLoop loop;
  loop.Run();

  EXPECT_FALSE(delegate_.request_failed());
  int expected_length =
      range ? (range->end - range->start + 1) : content.length();
  EXPECT_EQ(delegate_.bytes_received(), expected_length);

  std::string expected_content;
  if (range) {
    expected_content.insert(0, content, range->start, expected_length);
  } else {
    expected_content = content;
  }
  EXPECT_TRUE(delegate_.data_received() == expected_content);

  ASSERT_EQ(observer_.jobs().size(), 1u);
  ASSERT_EQ(observer_.jobs().at(0)->seek_position(), range ? range->start : 0);

  std::string observed_content;
  const std::vector<std::string>& chunks =
      observer_.jobs().at(0)->data_chunks();
  for (std::vector<std::string>::const_iterator i = chunks.begin();
       i != chunks.end();
       ++i) {
    observed_content.append(*i);
  }
  EXPECT_EQ(expected_content, observed_content);
  EXPECT_TRUE(expected_content == observed_content);
}

// Helper function to make a character array filled with |size| bytes of
// test content.
std::string MakeContentOfSize(int size) {
  EXPECT_GE(size, 0);
  std::string result;
  result.reserve(size);
  for (int i = 0; i < size; i++) {
    result.append(1, static_cast<char>(i % 256));
  }
  return result;
}

}  // namespace

TEST_F(URLRequestFileJobEventsTest, TinyFile) {
  RunRequest(std::string("hello world"), NULL);
}

TEST_F(URLRequestFileJobEventsTest, SmallFile) {
  RunRequest(MakeContentOfSize(17 * 1024), NULL);
}

TEST_F(URLRequestFileJobEventsTest, BigFile) {
  RunRequest(MakeContentOfSize(3 * 1024 * 1024), NULL);
}

TEST_F(URLRequestFileJobEventsTest, Range) {
  // Use a 15KB content file and read a range chosen somewhat arbitrarily but
  // not aligned on any likely page boundaries.
  int size = 15 * 1024;
  Range range(1701, (6 * 1024) + 3);
  RunRequest(MakeContentOfSize(size), &range);
}
