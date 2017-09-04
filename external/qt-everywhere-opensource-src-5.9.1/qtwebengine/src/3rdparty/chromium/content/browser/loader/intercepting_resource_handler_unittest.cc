// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/intercepting_resource_handler.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/loader/test_resource_handler.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

class TestResourceController : public ResourceController {
 public:
  TestResourceController() = default;
  void Cancel() override {}
  void CancelAndIgnore() override {}
  void CancelWithError(int error_code) override {}
  void Resume() override { ++resume_calls_; }

  int resume_calls() const { return resume_calls_; }

 private:
  int resume_calls_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestResourceController);
};

class InterceptingResourceHandlerTest : public testing::Test {
 public:
  InterceptingResourceHandlerTest() {}

 private:
  TestBrowserThreadBundle thread_bundle_;
};

// Tests that the handler behaves properly when it doesn't have to use an
// alternate next handler.
TEST_F(InterceptingResourceHandlerTest, NoSwitching) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  std::string old_handler_body;
  std::unique_ptr<TestResourceHandler> old_handler(
      new TestResourceHandler(&old_handler_status, &old_handler_body));
  TestResourceHandler* old_test_handler = old_handler.get();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const std::string kData = "The data";
  EXPECT_NE(kData, std::string(old_test_handler->buffer()->data()));

  ASSERT_NE(read_buffer.get(), old_test_handler->buffer());
  ASSERT_GT(static_cast<size_t>(buf_size), kData.length());
  memcpy(read_buffer->data(), kData.c_str(), kData.length());

  // The response is received. The handler should not change.
  EXPECT_TRUE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  EXPECT_FALSE(defer);

  // The read is replayed by the MimeSniffingResourceHandler. The data should
  // have been received by the old intercepting_handler.
  EXPECT_TRUE(intercepting_handler->OnReadCompleted(kData.length(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(kData, std::string(old_test_handler->buffer()->data()));

  // Make sure another read behave as expected.
  buf_size = 0;
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));
  ASSERT_EQ(read_buffer.get(), old_test_handler->buffer());

  const std::string kData2 = "Data 2";
  EXPECT_NE(kData, std::string(old_test_handler->buffer()->data()));
  ASSERT_GT(static_cast<size_t>(buf_size), kData2.length());
  memcpy(read_buffer->data(), kData2.c_str(), kData2.length());

  EXPECT_TRUE(intercepting_handler->OnReadCompleted(kData2.length(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(kData2, std::string(old_test_handler->buffer()->data()));
  EXPECT_EQ(kData + kData2, old_handler_body);
}

// Tests that the data received is transmitted to the newly created
// ResourceHandler.
TEST_F(InterceptingResourceHandlerTest, HandlerSwitchNoPayload) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  std::string old_handler_body;
  std::unique_ptr<TestResourceHandler> old_handler(
      new TestResourceHandler(&old_handler_status, &old_handler_body));
  scoped_refptr<net::IOBuffer> old_buffer = old_handler.get()->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const std::string kData = "The data";
  ASSERT_NE(read_buffer.get(), old_buffer.get());
  ASSERT_GT(static_cast<size_t>(buf_size), kData.length());
  memcpy(read_buffer->data(), kData.c_str(), kData.length());

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  net::URLRequestStatus new_handler_status;
  std::string new_handler_body;
  std::unique_ptr<TestResourceHandler> new_handler_scoped(
      new TestResourceHandler(&new_handler_status, &new_handler_body));
  TestResourceHandler* new_test_handler = new_handler_scoped.get();
  intercepting_handler->UseNewHandler(std::move(new_handler_scoped),
                                      std::string());

  // The response is received. The new ResourceHandler should be used handle
  // the download.
  EXPECT_TRUE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  EXPECT_FALSE(defer);

  EXPECT_FALSE(old_handler_status.is_success());
  EXPECT_EQ(net::ERR_ABORTED, old_handler_status.error());
  EXPECT_EQ(std::string(), old_handler_body);

  // It should not have received the download data yet.
  EXPECT_NE(kData, std::string(new_test_handler->buffer()->data()));

  // The read is replayed by the MimeSniffingResourceHandler. The data should
  // have been received by the new handler.
  EXPECT_TRUE(intercepting_handler->OnReadCompleted(kData.length(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(kData, std::string(new_test_handler->buffer()->data()));

  // Make sure another read behaves as expected.
  buf_size = 0;
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));
  ASSERT_EQ(read_buffer.get(), new_test_handler->buffer());
  ASSERT_GT(static_cast<size_t>(buf_size), kData.length());

  const std::string kData2 = "Data 2";
  EXPECT_NE(kData2, std::string(new_test_handler->buffer()->data()));
  memcpy(read_buffer->data(), kData2.c_str(), kData2.length());

  EXPECT_TRUE(intercepting_handler->OnReadCompleted(kData2.length(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(kData2, std::string(new_test_handler->buffer()->data()));
  EXPECT_EQ(kData + kData2, new_handler_body);
}

// Tests that the data received is transmitted to the newly created
// ResourceHandler and the specified payload to the old ResourceHandler.
TEST_F(InterceptingResourceHandlerTest, HandlerSwitchWithPayload) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  std::string old_handler_body;
  std::unique_ptr<TestResourceHandler> old_handler_scoped(
      new TestResourceHandler(&old_handler_status, &old_handler_body));
  TestResourceHandler* old_handler = old_handler_scoped.get();
  scoped_refptr<net::IOBuffer> old_buffer = old_handler->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler_scoped),
                                      request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const std::string kData = "The data";
  ASSERT_NE(read_buffer.get(), old_buffer.get());
  ASSERT_GT(static_cast<size_t>(buf_size), kData.length());
  memcpy(read_buffer->data(), kData.c_str(), kData.length());

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  const std::string kPayload = "The payload";
  net::URLRequestStatus new_handler_status;
  std::string new_handler_body;
  std::unique_ptr<TestResourceHandler> new_handler_scoped(
      new TestResourceHandler(&new_handler_status, &new_handler_body));
  TestResourceHandler* new_test_handler = new_handler_scoped.get();
  intercepting_handler->UseNewHandler(std::move(new_handler_scoped), kPayload);

  // The old handler should not have received the payload yet.
  ASSERT_EQ(std::string(), old_handler_body);

  // The response is received. The new ResourceHandler should be used to handle
  // the download.
  EXPECT_TRUE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  EXPECT_FALSE(defer);

  // The old handler should have received the payload.
  EXPECT_EQ(kPayload, old_handler_body);

  EXPECT_TRUE(old_handler_status.is_success());
  EXPECT_EQ(net::OK, old_handler_status.error());

  // It should not have received the download data yet.
  EXPECT_NE(kData, std::string(new_test_handler->buffer()->data()));

  // The read is replayed by the MimeSniffingResourceHandler. The data should
  // have been received by the new handler.
  EXPECT_TRUE(intercepting_handler->OnReadCompleted(kData.length(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(kData, std::string(new_test_handler->buffer()->data()));

  // Make sure another read behave as expected.
  buf_size = 0;
  const std::string kData2 = "Data 2";
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));
  ASSERT_EQ(read_buffer.get(), new_test_handler->buffer());
  ASSERT_GT(static_cast<size_t>(buf_size), kData.length());

  EXPECT_NE(kData2, std::string(new_test_handler->buffer()->data()));
  memcpy(read_buffer->data(), kData2.c_str(), kData2.length());

  EXPECT_TRUE(intercepting_handler->OnReadCompleted(kData2.length(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(kData2, std::string(new_test_handler->buffer()->data()));
  EXPECT_EQ(kData + kData2, new_handler_body);
}

// Tests that the handler behaves properly if the old handler fails will read.
TEST_F(InterceptingResourceHandlerTest, OldHandlerFailsWillRead) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  std::string old_handler_body;
  std::unique_ptr<TestResourceHandler> old_handler(
      new TestResourceHandler(&old_handler_status, &old_handler_body));
  old_handler->set_on_will_read_result(false);
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data. The old
  // handler should tell the caller to fail.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_FALSE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));
}

// Tests that the handler behaves properly if the new handler fails in
// OnWillStart.
TEST_F(InterceptingResourceHandlerTest, NewHandlerFailsOnWillStart) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  std::string old_handler_body;
  std::unique_ptr<TestResourceHandler> old_handler(
      new TestResourceHandler(&old_handler_status, &old_handler_body));
  scoped_refptr<net::IOBuffer> old_buffer = old_handler.get()->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const char kData[] = "The data";
  ASSERT_NE(read_buffer.get(), old_buffer.get());
  ASSERT_GT(static_cast<size_t>(buf_size), sizeof(kData));
  memcpy(read_buffer->data(), kData, sizeof(kData));

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  net::URLRequestStatus new_handler_status;
  std::string new_handler_body;
  std::unique_ptr<TestResourceHandler> new_handler(
      new TestResourceHandler(&new_handler_status, &new_handler_body));
  new_handler->set_on_will_start_result(false);
  intercepting_handler->UseNewHandler(std::move(new_handler), std::string());

  // The response is received. The new ResourceHandler should tell us to fail.
  EXPECT_FALSE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  EXPECT_FALSE(defer);
}

// Tests that the handler behaves properly if the new handler fails response
// started.
TEST_F(InterceptingResourceHandlerTest, NewHandlerFailsResponseStarted) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  std::string old_handler_body;
  std::unique_ptr<TestResourceHandler> old_handler(
      new TestResourceHandler(&old_handler_status, &old_handler_body));
  scoped_refptr<net::IOBuffer> old_buffer = old_handler.get()->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const char kData[] = "The data";
  ASSERT_NE(read_buffer.get(), old_buffer.get());
  ASSERT_GT(static_cast<size_t>(buf_size), sizeof(kData));
  memcpy(read_buffer->data(), kData, sizeof(kData));

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  net::URLRequestStatus new_handler_status;
  std::string new_handler_body;
  std::unique_ptr<TestResourceHandler> new_handler(
      new TestResourceHandler(&new_handler_status, &new_handler_body));
  new_handler->set_on_response_started_result(false);
  intercepting_handler->UseNewHandler(std::move(new_handler), std::string());

  // The response is received. The new ResourceHandler should tell us to fail.
  EXPECT_FALSE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  EXPECT_FALSE(defer);
}

// Tests that the handler behaves properly if the new handler fails will read.
TEST_F(InterceptingResourceHandlerTest, NewHandlerFailsWillRead) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  std::string old_handler_body;
  std::unique_ptr<TestResourceHandler> old_handler(
      new TestResourceHandler(&old_handler_status, &old_handler_body));
  scoped_refptr<net::IOBuffer> old_buffer = old_handler.get()->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const char kData[] = "The data";
  ASSERT_NE(read_buffer.get(), old_buffer.get());
  ASSERT_GT(static_cast<size_t>(buf_size), sizeof(kData));
  memcpy(read_buffer->data(), kData, sizeof(kData));

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  net::URLRequestStatus new_handler_status;
  std::string new_handler_body;
  std::unique_ptr<TestResourceHandler> new_handler(
      new TestResourceHandler(&new_handler_status, &new_handler_body));
  new_handler->set_on_will_read_result(false);
  intercepting_handler->UseNewHandler(std::move(new_handler), std::string());

  // The response is received. The new handler should not have been asked to
  // read yet.
  EXPECT_TRUE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(net::URLRequestStatus::CANCELED, old_handler_status.status());
  EXPECT_EQ(net::ERR_ABORTED, old_handler_status.error());

  // The read is replayed by the MimeSniffingResourceHandler. The new
  // handler should tell the caller to fail.
  EXPECT_FALSE(intercepting_handler->OnReadCompleted(sizeof(kData), &defer));
  EXPECT_FALSE(defer);
}

// Tests that the handler behaves properly if the new handler fails read
// completed.
TEST_F(InterceptingResourceHandlerTest, NewHandlerFailsReadCompleted) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  std::string old_handler_body;
  std::unique_ptr<TestResourceHandler> old_handler(
      new TestResourceHandler(&old_handler_status, &old_handler_body));
  scoped_refptr<net::IOBuffer> old_buffer = old_handler.get()->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const char kData[] = "The data";
  ASSERT_NE(read_buffer.get(), old_buffer.get());
  ASSERT_GT(static_cast<size_t>(buf_size), sizeof(kData));
  memcpy(read_buffer->data(), kData, sizeof(kData));

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  net::URLRequestStatus new_handler_status;
  std::string new_handler_body;
  std::unique_ptr<TestResourceHandler> new_handler(
      new TestResourceHandler(&new_handler_status, &new_handler_body));
  new_handler->set_on_read_completed_result(false);
  intercepting_handler->UseNewHandler(std::move(new_handler), std::string());

  // The response is received.
  EXPECT_TRUE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(net::URLRequestStatus::CANCELED, old_handler_status.status());
  EXPECT_EQ(net::ERR_ABORTED, old_handler_status.error());

  // The read is replayed by the MimeSniffingResourceHandler. The new handler
  // should tell the caller to fail.
  EXPECT_FALSE(intercepting_handler->OnReadCompleted(sizeof(kData), &defer));
  EXPECT_FALSE(defer);
}

// The old handler sets |defer| to true in OnReadCompleted and
// OnResponseCompleted. The new handler sets |defer| to true in
// OnResponseStarted and OnReadCompleted.
TEST_F(InterceptingResourceHandlerTest, DeferredOperations) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  std::unique_ptr<TestResourceController> resource_controller =
      base::MakeUnique<TestResourceController>();
  net::URLRequestStatus old_handler_status = {net::URLRequestStatus::IO_PENDING,
                                              0};
  std::string old_handler_body;
  std::unique_ptr<TestResourceHandler> old_handler(
      new TestResourceHandler(&old_handler_status, &old_handler_body));
  old_handler->SetBufferSize(10);
  old_handler->set_defer_on_read_completed(true);

  scoped_refptr<net::IOBuffer> old_buffer = old_handler.get()->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));
  intercepting_handler->SetController(resource_controller.get());

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const char kData[] = "The data";
  ASSERT_NE(read_buffer.get(), old_buffer.get());
  ASSERT_GT(static_cast<size_t>(buf_size), strlen(kData));
  memcpy(read_buffer->data(), kData, strlen(kData));

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  net::URLRequestStatus new_handler_status = {net::URLRequestStatus::IO_PENDING,
                                              0};

  std::string new_handler_body;
  const std::string kPayload = "The long long long long long payload";
  ASSERT_GT(kPayload.size(), static_cast<size_t>(buf_size));
  std::unique_ptr<TestResourceHandler> new_handler(
      new TestResourceHandler(&new_handler_status, &new_handler_body));
  new_handler->SetBufferSize(1);
  new_handler->set_defer_on_will_start(true);
  new_handler->set_defer_on_response_started(true);
  new_handler->set_defer_on_read_completed(true);
  new_handler->set_defer_on_response_completed(true);
  intercepting_handler->UseNewHandler(std::move(new_handler), kPayload);

  // The response is received, and then deferred by the old handler's
  // OnReadCompleted method.
  ASSERT_TRUE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  ASSERT_TRUE(defer);

  // The old handler has received the first N bytes of the payload synchronously
  // where N is the size of the buffer exposed via OnWillRead.
  EXPECT_EQ("The long l", old_handler_body);
  EXPECT_EQ(std::string(), new_handler_body);
  EXPECT_EQ(old_handler_status.status(), net::URLRequestStatus::IO_PENDING);
  EXPECT_EQ(new_handler_status.status(), net::URLRequestStatus::IO_PENDING);

  // Run until the new handler's OnWillStart method defers the request.
  intercepting_handler->Resume();
  EXPECT_EQ(0, resource_controller->resume_calls());
  EXPECT_EQ(kPayload, old_handler_body);
  EXPECT_EQ(std::string(), new_handler_body);
  EXPECT_EQ(old_handler_status.status(), net::URLRequestStatus::SUCCESS);
  EXPECT_EQ(new_handler_status.status(), net::URLRequestStatus::IO_PENDING);

  // Run until the new handler's OnResponseStarted method defers the request.
  intercepting_handler->Resume();
  EXPECT_EQ(0, resource_controller->resume_calls());
  EXPECT_EQ(std::string(), new_handler_body);
  EXPECT_EQ(old_handler_status.status(), net::URLRequestStatus::SUCCESS);
  EXPECT_EQ(new_handler_status.status(), net::URLRequestStatus::IO_PENDING);

  // Resuming should finally call back into the ResourceController.
  intercepting_handler->Resume();
  EXPECT_EQ(1, resource_controller->resume_calls());

  // Data is read, the new handler defers completion of the read.
  defer = false;
  ASSERT_TRUE(intercepting_handler->OnReadCompleted(strlen(kData), &defer));
  ASSERT_TRUE(defer);

  EXPECT_EQ(kPayload, old_handler_body);
  EXPECT_EQ("T", new_handler_body);

  intercepting_handler->Resume();
  EXPECT_EQ(2, resource_controller->resume_calls());
  EXPECT_EQ(kPayload, old_handler_body);
  EXPECT_EQ(kData, new_handler_body);

  EXPECT_EQ(old_handler_status.status(), net::URLRequestStatus::SUCCESS);
  EXPECT_EQ(new_handler_status.status(), net::URLRequestStatus::IO_PENDING);

  defer = false;
  intercepting_handler->OnResponseCompleted({net::URLRequestStatus::SUCCESS, 0},
                                            &defer);
  ASSERT_TRUE(defer);
  EXPECT_EQ(old_handler_status.status(), net::URLRequestStatus::SUCCESS);
  EXPECT_EQ(new_handler_status.status(), net::URLRequestStatus::SUCCESS);
}

// Test cancellation where there is only the old handler in an
// InterceptingResourceHandler.
TEST_F(InterceptingResourceHandlerTest, CancelOldHandler) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  std::unique_ptr<TestResourceController> resource_controller =
      base::MakeUnique<TestResourceController>();
  net::URLRequestStatus old_handler_status = {net::URLRequestStatus::IO_PENDING,
                                              0};
  std::string old_handler_body;
  std::unique_ptr<TestResourceHandler> old_handler(
      new TestResourceHandler(&old_handler_status, &old_handler_body));

  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));
  intercepting_handler->SetController(resource_controller.get());

  EXPECT_EQ(net::URLRequestStatus::IO_PENDING, old_handler_status.status());

  bool defer = false;
  intercepting_handler->OnResponseCompleted(
      {net::URLRequestStatus::CANCELED, net::ERR_FAILED}, &defer);
  ASSERT_FALSE(defer);
  EXPECT_EQ(0, resource_controller->resume_calls());
  EXPECT_EQ(net::URLRequestStatus::CANCELED, old_handler_status.status());
}

// Test cancellation where there is only the new handler in an
// InterceptingResourceHandler.
TEST_F(InterceptingResourceHandlerTest, CancelNewHandler) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  std::unique_ptr<TestResourceController> resource_controller =
      base::MakeUnique<TestResourceController>();
  net::URLRequestStatus old_handler_status = {net::URLRequestStatus::IO_PENDING,
                                              0};
  std::string old_handler_body;
  std::unique_ptr<TestResourceHandler> old_handler(
      new TestResourceHandler(&old_handler_status, &old_handler_body));

  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));
  intercepting_handler->SetController(resource_controller.get());

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const char kData[] = "The data";
  ASSERT_GT(static_cast<size_t>(buf_size), strlen(kData));
  memcpy(read_buffer->data(), kData, strlen(kData));

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  net::URLRequestStatus new_handler_status = {net::URLRequestStatus::IO_PENDING,
                                              0};

  std::string new_handler_body;
  const std::string kPayload = "The payload";
  std::unique_ptr<TestResourceHandler> new_handler(
      new TestResourceHandler(&new_handler_status, &new_handler_body));
  new_handler->SetBufferSize(1);
  new_handler->set_defer_on_response_started(true);
  new_handler->set_defer_on_response_completed(true);
  intercepting_handler->UseNewHandler(std::move(new_handler), kPayload);

  // The response is received.
  ASSERT_TRUE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  ASSERT_TRUE(defer);

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, old_handler_status.status());
  EXPECT_EQ(net::URLRequestStatus::IO_PENDING, new_handler_status.status());

  defer = false;
  intercepting_handler->OnResponseCompleted(
      {net::URLRequestStatus::CANCELED, net::ERR_FAILED}, &defer);
  ASSERT_TRUE(defer);
  EXPECT_EQ(0, resource_controller->resume_calls());
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, old_handler_status.status());
  EXPECT_EQ(net::URLRequestStatus::CANCELED, new_handler_status.status());
}

// Test cancellation where there are both the old and the new handlers in an
// InterceptingResourceHandler.
TEST_F(InterceptingResourceHandlerTest, CancelBothHandlers) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  std::unique_ptr<TestResourceController> resource_controller =
      base::MakeUnique<TestResourceController>();
  net::URLRequestStatus old_handler_status = {net::URLRequestStatus::IO_PENDING,
                                              0};
  std::string old_handler_body;
  std::unique_ptr<TestResourceHandler> old_handler(
      new TestResourceHandler(&old_handler_status, &old_handler_body));
  old_handler->set_defer_on_read_completed(true);

  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));
  intercepting_handler->SetController(resource_controller.get());

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const char kData[] = "The data";
  ASSERT_GT(static_cast<size_t>(buf_size), strlen(kData));
  memcpy(read_buffer->data(), kData, strlen(kData));

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  net::URLRequestStatus new_handler_status = {net::URLRequestStatus::IO_PENDING,
                                              0};

  std::string new_handler_body;
  const std::string kPayload = "The payload";
  std::unique_ptr<TestResourceHandler> new_handler(
      new TestResourceHandler(&new_handler_status, &new_handler_body));
  new_handler->SetBufferSize(1);
  new_handler->set_defer_on_response_completed(true);
  intercepting_handler->UseNewHandler(std::move(new_handler), kPayload);

  // The response is received.
  ASSERT_TRUE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  ASSERT_TRUE(defer);

  EXPECT_EQ(net::URLRequestStatus::IO_PENDING, old_handler_status.status());
  EXPECT_EQ(net::URLRequestStatus::IO_PENDING, new_handler_status.status());

  defer = false;
  intercepting_handler->OnResponseCompleted(
      {net::URLRequestStatus::CANCELED, net::ERR_FAILED}, &defer);
  ASSERT_TRUE(defer);
  EXPECT_EQ(0, resource_controller->resume_calls());
  EXPECT_EQ(net::URLRequestStatus::CANCELED, old_handler_status.status());
  EXPECT_EQ(net::URLRequestStatus::CANCELED, new_handler_status.status());
}

}  // namespace

}  // namespace content
