// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "content/renderer/media/buffered_resource_loader.h"
#include "content/test/mock_webframeclient.h"
#include "content/test/mock_weburlloader.h"
#include "media/base/media_log.h"
#include "media/base/seekable_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::Truly;
using ::testing::NiceMock;

using blink::WebLocalFrame;
using blink::WebString;
using blink::WebURLError;
using blink::WebURLResponse;
using blink::WebView;

namespace content {

static const char* kHttpUrl = "http://test";
static const char kHttpRedirectToSameDomainUrl1[] = "http://test/ing";
static const char kHttpRedirectToSameDomainUrl2[] = "http://test/ing2";
static const char kHttpRedirectToDifferentDomainUrl1[] = "http://test2";

static const int kDataSize = 1024;
static const int kHttpOK = 200;
static const int kHttpPartialContent = 206;

enum NetworkState {
  NONE,
  LOADED,
  LOADING
};

// Predicate that tests that request disallows compressed data.
static bool CorrectAcceptEncoding(const blink::WebURLRequest &request) {
  std::string value = request.httpHeaderField(
      WebString::fromUTF8(net::HttpRequestHeaders::kAcceptEncoding)).utf8();
  return (value.find("identity;q=1") != std::string::npos) &&
         (value.find("*;q=0") != std::string::npos);
}

class BufferedResourceLoaderTest : public testing::Test {
 public:
  BufferedResourceLoaderTest()
      : view_(WebView::create(NULL)), frame_(WebLocalFrame::create(&client_)) {
    view_->setMainFrame(frame_);

    for (int i = 0; i < kDataSize; ++i) {
      data_[i] = i;
    }
  }

  virtual ~BufferedResourceLoaderTest() {
    view_->close();
    frame_->close();
  }

  void Initialize(const char* url, int first_position, int last_position) {
    gurl_ = GURL(url);
    first_position_ = first_position;
    last_position_ = last_position;

    loader_.reset(new BufferedResourceLoader(
        gurl_, BufferedResourceLoader::kUnspecified,
        first_position_, last_position_,
        BufferedResourceLoader::kCapacityDefer, 0, 0,
        new media::MediaLog()));

    // |test_loader_| will be used when Start() is called.
    url_loader_ = new NiceMock<MockWebURLLoader>();
    loader_->test_loader_ = scoped_ptr<blink::WebURLLoader>(url_loader_);
  }

  void SetLoaderBuffer(int forward_capacity, int backward_capacity) {
    loader_->buffer_.set_forward_capacity(forward_capacity);
    loader_->buffer_.set_backward_capacity(backward_capacity);
    loader_->buffer_.Clear();
  }

  void Start() {
    InSequence s;
    EXPECT_CALL(*url_loader_, loadAsynchronously(Truly(CorrectAcceptEncoding),
                                                 loader_.get()));

    EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoading));
    loader_->Start(
        base::Bind(&BufferedResourceLoaderTest::StartCallback,
                   base::Unretained(this)),
        base::Bind(&BufferedResourceLoaderTest::LoadingCallback,
                   base::Unretained(this)),
        base::Bind(&BufferedResourceLoaderTest::ProgressCallback,
                   base::Unretained(this)),
        view_->mainFrame());
  }

  void FullResponse(int64 instance_size) {
    FullResponse(instance_size, BufferedResourceLoader::kOk);
  }

  void FullResponse(int64 instance_size,
                    BufferedResourceLoader::Status status) {
    EXPECT_CALL(*this, StartCallback(status));

    WebURLResponse response(gurl_);
    response.setHTTPHeaderField(WebString::fromUTF8("Content-Length"),
                                WebString::fromUTF8(base::StringPrintf("%"
                                    PRId64, instance_size)));
    response.setExpectedContentLength(instance_size);
    response.setHTTPStatusCode(kHttpOK);
    loader_->didReceiveResponse(url_loader_, response);

    if (status == BufferedResourceLoader::kOk) {
      EXPECT_EQ(instance_size, loader_->content_length());
      EXPECT_EQ(instance_size, loader_->instance_size());
    }

    EXPECT_FALSE(loader_->range_supported());
  }

  void PartialResponse(int64 first_position, int64 last_position,
                       int64 instance_size) {
    PartialResponse(first_position, last_position, instance_size, false, true);
  }

  void PartialResponse(int64 first_position, int64 last_position,
                       int64 instance_size, bool chunked, bool accept_ranges) {
    EXPECT_CALL(*this, StartCallback(BufferedResourceLoader::kOk));

    WebURLResponse response(gurl_);
    response.setHTTPHeaderField(WebString::fromUTF8("Content-Range"),
                                WebString::fromUTF8(base::StringPrintf("bytes "
                                            "%" PRId64 "-%" PRId64 "/%" PRId64,
                                            first_position,
                                            last_position,
                                            instance_size)));

    // HTTP 1.1 doesn't permit Content-Length with Transfer-Encoding: chunked.
    int64 content_length = -1;
    if (chunked) {
      response.setHTTPHeaderField(WebString::fromUTF8("Transfer-Encoding"),
                                  WebString::fromUTF8("chunked"));
    } else {
      content_length = last_position - first_position + 1;
    }
    response.setExpectedContentLength(content_length);

    // A server isn't required to return Accept-Ranges even though it might.
    if (accept_ranges) {
      response.setHTTPHeaderField(WebString::fromUTF8("Accept-Ranges"),
                                  WebString::fromUTF8("bytes"));
    }

    response.setHTTPStatusCode(kHttpPartialContent);
    loader_->didReceiveResponse(url_loader_, response);

    // XXX: what's the difference between these two? For example in the chunked
    // range request case, Content-Length is unspecified (because it's chunked)
    // but Content-Range: a-b/c can be returned, where c == Content-Length
    //
    // Can we eliminate one?
    EXPECT_EQ(content_length, loader_->content_length());
    EXPECT_EQ(instance_size, loader_->instance_size());

    // A valid partial response should always result in this being true.
    EXPECT_TRUE(loader_->range_supported());
  }

  void Redirect(const char* url) {
    GURL redirectUrl(url);
    blink::WebURLRequest newRequest(redirectUrl);
    blink::WebURLResponse redirectResponse(gurl_);

    loader_->willSendRequest(url_loader_, newRequest, redirectResponse);

    base::MessageLoop::current()->RunUntilIdle();
  }

  void StopWhenLoad() {
    InSequence s;
    EXPECT_CALL(*url_loader_, cancel());
    loader_->Stop();
    loader_.reset();
  }

  // Helper method to write to |loader_| from |data_|.
  void WriteLoader(int position, int size) {
    EXPECT_CALL(*this, ProgressCallback(position + size - 1));
    loader_->didReceiveData(url_loader_,
                            reinterpret_cast<char*>(data_ + position),
                            size,
                            size);
  }

  void WriteData(int size) {
    EXPECT_CALL(*this, ProgressCallback(_));

    scoped_ptr<char[]> data(new char[size]);
    loader_->didReceiveData(url_loader_, data.get(), size, size);
  }

  void WriteUntilThreshold() {
    int buffered = loader_->buffer_.forward_bytes();
    int capacity = loader_->buffer_.forward_capacity();
    CHECK_LT(buffered, capacity);

    EXPECT_CALL(*this, LoadingCallback(
        BufferedResourceLoader::kLoadingDeferred));
    WriteData(capacity - buffered);
  }

  // Helper method to read from |loader_|.
  void ReadLoader(int64 position, int size, uint8* buffer) {
    loader_->Read(position, size, buffer,
                  base::Bind(&BufferedResourceLoaderTest::ReadCallback,
                             base::Unretained(this)));
  }

  // Verifies that data in buffer[0...size] is equal to data_[pos...pos+size].
  void VerifyBuffer(uint8* buffer, int pos, int size) {
    EXPECT_EQ(0, memcmp(buffer, data_ + pos, size));
  }

  void ConfirmLoaderOffsets(int64 expected_offset,
                            int expected_first_offset,
                            int expected_last_offset) {
    EXPECT_EQ(loader_->offset_, expected_offset);
    EXPECT_EQ(loader_->first_offset_, expected_first_offset);
    EXPECT_EQ(loader_->last_offset_, expected_last_offset);
  }

  void ConfirmBufferState(int backward_bytes,
                          int backward_capacity,
                          int forward_bytes,
                          int forward_capacity) {
    EXPECT_EQ(backward_bytes, loader_->buffer_.backward_bytes());
    EXPECT_EQ(backward_capacity, loader_->buffer_.backward_capacity());
    EXPECT_EQ(forward_bytes, loader_->buffer_.forward_bytes());
    EXPECT_EQ(forward_capacity, loader_->buffer_.forward_capacity());
  }

  void ConfirmLoaderBufferBackwardCapacity(int expected_backward_capacity) {
    EXPECT_EQ(loader_->buffer_.backward_capacity(),
              expected_backward_capacity);
  }

  void ConfirmLoaderBufferForwardCapacity(int expected_forward_capacity) {
    EXPECT_EQ(loader_->buffer_.forward_capacity(), expected_forward_capacity);
  }

  // Makes sure the |loader_| buffer window is in a reasonable range.
  void CheckBufferWindowBounds() {
    // Corresponds to value defined in buffered_resource_loader.cc.
    static const int kMinBufferCapacity = 2 * 1024 * 1024;
    EXPECT_GE(loader_->buffer_.forward_capacity(), kMinBufferCapacity);
    EXPECT_GE(loader_->buffer_.backward_capacity(), kMinBufferCapacity);

    // Corresponds to value defined in buffered_resource_loader.cc.
    static const int kMaxBufferCapacity = 20 * 1024 * 1024;
    EXPECT_LE(loader_->buffer_.forward_capacity(), kMaxBufferCapacity);
    EXPECT_LE(loader_->buffer_.backward_capacity(), kMaxBufferCapacity);
  }

  MOCK_METHOD1(StartCallback, void(BufferedResourceLoader::Status));
  MOCK_METHOD2(ReadCallback, void(BufferedResourceLoader::Status, int));
  MOCK_METHOD1(LoadingCallback, void(BufferedResourceLoader::LoadingState));
  MOCK_METHOD1(ProgressCallback, void(int64));

 protected:
  GURL gurl_;
  int64 first_position_;
  int64 last_position_;

  scoped_ptr<BufferedResourceLoader> loader_;
  NiceMock<MockWebURLLoader>* url_loader_;

  MockWebFrameClient client_;
  WebView* view_;
  WebLocalFrame* frame_;

  base::MessageLoop message_loop_;

  uint8 data_[kDataSize];

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedResourceLoaderTest);
};

TEST_F(BufferedResourceLoaderTest, StartStop) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  StopWhenLoad();
}

// Tests that a bad HTTP response is recived, e.g. file not found.
TEST_F(BufferedResourceLoaderTest, BadHttpResponse) {
  Initialize(kHttpUrl, -1, -1);
  Start();

  EXPECT_CALL(*this, StartCallback(BufferedResourceLoader::kFailed));

  WebURLResponse response(gurl_);
  response.setHTTPStatusCode(404);
  response.setHTTPStatusText("Not Found\n");
  loader_->didReceiveResponse(url_loader_, response);
  StopWhenLoad();
}

// Tests that partial content is requested but not fulfilled.
TEST_F(BufferedResourceLoaderTest, NotPartialResponse) {
  Initialize(kHttpUrl, 100, -1);
  Start();
  FullResponse(1024, BufferedResourceLoader::kFailed);
  StopWhenLoad();
}

// Tests that a 200 response is received.
TEST_F(BufferedResourceLoaderTest, FullResponse) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  FullResponse(1024);
  StopWhenLoad();
}

// Tests that a partial content response is received.
TEST_F(BufferedResourceLoaderTest, PartialResponse) {
  Initialize(kHttpUrl, 100, 200);
  Start();
  PartialResponse(100, 200, 1024);
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, PartialResponse_Chunked) {
  Initialize(kHttpUrl, 100, 200);
  Start();
  PartialResponse(100, 200, 1024, true, true);
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, PartialResponse_NoAcceptRanges) {
  Initialize(kHttpUrl, 100, 200);
  Start();
  PartialResponse(100, 200, 1024, false, false);
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, PartialResponse_ChunkedNoAcceptRanges) {
  Initialize(kHttpUrl, 100, 200);
  Start();
  PartialResponse(100, 200, 1024, true, false);
  StopWhenLoad();
}

// Tests that an invalid partial response is received.
TEST_F(BufferedResourceLoaderTest, InvalidPartialResponse) {
  Initialize(kHttpUrl, 0, 10);
  Start();

  EXPECT_CALL(*this, StartCallback(BufferedResourceLoader::kFailed));

  WebURLResponse response(gurl_);
  response.setHTTPHeaderField(WebString::fromUTF8("Content-Range"),
                              WebString::fromUTF8(base::StringPrintf("bytes "
                                  "%d-%d/%d", 1, 10, 1024)));
  response.setExpectedContentLength(10);
  response.setHTTPStatusCode(kHttpPartialContent);
  loader_->didReceiveResponse(url_loader_, response);
  StopWhenLoad();
}

// Tests the logic of sliding window for data buffering and reading.
TEST_F(BufferedResourceLoaderTest, BufferAndRead) {
  Initialize(kHttpUrl, 10, 29);
  loader_->UpdateDeferStrategy(BufferedResourceLoader::kCapacityDefer);
  Start();
  PartialResponse(10, 29, 30);

  uint8 buffer[10];
  InSequence s;

  // Writes 10 bytes and read them back.
  WriteLoader(10, 10);
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 10));
  ReadLoader(10, 10, buffer);
  VerifyBuffer(buffer, 10, 10);

  // Writes 10 bytes and read 2 times.
  WriteLoader(20, 10);
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 5));
  ReadLoader(20, 5, buffer);
  VerifyBuffer(buffer, 20, 5);
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 5));
  ReadLoader(25, 5, buffer);
  VerifyBuffer(buffer, 25, 5);

  // Read backward within buffer.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 10));
  ReadLoader(10, 10, buffer);
  VerifyBuffer(buffer, 10, 10);

  // Read backward outside buffer.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kCacheMiss, 0));
  ReadLoader(9, 10, buffer);

  // Response has completed.
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoadingFinished));
  loader_->didFinishLoading(url_loader_, 0, -1);

  // Try to read 10 from position 25 will just return with 5 bytes.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 5));
  ReadLoader(25, 10, buffer);
  VerifyBuffer(buffer, 25, 5);

  // Try to read outside buffered range after request has completed.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kCacheMiss, 0));
  ReadLoader(5, 10, buffer);

  // Try to read beyond the instance size.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 0));
  ReadLoader(30, 10, buffer);
}

// Tests the logic of expanding the data buffer for large reads.
TEST_F(BufferedResourceLoaderTest, ReadExtendBuffer) {
  Initialize(kHttpUrl, 10, 0x014FFFFFF);
  SetLoaderBuffer(10, 20);
  Start();
  PartialResponse(10, 0x014FFFFFF, 0x015000000);

  uint8 buffer[20];
  InSequence s;

  // Write more than forward capacity and read it back. Ensure forward capacity
  // gets reset after reading.
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoadingDeferred));
  WriteLoader(10, 20);

  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 20));
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoading));
  ReadLoader(10, 20, buffer);

  VerifyBuffer(buffer, 10, 20);
  ConfirmLoaderBufferForwardCapacity(10);

  // Make and outstanding read request larger than forward capacity. Ensure
  // forward capacity gets extended.
  ReadLoader(30, 20, buffer);
  ConfirmLoaderBufferForwardCapacity(20);

  // Fulfill outstanding request. Ensure forward capacity gets reset.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 20));
  WriteLoader(30, 20);

  VerifyBuffer(buffer, 30, 20);
  ConfirmLoaderBufferForwardCapacity(10);

  // Try to read further ahead than kForwardWaitThreshold allows. Ensure
  // forward capacity is not changed.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kCacheMiss, 0));
  ReadLoader(0x00300000, 1, buffer);

  ConfirmLoaderBufferForwardCapacity(10);

  // Try to read more than maximum forward capacity. Ensure forward capacity is
  // not changed.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kFailed, 0));
  ReadLoader(30, 0x01400001, buffer);

  ConfirmLoaderBufferForwardCapacity(10);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, ReadOutsideBuffer) {
  Initialize(kHttpUrl, 10, 0x00FFFFFF);
  Start();
  PartialResponse(10, 0x00FFFFFF, 0x01000000);

  uint8 buffer[10];
  InSequence s;

  // Read very far ahead will get a cache miss.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kCacheMiss, 0));
  ReadLoader(0x00FFFFFF, 1, buffer);

  // The following call will not call ReadCallback() because it is waiting for
  // data to arrive.
  ReadLoader(10, 10, buffer);

  // Writing to loader will fulfill the read request.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 10));
  WriteLoader(10, 20);
  VerifyBuffer(buffer, 10, 10);

  // The following call cannot be fulfilled now.
  ReadLoader(25, 10, buffer);

  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoadingFinished));
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 5));
  loader_->didFinishLoading(url_loader_, 0, -1);
}

TEST_F(BufferedResourceLoaderTest, RequestFailedWhenRead) {
  Initialize(kHttpUrl, 10, 29);
  Start();
  PartialResponse(10, 29, 30);

  uint8 buffer[10];
  InSequence s;

  // We should convert any error we receive to BufferedResourceLoader::kFailed.
  ReadLoader(10, 10, buffer);
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoadingFailed));
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kFailed, 0));
  WebURLError error;
  error.reason = net::ERR_TIMED_OUT;
  error.isCancellation = false;
  loader_->didFail(url_loader_, error);
}

TEST_F(BufferedResourceLoaderTest, RequestFailedWithNoPendingReads) {
  Initialize(kHttpUrl, 10, 29);
  Start();
  PartialResponse(10, 29, 30);

  uint8 buffer[10];
  InSequence s;

  // Write enough data so that a read would technically complete had the request
  // not failed.
  WriteLoader(10, 20);

  // Fail without a pending read.
  WebURLError error;
  error.reason = net::ERR_TIMED_OUT;
  error.isCancellation = false;
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoadingFailed));
  loader_->didFail(url_loader_, error);

  // Now we should immediately fail any read even if we have data buffered.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kFailed, 0));
  ReadLoader(10, 10, buffer);
}

TEST_F(BufferedResourceLoaderTest, RequestCancelledWhenRead) {
  Initialize(kHttpUrl, 10, 29);
  Start();
  PartialResponse(10, 29, 30);

  uint8 buffer[10];
  InSequence s;

  // We should convert any error we receive to BufferedResourceLoader::kFailed.
  ReadLoader(10, 10, buffer);
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoadingFailed));
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kFailed, 0));
  WebURLError error;
  error.reason = 0;
  error.isCancellation = true;
  loader_->didFail(url_loader_, error);
}

// Tests the data buffering logic of NeverDefer strategy.
TEST_F(BufferedResourceLoaderTest, NeverDeferStrategy) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 20);
  loader_->UpdateDeferStrategy(BufferedResourceLoader::kNeverDefer);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[10];

  // Read past the buffer size; should not defer regardless.
  WriteLoader(10, 10);
  WriteLoader(20, 50);

  // Should move past window.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kCacheMiss, 0));
  ReadLoader(10, 10, buffer);

  StopWhenLoad();
}

// Tests the data buffering logic of ReadThenDefer strategy.
TEST_F(BufferedResourceLoaderTest, ReadThenDeferStrategy) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 20);
  loader_->UpdateDeferStrategy(BufferedResourceLoader::kReadThenDefer);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[10];

  // Make an outstanding read request.
  ReadLoader(10, 10, buffer);

  // Receive almost enough data to cover, shouldn't defer.
  WriteLoader(10, 9);

  // As soon as we have received enough data to fulfill the read, defer.
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoadingDeferred));
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 10));
  WriteLoader(19, 1);

  VerifyBuffer(buffer, 10, 10);

  // Read again which should disable deferring since there should be nothing
  // left in our internal buffer.
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoading));
  ReadLoader(20, 10, buffer);

  // Over-fulfill requested bytes, then deferring should be enabled again.
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoadingDeferred));
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 10));
  WriteLoader(20, 40);

  VerifyBuffer(buffer, 20, 10);

  // Read far ahead, which should disable deferring. In this case we still have
  // bytes in our internal buffer.
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoading));
  ReadLoader(80, 10, buffer);

  // Fulfill requested bytes, then deferring should be enabled again.
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoadingDeferred));
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 10));
  WriteLoader(60, 40);

  VerifyBuffer(buffer, 80, 10);

  StopWhenLoad();
}

// Tests the data buffering logic of kCapacityDefer strategy.
TEST_F(BufferedResourceLoaderTest, ThresholdDeferStrategy) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 20);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[10];
  InSequence s;

  // Write half of capacity: keep not deferring.
  WriteData(5);

  // Write rest of space until capacity: start deferring.
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoadingDeferred));
  WriteData(5);

  // Read a byte from the buffer: stop deferring.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 1));
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoading));
  ReadLoader(10, 1, buffer);

  // Write a byte to hit capacity: start deferring.
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoadingDeferred));
  WriteData(6);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, Tricky_ReadForwardsPastBuffered) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 10);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[256];
  InSequence s;

  // PRECONDITION
  WriteUntilThreshold();
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 1));
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoading));
  ReadLoader(10, 1, buffer);
  ConfirmBufferState(1, 10, 9, 10);
  ConfirmLoaderOffsets(11, 0, 0);

  // *** TRICKY BUSINESS, PT. I ***
  // Read past buffered: stop deferring.
  //
  // In order for the read to complete we must:
  //   1) Stop deferring to receive more data.
  //
  // BEFORE
  //   offset=11 [xxxxxxxxx_]
  //                       ^ ^^^ requested 4 bytes @ offset 20
  // AFTER
  //   offset=24 [__________]
  //
  ReadLoader(20, 4, buffer);

  // Write a little, make sure we didn't start deferring.
  WriteData(2);

  // Write the rest, read should complete.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 4));
  WriteData(2);

  // POSTCONDITION
  ConfirmBufferState(4, 10, 0, 10);
  ConfirmLoaderOffsets(24, 0, 0);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, Tricky_ReadBackwardsPastBuffered) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 10);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[256];
  InSequence s;

  // PRECONDITION
  WriteUntilThreshold();
  ConfirmBufferState(0, 10, 10, 10);
  ConfirmLoaderOffsets(10, 0, 0);

  // *** TRICKY BUSINESS, PT. II ***
  // Read backwards a little too much: cache miss.
  //
  // BEFORE
  //   offset=10 [__________|xxxxxxxxxx]
  //                       ^ ^^^ requested 10 bytes @ offset 9
  // AFTER
  //   offset=10 [__________|xxxxxxxxxx]  !!! cache miss !!!
  //
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kCacheMiss, 0));
  ReadLoader(9, 4, buffer);

  // POSTCONDITION
  ConfirmBufferState(0, 10, 10, 10);
  ConfirmLoaderOffsets(10, 0, 0);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, Tricky_SmallReadWithinThreshold) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 10);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[256];
  InSequence s;

  // PRECONDITION
  WriteUntilThreshold();
  ConfirmBufferState(0, 10, 10, 10);
  ConfirmLoaderOffsets(10, 0, 0);

  // *** TRICKY BUSINESS, PT. III ***
  // Read past forward capacity but within capacity: stop deferring.
  //
  // In order for the read to complete we must:
  //   1) Adjust offset forward to create capacity.
  //   2) Stop deferring to receive more data.
  //
  // BEFORE
  //   offset=10 [xxxxxxxxxx]
  //                             ^^^^ requested 4 bytes @ offset 24
  // ADJUSTED OFFSET
  //   offset=20 [__________]
  //                  ^^^^ requested 4 bytes @ offset 24
  // AFTER
  //   offset=28 [__________]
  //
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoading));
  ReadLoader(24, 4, buffer);
  ConfirmLoaderOffsets(20, 4, 8);

  // Write a little, make sure we didn't start deferring.
  WriteData(4);

  // Write the rest, read should complete.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 4));
  WriteData(4);

  // POSTCONDITION
  ConfirmBufferState(8, 10, 0, 10);
  ConfirmLoaderOffsets(28, 0, 0);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, Tricky_LargeReadWithinThreshold) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 10);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[256];
  InSequence s;

  // PRECONDITION
  WriteUntilThreshold();
  ConfirmBufferState(0, 10, 10, 10);
  ConfirmLoaderOffsets(10, 0, 0);

  // *** TRICKY BUSINESS, PT. IV ***
  // Read a large amount past forward capacity but within
  // capacity: stop deferring.
  //
  // In order for the read to complete we must:
  //   1) Adjust offset forward to create capacity.
  //   2) Expand capacity to make sure we don't defer as data arrives.
  //   3) Stop deferring to receive more data.
  //
  // BEFORE
  //   offset=10 [xxxxxxxxxx]
  //                             ^^^^^^^^^^^^ requested 12 bytes @ offset 24
  // ADJUSTED OFFSET
  //   offset=20 [__________]
  //                  ^^^^^^ ^^^^^^ requested 12 bytes @ offset 24
  // ADJUSTED CAPACITY
  //   offset=20 [________________]
  //                  ^^^^^^^^^^^^ requested 12 bytes @ offset 24
  // AFTER
  //   offset=36 [__________]
  //
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoading));
  ReadLoader(24, 12, buffer);
  ConfirmLoaderOffsets(20, 4, 16);
  ConfirmBufferState(10, 10, 0, 16);

  // Write a little, make sure we didn't start deferring.
  WriteData(10);

  // Write the rest, read should complete and capacity should go back to normal.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 12));
  WriteData(6);
  ConfirmLoaderBufferForwardCapacity(10);

  // POSTCONDITION
  ConfirmBufferState(6, 10, 0, 10);
  ConfirmLoaderOffsets(36, 0, 0);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, Tricky_LargeReadBackwards) {
  Initialize(kHttpUrl, 10, 99);
  SetLoaderBuffer(10, 10);
  Start();
  PartialResponse(10, 99, 100);

  uint8 buffer[256];
  InSequence s;

  // PRECONDITION
  WriteUntilThreshold();
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 10));
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoading));
  ReadLoader(10, 10, buffer);
  WriteUntilThreshold();
  ConfirmBufferState(10, 10, 10, 10);
  ConfirmLoaderOffsets(20, 0, 0);

  // *** TRICKY BUSINESS, PT. V ***
  // Read a large amount that involves backwards data: stop deferring.
  //
  // In order for the read to complete we must:
  //   1) Adjust offset *backwards* to create capacity.
  //   2) Expand capacity to make sure we don't defer as data arrives.
  //   3) Stop deferring to receive more data.
  //
  // BEFORE
  //   offset=20 [xxxxxxxxxx|xxxxxxxxxx]
  //                    ^^^^ ^^^^^^^^^^ ^^^^ requested 18 bytes @ offset 16
  // ADJUSTED OFFSET
  //   offset=16 [____xxxxxx|xxxxxxxxxx]xxxx
  //                         ^^^^^^^^^^ ^^^^^^^^ requested 18 bytes @ offset 16
  // ADJUSTED CAPACITY
  //   offset=16 [____xxxxxx|xxxxxxxxxxxxxx____]
  //                         ^^^^^^^^^^^^^^^^^^ requested 18 bytes @ offset 16
  // AFTER
  //   offset=34 [xxxxxxxxxx|__________]
  //
  EXPECT_CALL(*this, LoadingCallback(BufferedResourceLoader::kLoading));
  ReadLoader(16, 18, buffer);
  ConfirmLoaderOffsets(16, 0, 18);
  ConfirmBufferState(6, 10, 14, 18);

  // Write a little, make sure we didn't start deferring.
  WriteData(2);

  // Write the rest, read should complete and capacity should go back to normal.
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kOk, 18));
  WriteData(2);
  ConfirmLoaderBufferForwardCapacity(10);

  // POSTCONDITION
  ConfirmBufferState(4, 10, 0, 10);
  ConfirmLoaderOffsets(34, 0, 0);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, Tricky_ReadPastThreshold) {
  const int kSize = 5 * 1024 * 1024;
  const int kThreshold = 2 * 1024 * 1024;

  Initialize(kHttpUrl, 10, kSize);
  SetLoaderBuffer(10, 10);
  Start();
  PartialResponse(10, kSize - 1, kSize);

  uint8 buffer[256];
  InSequence s;

  // PRECONDITION
  WriteUntilThreshold();
  ConfirmBufferState(0, 10, 10, 10);
  ConfirmLoaderOffsets(10, 0, 0);

  // *** TRICKY BUSINESS, PT. VI ***
  // Read past the forward wait threshold: cache miss.
  //
  // BEFORE
  //   offset=10 [xxxxxxxxxx] ...
  //                              ^^^^ requested 10 bytes @ threshold
  // AFTER
  //   offset=10 [xxxxxxxxxx]  !!! cache miss !!!
  //
  EXPECT_CALL(*this, ReadCallback(BufferedResourceLoader::kCacheMiss, 0));
  ReadLoader(kThreshold + 20, 10, buffer);

  // POSTCONDITION
  ConfirmBufferState(0, 10, 10, 10);
  ConfirmLoaderOffsets(10, 0, 0);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, HasSingleOrigin) {
  // Make sure no redirect case works as expected.
  Initialize(kHttpUrl, -1, -1);
  Start();
  FullResponse(1024);
  EXPECT_TRUE(loader_->HasSingleOrigin());
  StopWhenLoad();

  // Test redirect to the same domain.
  Initialize(kHttpUrl, -1, -1);
  Start();
  Redirect(kHttpRedirectToSameDomainUrl1);
  FullResponse(1024);
  EXPECT_TRUE(loader_->HasSingleOrigin());
  StopWhenLoad();

  // Test redirect twice to the same domain.
  Initialize(kHttpUrl, -1, -1);
  Start();
  Redirect(kHttpRedirectToSameDomainUrl1);
  Redirect(kHttpRedirectToSameDomainUrl2);
  FullResponse(1024);
  EXPECT_TRUE(loader_->HasSingleOrigin());
  StopWhenLoad();

  // Test redirect to a different domain.
  Initialize(kHttpUrl, -1, -1);
  Start();
  Redirect(kHttpRedirectToDifferentDomainUrl1);
  FullResponse(1024);
  EXPECT_FALSE(loader_->HasSingleOrigin());
  StopWhenLoad();

  // Test redirect to the same domain and then to a different domain.
  Initialize(kHttpUrl, -1, -1);
  Start();
  Redirect(kHttpRedirectToSameDomainUrl1);
  Redirect(kHttpRedirectToDifferentDomainUrl1);
  FullResponse(1024);
  EXPECT_FALSE(loader_->HasSingleOrigin());
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_Default) {
  Initialize(kHttpUrl, -1, -1);
  Start();

  // Test ensures that default construction of a BufferedResourceLoader has sane
  // values.
  //
  // Please do not change these values in order to make a test pass! Instead,
  // start a conversation on what the default buffer window capacities should
  // be.
  ConfirmLoaderBufferBackwardCapacity(2 * 1024 * 1024);
  ConfirmLoaderBufferForwardCapacity(2 * 1024 * 1024);

  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_Bitrate_Unknown) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetBitrate(0);
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_Bitrate_BelowLowerBound) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetBitrate(1024 * 8);  // 1 Kbps.
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_Bitrate_WithinBounds) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetBitrate(2 * 1024 * 1024 * 8);  // 2 Mbps.
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_Bitrate_AboveUpperBound) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetBitrate(100 * 1024 * 1024 * 8);  // 100 Mbps.
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_PlaybackRate_Negative) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetPlaybackRate(-10);
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_PlaybackRate_Zero) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetPlaybackRate(0);
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_PlaybackRate_BelowLowerBound) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetPlaybackRate(0.1f);
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_PlaybackRate_WithinBounds) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetPlaybackRate(10);
  CheckBufferWindowBounds();
  StopWhenLoad();
}

TEST_F(BufferedResourceLoaderTest, BufferWindow_PlaybackRate_AboveUpperBound) {
  Initialize(kHttpUrl, -1, -1);
  Start();
  loader_->SetPlaybackRate(100);
  CheckBufferWindowBounds();
  StopWhenLoad();
}

static void ExpectContentRange(
    const std::string& str, bool expect_success,
    int64 expected_first, int64 expected_last, int64 expected_size) {
  int64 first, last, size;
  ASSERT_EQ(expect_success, BufferedResourceLoader::ParseContentRange(
      str, &first, &last, &size)) << str;
  if (!expect_success)
    return;
  EXPECT_EQ(first, expected_first);
  EXPECT_EQ(last, expected_last);
  EXPECT_EQ(size, expected_size);
}

static void ExpectContentRangeFailure(const std::string& str) {
  ExpectContentRange(str, false, 0, 0, 0);
}

static void ExpectContentRangeSuccess(
    const std::string& str,
    int64 expected_first, int64 expected_last, int64 expected_size) {
  ExpectContentRange(str, true, expected_first, expected_last, expected_size);
}

TEST(BufferedResourceLoaderStandaloneTest, ParseContentRange) {
  ExpectContentRangeFailure("cytes 0-499/500");
  ExpectContentRangeFailure("bytes 0499/500");
  ExpectContentRangeFailure("bytes 0-499500");
  ExpectContentRangeFailure("bytes 0-499/500-blorg");
  ExpectContentRangeFailure("bytes 0-499/500-1");
  ExpectContentRangeFailure("bytes 0-499/400");
  ExpectContentRangeFailure("bytes 0-/400");
  ExpectContentRangeFailure("bytes -300/400");
  ExpectContentRangeFailure("bytes 20-10/400");

  ExpectContentRangeSuccess("bytes 0-499/500", 0, 499, 500);
  ExpectContentRangeSuccess("bytes 0-0/500", 0, 0, 500);
  ExpectContentRangeSuccess("bytes 10-11/50", 10, 11, 50);
  ExpectContentRangeSuccess("bytes 10-11/*", 10, 11,
                            kPositionNotSpecified);
}

}  // namespace content
