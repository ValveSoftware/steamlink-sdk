// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/resource_multibuffer_data_provider.h"

#include <stdint.h>
#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "media/base/media_log.h"
#include "media/base/seekable_buffer.h"
#include "media/blink/mock_webframeclient.h"
#include "media/blink/mock_weburlloader.h"
#include "media/blink/url_index.h"
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

namespace media {

const char kHttpUrl[] = "http://test";
const char kHttpRedirect[] = "http://test/ing";

const int kDataSize = 1024;
const int kHttpOK = 200;
const int kHttpPartialContent = 206;

enum NetworkState { NONE, LOADED, LOADING };

// Predicate that tests that request disallows compressed data.
static bool CorrectAcceptEncoding(const blink::WebURLRequest& request) {
  std::string value =
      request.httpHeaderField(
                 WebString::fromUTF8(net::HttpRequestHeaders::kAcceptEncoding))
          .utf8();
  return (value.find("identity;q=1") != std::string::npos) &&
         (value.find("*;q=0") != std::string::npos);
}

class ResourceMultiBufferDataProviderTest : public testing::Test {
 public:
  ResourceMultiBufferDataProviderTest()
      : view_(WebView::create(nullptr, blink::WebPageVisibilityStateVisible)),
        frame_(WebLocalFrame::create(blink::WebTreeScopeType::Document,
                                     &client_)) {
    view_->setMainFrame(frame_);
    url_index_.reset(new UrlIndex(frame_, 0));

    for (int i = 0; i < kDataSize; ++i) {
      data_[i] = i;
    }
  }

  virtual ~ResourceMultiBufferDataProviderTest() {
    view_->close();
    frame_->close();
  }

  void Initialize(const char* url, int first_position) {
    gurl_ = GURL(url);
    url_data_ = url_index_->GetByUrl(gurl_, UrlData::CORS_UNSPECIFIED);
    DCHECK(url_data_);
    DCHECK(url_data_->frame());
    url_data_->OnRedirect(
        base::Bind(&ResourceMultiBufferDataProviderTest::RedirectCallback,
                   base::Unretained(this)));

    first_position_ = first_position;

    std::unique_ptr<ResourceMultiBufferDataProvider> loader(
        new ResourceMultiBufferDataProvider(url_data_.get(), first_position_));
    loader_ = loader.get();
    url_data_->multibuffer()->AddProvider(std::move(loader));

    // |test_loader_| will be used when Start() is called.
    url_loader_ = new NiceMock<MockWebURLLoader>();
    loader_->test_loader_ = std::unique_ptr<blink::WebURLLoader>(url_loader_);
  }

  void Start() {
    InSequence s;
    EXPECT_CALL(*url_loader_,
                loadAsynchronously(Truly(CorrectAcceptEncoding), loader_));

    loader_->Start();
  }

  void FullResponse(int64_t instance_size, bool ok = true) {
    WebURLResponse response(gurl_);
    response.setHTTPHeaderField(
        WebString::fromUTF8("Content-Length"),
        WebString::fromUTF8(base::StringPrintf("%" PRId64, instance_size)));
    response.setExpectedContentLength(instance_size);
    response.setHTTPStatusCode(kHttpOK);
    loader_->didReceiveResponse(url_loader_, response);

    if (ok) {
      EXPECT_EQ(instance_size, url_data_->length());
    }

    EXPECT_FALSE(url_data_->range_supported());
  }

  void PartialResponse(int64_t first_position,
                       int64_t last_position,
                       int64_t instance_size) {
    PartialResponse(first_position, last_position, instance_size, false, true);
  }

  void PartialResponse(int64_t first_position,
                       int64_t last_position,
                       int64_t instance_size,
                       bool chunked,
                       bool accept_ranges) {
    WebURLResponse response(gurl_);
    response.setHTTPHeaderField(
        WebString::fromUTF8("Content-Range"),
        WebString::fromUTF8(
            base::StringPrintf("bytes "
                               "%" PRId64 "-%" PRId64 "/%" PRId64,
                               first_position, last_position, instance_size)));

    // HTTP 1.1 doesn't permit Content-Length with Transfer-Encoding: chunked.
    int64_t content_length = -1;
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

    EXPECT_EQ(instance_size, url_data_->length());

    // A valid partial response should always result in this being true.
    EXPECT_TRUE(url_data_->range_supported());
  }

  void Redirect(const char* url) {
    GURL redirectUrl(url);
    blink::WebURLRequest newRequest(redirectUrl);
    blink::WebURLResponse redirectResponse(gurl_);

    EXPECT_CALL(*this, RedirectCallback(_))
        .WillOnce(
            Invoke(this, &ResourceMultiBufferDataProviderTest::SetUrlData));

    loader_->willFollowRedirect(url_loader_, newRequest, redirectResponse);

    base::RunLoop().RunUntilIdle();
  }

  void StopWhenLoad() {
    InSequence s;
    EXPECT_CALL(*url_loader_, cancel());
    loader_ = nullptr;
    url_data_ = nullptr;
  }

  // Helper method to write to |loader_| from |data_|.
  void WriteLoader(int position, int size) {
    loader_->didReceiveData(
        url_loader_, reinterpret_cast<char*>(data_ + position), size, size);
  }

  void WriteData(int size) {
    std::unique_ptr<char[]> data(new char[size]);
    loader_->didReceiveData(url_loader_, data.get(), size, size);
  }

  // Verifies that data in buffer[0...size] is equal to data_[pos...pos+size].
  void VerifyBuffer(uint8_t* buffer, int pos, int size) {
    EXPECT_EQ(0, memcmp(buffer, data_ + pos, size));
  }

  bool HasActiveLoader() { return loader_->active_loader_ != nullptr; }
  MOCK_METHOD1(RedirectCallback, void(const scoped_refptr<UrlData>&));

  void SetUrlData(const scoped_refptr<UrlData>& new_url_data) {
    url_data_ = new_url_data;
  }

 protected:
  GURL gurl_;
  int64_t first_position_;

  std::unique_ptr<UrlIndex> url_index_;
  scoped_refptr<UrlData> url_data_;
  scoped_refptr<UrlData> redirected_to_;
  // The loader is owned by the UrlData above.
  ResourceMultiBufferDataProvider* loader_;
  NiceMock<MockWebURLLoader>* url_loader_;

  MockWebFrameClient client_;
  WebView* view_;
  WebLocalFrame* frame_;

  base::MessageLoop message_loop_;

  uint8_t data_[kDataSize];

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceMultiBufferDataProviderTest);
};

TEST_F(ResourceMultiBufferDataProviderTest, StartStop) {
  Initialize(kHttpUrl, 0);
  Start();
  StopWhenLoad();
}

// Tests that a bad HTTP response is recived, e.g. file not found.
TEST_F(ResourceMultiBufferDataProviderTest, BadHttpResponse) {
  Initialize(kHttpUrl, 0);
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));

  WebURLResponse response(gurl_);
  response.setHTTPStatusCode(404);
  response.setHTTPStatusText("Not Found\n");
  loader_->didReceiveResponse(url_loader_, response);
}

// Tests that partial content is requested but not fulfilled.
TEST_F(ResourceMultiBufferDataProviderTest, NotPartialResponse) {
  Initialize(kHttpUrl, 100);
  Start();
  FullResponse(1024, false);
}

// Tests that a 200 response is received.
TEST_F(ResourceMultiBufferDataProviderTest, FullResponse) {
  Initialize(kHttpUrl, 0);
  Start();
  FullResponse(1024);
  StopWhenLoad();
}

// Tests that a partial content response is received.
TEST_F(ResourceMultiBufferDataProviderTest, PartialResponse) {
  Initialize(kHttpUrl, 100);
  Start();
  PartialResponse(100, 200, 1024);
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest, PartialResponse_Chunked) {
  Initialize(kHttpUrl, 100);
  Start();
  PartialResponse(100, 200, 1024, true, true);
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest, PartialResponse_NoAcceptRanges) {
  Initialize(kHttpUrl, 100);
  Start();
  PartialResponse(100, 200, 1024, false, false);
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       PartialResponse_ChunkedNoAcceptRanges) {
  Initialize(kHttpUrl, 100);
  Start();
  PartialResponse(100, 200, 1024, true, false);
  StopWhenLoad();
}

// Tests that an invalid partial response is received.
TEST_F(ResourceMultiBufferDataProviderTest, InvalidPartialResponse) {
  Initialize(kHttpUrl, 0);
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));

  WebURLResponse response(gurl_);
  response.setHTTPHeaderField(
      WebString::fromUTF8("Content-Range"),
      WebString::fromUTF8(base::StringPrintf("bytes "
                                             "%d-%d/%d",
                                             1, 10, 1024)));
  response.setExpectedContentLength(10);
  response.setHTTPStatusCode(kHttpPartialContent);
  loader_->didReceiveResponse(url_loader_, response);
}

TEST_F(ResourceMultiBufferDataProviderTest, TestRedirects) {
  // Test redirect.
  Initialize(kHttpUrl, 0);
  Start();
  Redirect(kHttpRedirect);
  FullResponse(1024);
  StopWhenLoad();
}

}  // namespace media
