// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"

#include "base/message_loop/message_loop.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

using testing::_;
using testing::Return;
using testing::SaveArg;

namespace content {
namespace {

class MockRenderViewHostDelegate : public RenderViewHostDelegate {
 public:
  MOCK_METHOD2(RequestMediaAccessPermission,
               void(const MediaStreamRequest& request,
                    const MediaResponseCallback& callback));

  // Stubs for pure virtual methods we don't care about.
  virtual gfx::Rect GetRootWindowResizerRect() const OVERRIDE {
    NOTREACHED();
    return gfx::Rect();
  }
  virtual RendererPreferences GetRendererPrefs(
      BrowserContext* browser_context) const OVERRIDE {
    NOTREACHED();
    return RendererPreferences();
  }
};

class MockResponseCallback {
 public:
  MOCK_METHOD2(OnAccessRequestResponse,
               void(const MediaStreamDevices& devices,
               content::MediaStreamRequestResult result));
};

class MockMediaStreamUI : public MediaStreamUI {
 public:
  MOCK_METHOD1(OnStarted, gfx::NativeViewId(const base::Closure& stop));
};

class MockStopStreamHandler {
 public:
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD1(OnWindowId, void(gfx::NativeViewId window_id));
};


}  // namespace

class MediaStreamUIProxyTest : public testing::Test {
 public:
  MediaStreamUIProxyTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
    proxy_ = MediaStreamUIProxy::CreateForTests(&delegate_);
  }

  virtual ~MediaStreamUIProxyTest() {
    proxy_.reset();
    message_loop_.RunUntilIdle();
  }

 protected:
  base::MessageLoop message_loop_;
  TestBrowserThread ui_thread_;
  TestBrowserThread io_thread_;

  MockRenderViewHostDelegate delegate_;
  MockResponseCallback response_callback_;
  scoped_ptr<MediaStreamUIProxy> proxy_;
};

MATCHER_P(SameRequest, expected, "") {
  return
    expected.render_process_id == arg.render_process_id &&
    expected.render_view_id == arg.render_view_id &&
    expected.tab_capture_device_id == arg.tab_capture_device_id &&
    expected.security_origin == arg.security_origin &&
    expected.request_type == arg.request_type &&
    expected.requested_audio_device_id == arg.requested_audio_device_id &&
    expected.requested_video_device_id == arg.requested_video_device_id &&
    expected.audio_type == arg.audio_type &&
    expected.video_type == arg.video_type;
}

TEST_F(MediaStreamUIProxyTest, Deny) {
  MediaStreamRequest request(0, 0, 0, GURL("http://origin/"), false,
                             MEDIA_GENERATE_STREAM, std::string(),
                             std::string(),
                             MEDIA_DEVICE_AUDIO_CAPTURE,
                             MEDIA_DEVICE_VIDEO_CAPTURE);
  proxy_->RequestAccess(
      request, base::Bind(&MockResponseCallback::OnAccessRequestResponse,
                          base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_, RequestMediaAccessPermission(SameRequest(request), _))
    .WillOnce(SaveArg<1>(&callback));
  message_loop_.RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  MediaStreamDevices devices;
  callback.Run(devices, MEDIA_DEVICE_OK, scoped_ptr<MediaStreamUI>());

  MediaStreamDevices response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
    .WillOnce(SaveArg<0>(&response));
  message_loop_.RunUntilIdle();

  EXPECT_TRUE(response.empty());
}

TEST_F(MediaStreamUIProxyTest, AcceptAndStart) {
  MediaStreamRequest request(0, 0, 0, GURL("http://origin/"), false,
                             MEDIA_GENERATE_STREAM, std::string(),
                             std::string(),
                             MEDIA_DEVICE_AUDIO_CAPTURE,
                             MEDIA_DEVICE_VIDEO_CAPTURE);
  proxy_->RequestAccess(
      request, base::Bind(&MockResponseCallback::OnAccessRequestResponse,
                          base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_, RequestMediaAccessPermission(SameRequest(request), _))
    .WillOnce(SaveArg<1>(&callback));
  message_loop_.RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  MediaStreamDevices devices;
  devices.push_back(
      MediaStreamDevice(MEDIA_DEVICE_AUDIO_CAPTURE, "Mic", "Mic"));
  scoped_ptr<MockMediaStreamUI> ui(new MockMediaStreamUI());
  EXPECT_CALL(*ui, OnStarted(_)).WillOnce(Return(0));
  callback.Run(devices, MEDIA_DEVICE_OK, ui.PassAs<MediaStreamUI>());

  MediaStreamDevices response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
    .WillOnce(SaveArg<0>(&response));
  message_loop_.RunUntilIdle();

  EXPECT_FALSE(response.empty());

  proxy_->OnStarted(base::Closure(), MediaStreamUIProxy::WindowIdCallback());
  message_loop_.RunUntilIdle();
}

// Verify that the proxy can be deleted before the request is processed.
TEST_F(MediaStreamUIProxyTest, DeleteBeforeAccepted) {
  MediaStreamRequest request(0, 0, 0, GURL("http://origin/"), false,
                             MEDIA_GENERATE_STREAM, std::string(),
                             std::string(),
                             MEDIA_DEVICE_AUDIO_CAPTURE,
                             MEDIA_DEVICE_VIDEO_CAPTURE);
  proxy_->RequestAccess(
      request, base::Bind(&MockResponseCallback::OnAccessRequestResponse,
                          base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_, RequestMediaAccessPermission(SameRequest(request), _))
    .WillOnce(SaveArg<1>(&callback));
  message_loop_.RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  proxy_.reset();

  MediaStreamDevices devices;
  scoped_ptr<MediaStreamUI> ui;
  callback.Run(devices, MEDIA_DEVICE_OK, ui.Pass());
}

TEST_F(MediaStreamUIProxyTest, StopFromUI) {
  MediaStreamRequest request(0, 0, 0, GURL("http://origin/"), false,
                             MEDIA_GENERATE_STREAM, std::string(),
                             std::string(),
                             MEDIA_DEVICE_AUDIO_CAPTURE,
                             MEDIA_DEVICE_VIDEO_CAPTURE);
  proxy_->RequestAccess(
      request, base::Bind(&MockResponseCallback::OnAccessRequestResponse,
                          base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_, RequestMediaAccessPermission(SameRequest(request), _))
    .WillOnce(SaveArg<1>(&callback));
  message_loop_.RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  base::Closure stop_callback;

  MediaStreamDevices devices;
  devices.push_back(
      MediaStreamDevice(MEDIA_DEVICE_AUDIO_CAPTURE, "Mic", "Mic"));
  scoped_ptr<MockMediaStreamUI> ui(new MockMediaStreamUI());
  EXPECT_CALL(*ui, OnStarted(_))
      .WillOnce(testing::DoAll(SaveArg<0>(&stop_callback), Return(0)));
  callback.Run(devices, MEDIA_DEVICE_OK, ui.PassAs<MediaStreamUI>());

  MediaStreamDevices response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
    .WillOnce(SaveArg<0>(&response));
  message_loop_.RunUntilIdle();

  EXPECT_FALSE(response.empty());

  MockStopStreamHandler stop_handler;
  proxy_->OnStarted(base::Bind(&MockStopStreamHandler::OnStop,
                               base::Unretained(&stop_handler)),
                    MediaStreamUIProxy::WindowIdCallback());
  message_loop_.RunUntilIdle();

  ASSERT_FALSE(stop_callback.is_null());
  EXPECT_CALL(stop_handler, OnStop());
  stop_callback.Run();
  message_loop_.RunUntilIdle();
}

TEST_F(MediaStreamUIProxyTest, WindowIdCallbackCalled) {
  MediaStreamRequest request(0,
                             0,
                             0,
                             GURL("http://origin/"),
                             false,
                             MEDIA_GENERATE_STREAM,
                             std::string(),
                             std::string(),
                             MEDIA_NO_SERVICE,
                             MEDIA_DESKTOP_VIDEO_CAPTURE);
  proxy_->RequestAccess(
      request,
      base::Bind(&MockResponseCallback::OnAccessRequestResponse,
                 base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_, RequestMediaAccessPermission(SameRequest(request), _))
      .WillOnce(SaveArg<1>(&callback));
  message_loop_.RunUntilIdle();

  const int kWindowId = 1;
  scoped_ptr<MockMediaStreamUI> ui(new MockMediaStreamUI());
  EXPECT_CALL(*ui, OnStarted(_)).WillOnce(Return(kWindowId));

  callback.Run(
      MediaStreamDevices(), MEDIA_DEVICE_OK, ui.PassAs<MediaStreamUI>());
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _));

  MockStopStreamHandler handler;
  EXPECT_CALL(handler, OnWindowId(kWindowId));

  proxy_->OnStarted(
      base::Bind(&MockStopStreamHandler::OnStop, base::Unretained(&handler)),
      base::Bind(&MockStopStreamHandler::OnWindowId,
                 base::Unretained(&handler)));
  message_loop_.RunUntilIdle();
}

}  // content
