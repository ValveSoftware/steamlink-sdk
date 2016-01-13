// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/renderer/media/android/media_info_loader.h"
#include "content/test/mock_webframeclient.h"
#include "content/test/mock_weburlloader.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using ::testing::_;
using ::testing::InSequence;
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
static const char kHttpDataUrl[] = "data:audio/wav;base64,UklGRhwMAABXQVZFZm10";

static const int kHttpOK = 200;
static const int kHttpNotFound = 404;

class MediaInfoLoaderTest : public testing::Test {
 public:
  MediaInfoLoaderTest()
      : view_(WebView::create(NULL)), frame_(WebLocalFrame::create(&client_)) {
    view_->setMainFrame(frame_);
  }

  virtual ~MediaInfoLoaderTest() {
    view_->close();
    frame_->close();
  }

  void Initialize(
      const char* url,
      blink::WebMediaPlayer::CORSMode cors_mode) {
    gurl_ = GURL(url);

    loader_.reset(new MediaInfoLoader(
        gurl_, cors_mode,
        base::Bind(&MediaInfoLoaderTest::ReadyCallback,
                   base::Unretained(this))));

    // |test_loader_| will be used when Start() is called.
    url_loader_ = new NiceMock<MockWebURLLoader>();
    loader_->test_loader_ = scoped_ptr<blink::WebURLLoader>(url_loader_);
  }

  void Start() {
    InSequence s;
    EXPECT_CALL(*url_loader_, loadAsynchronously(_, _));
    loader_->Start(view_->mainFrame());
  }

  void Stop() {
    InSequence s;
    EXPECT_CALL(*url_loader_, cancel());
    loader_.reset();
  }

  void Redirect(const char* url) {
    GURL redirect_url(url);
    blink::WebURLRequest new_request(redirect_url);
    blink::WebURLResponse redirect_response(gurl_);

    loader_->willSendRequest(url_loader_, new_request, redirect_response);

    base::MessageLoop::current()->RunUntilIdle();
  }

  void SendResponse(
      int http_status, MediaInfoLoader::Status expected_status) {
    EXPECT_CALL(*this, ReadyCallback(expected_status, _, _, _));
    EXPECT_CALL(*url_loader_, cancel());

    WebURLResponse response(gurl_);
    response.setHTTPHeaderField(WebString::fromUTF8("Content-Length"),
                                WebString::fromUTF8("0"));
    response.setExpectedContentLength(0);
    response.setHTTPStatusCode(http_status);
    loader_->didReceiveResponse(url_loader_, response);
  }

  void FailLoad() {
    EXPECT_CALL(*this, ReadyCallback(
        MediaInfoLoader::kFailed, _, _, _));
    loader_->didFail(url_loader_, WebURLError());
  }

  MOCK_METHOD4(ReadyCallback,
               void(MediaInfoLoader::Status, const GURL&, const GURL&, bool));

 protected:
  GURL gurl_;

  scoped_ptr<MediaInfoLoader> loader_;
  NiceMock<MockWebURLLoader>* url_loader_;

  MockWebFrameClient client_;
  WebView* view_;
  WebLocalFrame* frame_;

  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaInfoLoaderTest);
};

TEST_F(MediaInfoLoaderTest, StartStop) {
  Initialize(kHttpUrl, blink::WebMediaPlayer::CORSModeUnspecified);
  Start();
  Stop();
}

TEST_F(MediaInfoLoaderTest, LoadFailure) {
  Initialize(kHttpUrl, blink::WebMediaPlayer::CORSModeUnspecified);
  Start();
  FailLoad();
}

TEST_F(MediaInfoLoaderTest, DataUri) {
  Initialize(kHttpDataUrl, blink::WebMediaPlayer::CORSModeUnspecified);
  Start();
  SendResponse(0, MediaInfoLoader::kOk);
}

TEST_F(MediaInfoLoaderTest, HasSingleOriginNoRedirect) {
  // Make sure no redirect case works as expected.
  Initialize(kHttpUrl, blink::WebMediaPlayer::CORSModeUnspecified);
  Start();
  SendResponse(kHttpOK, MediaInfoLoader::kOk);
  EXPECT_TRUE(loader_->HasSingleOrigin());
}

TEST_F(MediaInfoLoaderTest, HasSingleOriginSingleRedirect) {
  // Test redirect to the same domain.
  Initialize(kHttpUrl, blink::WebMediaPlayer::CORSModeUnspecified);
  Start();
  Redirect(kHttpRedirectToSameDomainUrl1);
  SendResponse(kHttpOK, MediaInfoLoader::kOk);
  EXPECT_TRUE(loader_->HasSingleOrigin());
}

TEST_F(MediaInfoLoaderTest, HasSingleOriginDoubleRedirect) {
  // Test redirect twice to the same domain.
  Initialize(kHttpUrl, blink::WebMediaPlayer::CORSModeUnspecified);
  Start();
  Redirect(kHttpRedirectToSameDomainUrl1);
  Redirect(kHttpRedirectToSameDomainUrl2);
  SendResponse(kHttpOK, MediaInfoLoader::kOk);
  EXPECT_TRUE(loader_->HasSingleOrigin());
}

TEST_F(MediaInfoLoaderTest, HasSingleOriginDifferentDomain) {
  // Test redirect to a different domain.
  Initialize(kHttpUrl, blink::WebMediaPlayer::CORSModeUnspecified);
  Start();
  Redirect(kHttpRedirectToDifferentDomainUrl1);
  SendResponse(kHttpOK, MediaInfoLoader::kOk);
  EXPECT_FALSE(loader_->HasSingleOrigin());
}

TEST_F(MediaInfoLoaderTest, HasSingleOriginMultipleDomains) {
  // Test redirect to the same domain and then to a different domain.
  Initialize(kHttpUrl, blink::WebMediaPlayer::CORSModeUnspecified);
  Start();
  Redirect(kHttpRedirectToSameDomainUrl1);
  Redirect(kHttpRedirectToDifferentDomainUrl1);
  SendResponse(kHttpOK, MediaInfoLoader::kOk);
  EXPECT_FALSE(loader_->HasSingleOrigin());
}

TEST_F(MediaInfoLoaderTest, CORSAccessCheckPassed) {
  Initialize(kHttpUrl, blink::WebMediaPlayer::CORSModeUseCredentials);
  Start();
  SendResponse(kHttpOK, MediaInfoLoader::kOk);
  EXPECT_TRUE(loader_->DidPassCORSAccessCheck());
}

TEST_F(MediaInfoLoaderTest, CORSAccessCheckFailed) {
  Initialize(kHttpUrl, blink::WebMediaPlayer::CORSModeUseCredentials);
  Start();
  SendResponse(kHttpNotFound, MediaInfoLoader::kFailed);
  EXPECT_FALSE(loader_->DidPassCORSAccessCheck());
}

}  // namespace content
