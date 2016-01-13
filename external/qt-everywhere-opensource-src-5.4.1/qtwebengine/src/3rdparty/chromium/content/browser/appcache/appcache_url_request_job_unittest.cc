// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stack>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/pickle.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/appcache/appcache_response.h"
#include "webkit/browser/appcache/appcache_url_request_job.h"

using appcache::AppCacheEntry;
using appcache::AppCacheStorage;
using appcache::AppCacheResponseInfo;
using appcache::AppCacheResponseReader;
using appcache::AppCacheResponseWriter;
using appcache::AppCacheURLRequestJob;
using appcache::HttpResponseInfoIOBuffer;
using appcache::kAppCacheNoCacheId;
using net::IOBuffer;
using net::WrappedIOBuffer;

namespace content {

namespace {

const char kHttpBasicHeaders[] = "HTTP/1.0 200 OK\0Content-Length: 5\0\0";
const char kHttpBasicBody[] = "Hello";

const int kNumBlocks = 4;
const int kBlockSize = 1024;

class MockURLRequestJobFactory : public net::URLRequestJobFactory {
 public:
  MockURLRequestJobFactory() : job_(NULL) {
  }

  virtual ~MockURLRequestJobFactory() {
    DCHECK(!job_);
  }

  void SetJob(net::URLRequestJob* job) {
    job_ = job;
  }

  bool has_job() const {
    return job_ != NULL;
  }

  // net::URLRequestJobFactory implementation.
  virtual net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    if (job_) {
      net::URLRequestJob* temp = job_;
      job_ = NULL;
      return temp;
    } else {
      return new net::URLRequestErrorJob(request,
                                         network_delegate,
                                         net::ERR_INTERNET_DISCONNECTED);
    }
  }

  virtual bool IsHandledProtocol(const std::string& scheme) const OVERRIDE {
    return scheme == "http";
  };

  virtual bool IsHandledURL(const GURL& url) const OVERRIDE {
    return url.SchemeIs("http");
  }

  virtual bool IsSafeRedirectTarget(const GURL& location) const OVERRIDE {
    return false;
  }

  private:
   mutable net::URLRequestJob* job_;
};

class AppCacheURLRequestJobTest : public testing::Test {
 public:

  // Test Harness -------------------------------------------------------------
  // TODO(michaeln): share this test harness with AppCacheResponseTest

  class MockStorageDelegate : public AppCacheStorage::Delegate {
   public:
    explicit MockStorageDelegate(AppCacheURLRequestJobTest* test)
        : loaded_info_id_(0), test_(test) {
    }

    virtual void OnResponseInfoLoaded(AppCacheResponseInfo* info,
                                      int64 response_id) OVERRIDE {
      loaded_info_ = info;
      loaded_info_id_ = response_id;
      test_->ScheduleNextTask();
    }

    scoped_refptr<AppCacheResponseInfo> loaded_info_;
    int64 loaded_info_id_;
    AppCacheURLRequestJobTest* test_;
  };

  class MockURLRequestDelegate : public net::URLRequest::Delegate {
   public:
    explicit MockURLRequestDelegate(AppCacheURLRequestJobTest* test)
        : test_(test),
          received_data_(new net::IOBuffer(kNumBlocks * kBlockSize)),
          did_receive_headers_(false), amount_received_(0),
          kill_after_amount_received_(0), kill_with_io_pending_(false) {
    }

    virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE {
      amount_received_ = 0;
      did_receive_headers_ = false;
      if (request->status().is_success()) {
        EXPECT_TRUE(request->response_headers());
        did_receive_headers_ = true;
        received_info_ = request->response_info();
        ReadSome(request);
      } else {
        RequestComplete();
      }
    }

    virtual void OnReadCompleted(net::URLRequest* request,
                                 int bytes_read) OVERRIDE {
      if (bytes_read > 0) {
        amount_received_ += bytes_read;

        if (kill_after_amount_received_ && !kill_with_io_pending_) {
          if (amount_received_ >= kill_after_amount_received_) {
            request->Cancel();
            return;
          }
        }

        ReadSome(request);

        if (kill_after_amount_received_ && kill_with_io_pending_) {
          if (amount_received_ >= kill_after_amount_received_) {
            request->Cancel();
            return;
          }
        }
      } else {
        RequestComplete();
      }
    }

    void ReadSome(net::URLRequest* request) {
      DCHECK(amount_received_ + kBlockSize <= kNumBlocks * kBlockSize);
      scoped_refptr<IOBuffer> wrapped_buffer(
          new net::WrappedIOBuffer(received_data_->data() + amount_received_));
      int bytes_read = 0;
      EXPECT_FALSE(
          request->Read(wrapped_buffer.get(), kBlockSize, &bytes_read));
      EXPECT_EQ(0, bytes_read);
    }

    void RequestComplete() {
      test_->ScheduleNextTask();
    }

    AppCacheURLRequestJobTest* test_;
    net::HttpResponseInfo received_info_;
    scoped_refptr<net::IOBuffer> received_data_;
    bool did_receive_headers_;
    int amount_received_;
    int kill_after_amount_received_;
    bool kill_with_io_pending_;
  };

  // Helper callback to run a test on our io_thread. The io_thread is spun up
  // once and reused for all tests.
  template <class Method>
  void MethodWrapper(Method method) {
    SetUpTest();
    (this->*method)();
  }

  static void SetUpTestCase() {
    io_thread_.reset(new base::Thread("AppCacheURLRequestJobTest Thread"));
    base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
    io_thread_->StartWithOptions(options);
  }

  static void TearDownTestCase() {
    io_thread_.reset(NULL);
  }

  AppCacheURLRequestJobTest() {}

  template <class Method>
  void RunTestOnIOThread(Method method) {
    test_finished_event_ .reset(new base::WaitableEvent(false, false));
    io_thread_->message_loop()->PostTask(
        FROM_HERE, base::Bind(&AppCacheURLRequestJobTest::MethodWrapper<Method>,
                              base::Unretained(this), method));
    test_finished_event_->Wait();
  }

  void SetUpTest() {
    DCHECK(base::MessageLoop::current() == io_thread_->message_loop());
    DCHECK(task_stack_.empty());

    storage_delegate_.reset(new MockStorageDelegate(this));
    service_.reset(new MockAppCacheService());
    expected_read_result_ = 0;
    expected_write_result_ = 0;
    written_response_id_ = 0;
    reader_deletion_count_down_ = 0;
    writer_deletion_count_down_ = 0;

    url_request_delegate_.reset(new MockURLRequestDelegate(this));
    job_factory_.reset(new MockURLRequestJobFactory());
    empty_context_.reset(new net::URLRequestContext());
    empty_context_->set_job_factory(job_factory_.get());
  }

  void TearDownTest() {
    DCHECK(base::MessageLoop::current() == io_thread_->message_loop());
    request_.reset();

    while (!task_stack_.empty())
      task_stack_.pop();

    reader_.reset();
    read_buffer_ = NULL;
    read_info_buffer_ = NULL;
    writer_.reset();
    write_buffer_ = NULL;
    write_info_buffer_ = NULL;
    storage_delegate_.reset();
    service_.reset();

    DCHECK(!job_factory_->has_job());
    empty_context_.reset();
    job_factory_.reset();
    url_request_delegate_.reset();
  }

  void TestFinished() {
    // We unwind the stack prior to finishing up to let stack
    // based objects get deleted.
    DCHECK(base::MessageLoop::current() == io_thread_->message_loop());
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AppCacheURLRequestJobTest::TestFinishedUnwound,
                   base::Unretained(this)));
  }

  void TestFinishedUnwound() {
    TearDownTest();
    test_finished_event_->Signal();
  }

  void PushNextTask(const base::Closure& task) {
    task_stack_.push(std::pair<base::Closure, bool>(task, false));
  }

  void PushNextTaskAsImmediate(const base::Closure& task) {
    task_stack_.push(std::pair<base::Closure, bool>(task, true));
  }

  void ScheduleNextTask() {
    DCHECK(base::MessageLoop::current() == io_thread_->message_loop());
    if (task_stack_.empty()) {
      TestFinished();
      return;
    }
    base::Closure task =task_stack_.top().first;
    bool immediate = task_stack_.top().second;
    task_stack_.pop();
    if (immediate)
      task.Run();
    else
      base::MessageLoop::current()->PostTask(FROM_HERE, task);
  }

  // Wrappers to call AppCacheResponseReader/Writer Read and Write methods

  void WriteBasicResponse() {
    scoped_refptr<IOBuffer> body(new WrappedIOBuffer(kHttpBasicBody));
    std::string raw_headers(kHttpBasicHeaders, arraysize(kHttpBasicHeaders));
    WriteResponse(
        MakeHttpResponseInfo(raw_headers), body.get(), strlen(kHttpBasicBody));
  }

  void WriteResponse(net::HttpResponseInfo* head,
                     IOBuffer* body, int body_len) {
    DCHECK(body);
    scoped_refptr<IOBuffer> body_ref(body);
    PushNextTask(base::Bind(&AppCacheURLRequestJobTest::WriteResponseBody,
                            base::Unretained(this), body_ref, body_len));
    WriteResponseHead(head);
  }

  void WriteResponseHead(net::HttpResponseInfo* head) {
    EXPECT_FALSE(writer_->IsWritePending());
    expected_write_result_ = GetHttpResponseInfoSize(head);
    write_info_buffer_ = new HttpResponseInfoIOBuffer(head);
    writer_->WriteInfo(
        write_info_buffer_.get(),
        base::Bind(&AppCacheURLRequestJobTest::OnWriteInfoComplete,
                   base::Unretained(this)));
  }

  void WriteResponseBody(scoped_refptr<IOBuffer> io_buffer, int buf_len) {
    EXPECT_FALSE(writer_->IsWritePending());
    write_buffer_ = io_buffer;
    expected_write_result_ = buf_len;
    writer_->WriteData(write_buffer_.get(),
                       buf_len,
                       base::Bind(&AppCacheURLRequestJobTest::OnWriteComplete,
                                  base::Unretained(this)));
  }

  void ReadResponseBody(scoped_refptr<IOBuffer> io_buffer, int buf_len) {
    EXPECT_FALSE(reader_->IsReadPending());
    read_buffer_ = io_buffer;
    expected_read_result_ = buf_len;
    reader_->ReadData(read_buffer_.get(),
                      buf_len,
                      base::Bind(&AppCacheURLRequestJobTest::OnReadComplete,
                                 base::Unretained(this)));
  }

  // AppCacheResponseReader / Writer completion callbacks

  void OnWriteInfoComplete(int result) {
    EXPECT_FALSE(writer_->IsWritePending());
    EXPECT_EQ(expected_write_result_, result);
    ScheduleNextTask();
  }

  void OnWriteComplete(int result) {
    EXPECT_FALSE(writer_->IsWritePending());
    EXPECT_EQ(expected_write_result_, result);
    ScheduleNextTask();
  }

  void OnReadInfoComplete(int result) {
    EXPECT_FALSE(reader_->IsReadPending());
    EXPECT_EQ(expected_read_result_, result);
    ScheduleNextTask();
  }

  void OnReadComplete(int result) {
    EXPECT_FALSE(reader_->IsReadPending());
    EXPECT_EQ(expected_read_result_, result);
    ScheduleNextTask();
  }

  // Helpers to work with HttpResponseInfo objects

  net::HttpResponseInfo* MakeHttpResponseInfo(const std::string& raw_headers) {
    net::HttpResponseInfo* info = new net::HttpResponseInfo;
    info->request_time = base::Time::Now();
    info->response_time = base::Time::Now();
    info->was_cached = false;
    info->headers = new net::HttpResponseHeaders(raw_headers);
    return info;
  }

  int GetHttpResponseInfoSize(const net::HttpResponseInfo* info) {
    Pickle pickle;
    return PickleHttpResonseInfo(&pickle, info);
  }

  bool CompareHttpResponseInfos(const net::HttpResponseInfo* info1,
                                const net::HttpResponseInfo* info2) {
    Pickle pickle1;
    Pickle pickle2;
    PickleHttpResonseInfo(&pickle1, info1);
    PickleHttpResonseInfo(&pickle2, info2);
    return (pickle1.size() == pickle2.size()) &&
           (0 == memcmp(pickle1.data(), pickle2.data(), pickle1.size()));
  }

  int PickleHttpResonseInfo(Pickle* pickle, const net::HttpResponseInfo* info) {
    const bool kSkipTransientHeaders = true;
    const bool kTruncated = false;
    info->Persist(pickle, kSkipTransientHeaders, kTruncated);
    return pickle->size();
  }

  // Helpers to fill and verify blocks of memory with a value

  void FillData(char value, char* data, int data_len) {
    memset(data, value, data_len);
  }

  bool CheckData(char value, const char* data, int data_len) {
    for (int i = 0; i < data_len; ++i, ++data) {
      if (*data != value)
        return false;
    }
    return true;
  }

  // Individual Tests ---------------------------------------------------------
  // Some of the individual tests involve multiple async steps. Each test
  // is delineated with a section header.

  // Basic -------------------------------------------------------------------
  void Basic() {
    AppCacheStorage* storage = service_->storage();
    net::URLRequest request(GURL("http://blah/"), net::DEFAULT_PRIORITY, NULL,
                            empty_context_.get());
    scoped_refptr<AppCacheURLRequestJob> job;

    // Create an instance and see that it looks as expected.

    job = new AppCacheURLRequestJob(
        &request, NULL, storage, NULL, false);
    EXPECT_TRUE(job->is_waiting());
    EXPECT_FALSE(job->is_delivering_appcache_response());
    EXPECT_FALSE(job->is_delivering_network_response());
    EXPECT_FALSE(job->is_delivering_error_response());
    EXPECT_FALSE(job->has_been_started());
    EXPECT_FALSE(job->has_been_killed());
    EXPECT_EQ(GURL(), job->manifest_url());
    EXPECT_EQ(kAppCacheNoCacheId, job->cache_id());
    EXPECT_FALSE(job->entry().has_response_id());

    TestFinished();
  }

  // DeliveryOrders -----------------------------------------------------
  void DeliveryOrders() {
    AppCacheStorage* storage = service_->storage();
    net::URLRequest request(GURL("http://blah/"), net::DEFAULT_PRIORITY, NULL,
                            empty_context_.get());
    scoped_refptr<AppCacheURLRequestJob> job;

    // Create an instance, give it a delivery order and see that
    // it looks as expected.

    job = new AppCacheURLRequestJob(&request, NULL, storage, NULL, false);
    job->DeliverErrorResponse();
    EXPECT_TRUE(job->is_delivering_error_response());
    EXPECT_FALSE(job->has_been_started());

    job = new AppCacheURLRequestJob(&request, NULL, storage, NULL, false);
    job->DeliverNetworkResponse();
    EXPECT_TRUE(job->is_delivering_network_response());
    EXPECT_FALSE(job->has_been_started());

    job = new AppCacheURLRequestJob(&request, NULL, storage, NULL, false);
    const GURL kManifestUrl("http://blah/");
    const int64 kCacheId(1);
    const int64 kGroupId(1);
    const AppCacheEntry kEntry(AppCacheEntry::EXPLICIT, 1);
    job->DeliverAppCachedResponse(kManifestUrl, kCacheId, kGroupId,
                                  kEntry, false);
    EXPECT_FALSE(job->is_waiting());
    EXPECT_TRUE(job->is_delivering_appcache_response());
    EXPECT_FALSE(job->has_been_started());
    EXPECT_EQ(kManifestUrl, job->manifest_url());
    EXPECT_EQ(kCacheId, job->cache_id());
    EXPECT_EQ(kGroupId, job->group_id());
    EXPECT_EQ(kEntry.types(), job->entry().types());
    EXPECT_EQ(kEntry.response_id(), job->entry().response_id());

    TestFinished();
  }

  // DeliverNetworkResponse --------------------------------------------------

  void DeliverNetworkResponse() {
    // This test has async steps.
    PushNextTask(
        base::Bind(&AppCacheURLRequestJobTest::VerifyDeliverNetworkResponse,
                   base::Unretained(this)));

    AppCacheStorage* storage = service_->storage();
    request_ = empty_context_->CreateRequest(GURL("http://blah/"),
                                             net::DEFAULT_PRIORITY,
                                             url_request_delegate_.get(),
                                             NULL);

    // Setup to create an AppCacheURLRequestJob with orders to deliver
    // a network response.
    AppCacheURLRequestJob* mock_job = new AppCacheURLRequestJob(
        request_.get(), NULL, storage, NULL, false);
    job_factory_->SetJob(mock_job);
    mock_job->DeliverNetworkResponse();
    EXPECT_TRUE(mock_job->is_delivering_network_response());
    EXPECT_FALSE(mock_job->has_been_started());

    // Start the request.
    request_->Start();

    // The job should have been picked up.
    EXPECT_FALSE(job_factory_->has_job());
    // Completion is async.
  }

  void VerifyDeliverNetworkResponse() {
    EXPECT_EQ(request_->status().error(),
              net::ERR_INTERNET_DISCONNECTED);
    TestFinished();
  }

  // DeliverErrorResponse --------------------------------------------------

  void DeliverErrorResponse() {
    // This test has async steps.
    PushNextTask(
        base::Bind(&AppCacheURLRequestJobTest::VerifyDeliverErrorResponse,
                   base::Unretained(this)));

    AppCacheStorage* storage = service_->storage();
    request_ = empty_context_->CreateRequest(GURL("http://blah/"),
                                             net::DEFAULT_PRIORITY,
                                             url_request_delegate_.get(),
                                             NULL);

    // Setup to create an AppCacheURLRequestJob with orders to deliver
    // a network response.
    AppCacheURLRequestJob* mock_job = new AppCacheURLRequestJob(
        request_.get(), NULL, storage, NULL, false);
    job_factory_->SetJob(mock_job);
    mock_job->DeliverErrorResponse();
    EXPECT_TRUE(mock_job->is_delivering_error_response());
    EXPECT_FALSE(mock_job->has_been_started());

    // Start the request.
    request_->Start();

    // The job should have been picked up.
    EXPECT_FALSE(job_factory_->has_job());
    // Completion is async.
  }

  void VerifyDeliverErrorResponse() {
    EXPECT_EQ(request_->status().error(), net::ERR_FAILED);
    TestFinished();
  }

  // DeliverSmallAppCachedResponse --------------------------------------
  // "Small" being small enough to read completely in a single
  // request->Read call.

  void DeliverSmallAppCachedResponse() {
    // This test has several async steps.
    // 1. Write a small response to response storage.
    // 2. Use net::URLRequest to retrieve it.
    // 3. Verify we received what we expected to receive.

    PushNextTask(base::Bind(
        &AppCacheURLRequestJobTest::VerifyDeliverSmallAppCachedResponse,
        base::Unretained(this)));
    PushNextTask(
        base::Bind(&AppCacheURLRequestJobTest::RequestAppCachedResource,
                   base::Unretained(this), false));

    writer_.reset(service_->storage()->CreateResponseWriter(GURL(), 0));
    written_response_id_ = writer_->response_id();
    WriteBasicResponse();
    // Continues async
  }

  void RequestAppCachedResource(bool start_after_delivery_orders) {
    AppCacheStorage* storage = service_->storage();
    request_ = empty_context_->CreateRequest(GURL("http://blah/"),
                                             net::DEFAULT_PRIORITY,
                                             url_request_delegate_.get(),
                                             NULL);

    // Setup to create an AppCacheURLRequestJob with orders to deliver
    // a network response.
    scoped_refptr<AppCacheURLRequestJob> job(new AppCacheURLRequestJob(
        request_.get(), NULL, storage, NULL, false));

    if (start_after_delivery_orders) {
      job->DeliverAppCachedResponse(
          GURL(), 0, 111,
          AppCacheEntry(AppCacheEntry::EXPLICIT, written_response_id_),
          false);
      EXPECT_TRUE(job->is_delivering_appcache_response());
    }

    // Start the request.
    EXPECT_FALSE(job->has_been_started());
    job_factory_->SetJob(job.get());
    request_->Start();
    EXPECT_FALSE(job_factory_->has_job());
    EXPECT_TRUE(job->has_been_started());

    if (!start_after_delivery_orders) {
      job->DeliverAppCachedResponse(
          GURL(), 0, 111,
          AppCacheEntry(AppCacheEntry::EXPLICIT, written_response_id_),
          false);
      EXPECT_TRUE(job->is_delivering_appcache_response());
    }

    // Completion is async.
  }

  void VerifyDeliverSmallAppCachedResponse() {
    EXPECT_TRUE(request_->status().is_success());
    EXPECT_TRUE(CompareHttpResponseInfos(
        write_info_buffer_->http_info.get(),
        &url_request_delegate_->received_info_));
    EXPECT_EQ(5, url_request_delegate_->amount_received_);
    EXPECT_EQ(0, memcmp(kHttpBasicBody,
                        url_request_delegate_->received_data_->data(),
                        strlen(kHttpBasicBody)));
    TestFinished();
  }

  // DeliverLargeAppCachedResponse --------------------------------------
  // "Large" enough to require multiple calls to request->Read to complete.

  void DeliverLargeAppCachedResponse() {
    // This test has several async steps.
    // 1. Write a large response to response storage.
    // 2. Use net::URLRequest to retrieve it.
    // 3. Verify we received what we expected to receive.

    PushNextTask(base::Bind(
       &AppCacheURLRequestJobTest::VerifyDeliverLargeAppCachedResponse,
       base::Unretained(this)));
    PushNextTask(base::Bind(
       &AppCacheURLRequestJobTest::RequestAppCachedResource,
       base::Unretained(this), true));

    writer_.reset(service_->storage()->CreateResponseWriter(GURL(), 0));
    written_response_id_ = writer_->response_id();
    WriteLargeResponse();
    // Continues async
  }

  void WriteLargeResponse() {
    // 3, 1k blocks
    static const char kHttpHeaders[] =
        "HTTP/1.0 200 OK\0Content-Length: 3072\0\0";
    scoped_refptr<IOBuffer> body(new IOBuffer(kBlockSize * 3));
    char* p = body->data();
    for (int i = 0; i < 3; ++i, p += kBlockSize)
      FillData(i + 1, p, kBlockSize);
    std::string raw_headers(kHttpHeaders, arraysize(kHttpHeaders));
    WriteResponse(
        MakeHttpResponseInfo(raw_headers), body.get(), kBlockSize * 3);
  }

  void VerifyDeliverLargeAppCachedResponse() {
    EXPECT_TRUE(request_->status().is_success());
    EXPECT_TRUE(CompareHttpResponseInfos(
        write_info_buffer_->http_info.get(),
        &url_request_delegate_->received_info_));
    EXPECT_EQ(3072, url_request_delegate_->amount_received_);
    char* p = url_request_delegate_->received_data_->data();
    for (int i = 0; i < 3; ++i, p += kBlockSize)
      EXPECT_TRUE(CheckData(i + 1, p, kBlockSize));
    TestFinished();
  }

  // DeliverPartialResponse --------------------------------------

  void DeliverPartialResponse() {
    // This test has several async steps.
    // 1. Write a small response to response storage.
    // 2. Use net::URLRequest to retrieve it a subset using a range request
    // 3. Verify we received what we expected to receive.
    PushNextTask(base::Bind(
       &AppCacheURLRequestJobTest::VerifyDeliverPartialResponse,
       base::Unretained(this)));
    PushNextTask(base::Bind(
       &AppCacheURLRequestJobTest::MakeRangeRequest, base::Unretained(this)));
    writer_.reset(service_->storage()->CreateResponseWriter(GURL(), 0));
    written_response_id_ = writer_->response_id();
    WriteBasicResponse();
    // Continues async
  }

  void MakeRangeRequest() {
    AppCacheStorage* storage = service_->storage();
    request_ = empty_context_->CreateRequest(GURL("http://blah/"),
                                             net::DEFAULT_PRIORITY,
                                             url_request_delegate_.get(),
                                             NULL);

    // Request a range, the 3 middle chars out of 'Hello'
    net::HttpRequestHeaders extra_headers;
    extra_headers.SetHeader("Range", "bytes= 1-3");
    request_->SetExtraRequestHeaders(extra_headers);

    // Create job with orders to deliver an appcached entry.
    scoped_refptr<AppCacheURLRequestJob> job(new AppCacheURLRequestJob(
        request_.get(), NULL, storage, NULL, false));
    job->DeliverAppCachedResponse(
        GURL(), 0, 111,
        AppCacheEntry(AppCacheEntry::EXPLICIT, written_response_id_),
        false);
    EXPECT_TRUE(job->is_delivering_appcache_response());

    // Start the request.
    EXPECT_FALSE(job->has_been_started());
    job_factory_->SetJob(job.get());
    request_->Start();
    EXPECT_FALSE(job_factory_->has_job());
    EXPECT_TRUE(job->has_been_started());
    // Completion is async.
  }

  void VerifyDeliverPartialResponse() {
    EXPECT_TRUE(request_->status().is_success());
    EXPECT_EQ(3, url_request_delegate_->amount_received_);
    EXPECT_EQ(0, memcmp(kHttpBasicBody + 1,
                        url_request_delegate_->received_data_->data(),
                        3));
    net::HttpResponseHeaders* headers =
        url_request_delegate_->received_info_.headers.get();
    EXPECT_EQ(206, headers->response_code());
    EXPECT_EQ(3, headers->GetContentLength());
    int64 range_start, range_end, object_size;
    EXPECT_TRUE(
        headers->GetContentRange(&range_start, &range_end, &object_size));
    EXPECT_EQ(1, range_start);
    EXPECT_EQ(3, range_end);
    EXPECT_EQ(5, object_size);
    TestFinished();
  }

  // CancelRequest --------------------------------------

  void CancelRequest() {
    // This test has several async steps.
    // 1. Write a large response to response storage.
    // 2. Use net::URLRequest to retrieve it.
    // 3. Cancel the request after data starts coming in.

    PushNextTask(base::Bind(
       &AppCacheURLRequestJobTest::VerifyCancel, base::Unretained(this)));
    PushNextTask(base::Bind(
       &AppCacheURLRequestJobTest::RequestAppCachedResource,
       base::Unretained(this), true));

    writer_.reset(service_->storage()->CreateResponseWriter(GURL(), 0));
    written_response_id_ = writer_->response_id();
    WriteLargeResponse();

    url_request_delegate_->kill_after_amount_received_ = kBlockSize;
    url_request_delegate_->kill_with_io_pending_ = false;
    // Continues async
  }

  void VerifyCancel() {
    EXPECT_EQ(net::URLRequestStatus::CANCELED,
              request_->status().status());
    TestFinished();
  }

  // CancelRequestWithIOPending --------------------------------------

  void CancelRequestWithIOPending() {
    // This test has several async steps.
    // 1. Write a large response to response storage.
    // 2. Use net::URLRequest to retrieve it.
    // 3. Cancel the request after data starts coming in.

    PushNextTask(base::Bind(
       &AppCacheURLRequestJobTest::VerifyCancel, base::Unretained(this)));
    PushNextTask(base::Bind(
       &AppCacheURLRequestJobTest::RequestAppCachedResource,
       base::Unretained(this), true));

    writer_.reset(service_->storage()->CreateResponseWriter(GURL(), 0));
    written_response_id_ = writer_->response_id();
    WriteLargeResponse();

    url_request_delegate_->kill_after_amount_received_ = kBlockSize;
    url_request_delegate_->kill_with_io_pending_ = true;
    // Continues async
  }


  // Data members --------------------------------------------------------

  scoped_ptr<base::WaitableEvent> test_finished_event_;
  scoped_ptr<MockStorageDelegate> storage_delegate_;
  scoped_ptr<MockAppCacheService> service_;
  std::stack<std::pair<base::Closure, bool> > task_stack_;

  scoped_ptr<AppCacheResponseReader> reader_;
  scoped_refptr<HttpResponseInfoIOBuffer> read_info_buffer_;
  scoped_refptr<IOBuffer> read_buffer_;
  int expected_read_result_;
  int reader_deletion_count_down_;

  int64 written_response_id_;
  scoped_ptr<AppCacheResponseWriter> writer_;
  scoped_refptr<HttpResponseInfoIOBuffer> write_info_buffer_;
  scoped_refptr<IOBuffer> write_buffer_;
  int expected_write_result_;
  int writer_deletion_count_down_;

  scoped_ptr<MockURLRequestJobFactory> job_factory_;
  scoped_ptr<net::URLRequestContext> empty_context_;
  scoped_ptr<net::URLRequest> request_;
  scoped_ptr<MockURLRequestDelegate> url_request_delegate_;

  static scoped_ptr<base::Thread> io_thread_;
};

// static
scoped_ptr<base::Thread> AppCacheURLRequestJobTest::io_thread_;

TEST_F(AppCacheURLRequestJobTest, Basic) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::Basic);
}

TEST_F(AppCacheURLRequestJobTest, DeliveryOrders) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::DeliveryOrders);
}

TEST_F(AppCacheURLRequestJobTest, DeliverNetworkResponse) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::DeliverNetworkResponse);
}

TEST_F(AppCacheURLRequestJobTest, DeliverErrorResponse) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::DeliverErrorResponse);
}

TEST_F(AppCacheURLRequestJobTest, DeliverSmallAppCachedResponse) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::DeliverSmallAppCachedResponse);
}

TEST_F(AppCacheURLRequestJobTest, DeliverLargeAppCachedResponse) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::DeliverLargeAppCachedResponse);
}

TEST_F(AppCacheURLRequestJobTest, DeliverPartialResponse) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::DeliverPartialResponse);
}

TEST_F(AppCacheURLRequestJobTest, CancelRequest) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::CancelRequest);
}

TEST_F(AppCacheURLRequestJobTest, CancelRequestWithIOPending) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::CancelRequestWithIOPending);
}

}  // namespace

}  // namespace content
