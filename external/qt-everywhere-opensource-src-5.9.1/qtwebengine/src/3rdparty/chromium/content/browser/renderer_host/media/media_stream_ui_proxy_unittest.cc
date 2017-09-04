// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"

#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::_;
using testing::Return;
using testing::SaveArg;

namespace content {

namespace {
class MockRenderFrameHostDelegate : public RenderFrameHostDelegate {
 public:
  MOCK_METHOD2(RequestMediaAccessPermission,
               void(const MediaStreamRequest& request,
                    const MediaResponseCallback& callback));
  MOCK_METHOD2(CheckMediaAccessPermission,
               bool(const GURL& security_origin,
                    MediaStreamType type));
};

class MockResponseCallback {
 public:
  MOCK_METHOD2(OnAccessRequestResponse,
               void(const MediaStreamDevices& devices,
               content::MediaStreamRequestResult result));
  MOCK_METHOD1(OnCheckResponse, void(bool have_access));
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

  ~MediaStreamUIProxyTest() override {
    proxy_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::MessageLoop message_loop_;
  TestBrowserThread ui_thread_;
  TestBrowserThread io_thread_;

  MockRenderFrameHostDelegate delegate_;
  MockResponseCallback response_callback_;
  std::unique_ptr<MediaStreamUIProxy> proxy_;
};

MATCHER_P(SameRequest, expected, "") {
  return
    expected->render_process_id == arg.render_process_id &&
    expected->render_frame_id == arg.render_frame_id &&
    expected->security_origin == arg.security_origin &&
    expected->request_type == arg.request_type &&
    expected->requested_audio_device_id == arg.requested_audio_device_id &&
    expected->requested_video_device_id == arg.requested_video_device_id &&
    expected->audio_type == arg.audio_type &&
    expected->video_type == arg.video_type;
}

TEST_F(MediaStreamUIProxyTest, Deny) {
  std::unique_ptr<MediaStreamRequest> request(new MediaStreamRequest(
      0, 0, 0, GURL("http://origin/"), false, MEDIA_GENERATE_STREAM,
      std::string(), std::string(), MEDIA_DEVICE_AUDIO_CAPTURE,
      MEDIA_DEVICE_VIDEO_CAPTURE, false));
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::Bind(&MockResponseCallback::OnAccessRequestResponse,
                 base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_, RequestMediaAccessPermission(SameRequest(request_ptr),
                                                      _))
    .WillOnce(SaveArg<1>(&callback));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  MediaStreamDevices devices;
  callback.Run(devices, MEDIA_DEVICE_OK, std::unique_ptr<MediaStreamUI>());

  MediaStreamDevices response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
    .WillOnce(SaveArg<0>(&response));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(response.empty());
}

TEST_F(MediaStreamUIProxyTest, AcceptAndStart) {
  std::unique_ptr<MediaStreamRequest> request(new MediaStreamRequest(
      0, 0, 0, GURL("http://origin/"), false, MEDIA_GENERATE_STREAM,
      std::string(), std::string(), MEDIA_DEVICE_AUDIO_CAPTURE,
      MEDIA_DEVICE_VIDEO_CAPTURE, false));
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::Bind(&MockResponseCallback::OnAccessRequestResponse,
                 base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_, RequestMediaAccessPermission(SameRequest(request_ptr),
                                                      _))
    .WillOnce(SaveArg<1>(&callback));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  MediaStreamDevices devices;
  devices.push_back(
      MediaStreamDevice(MEDIA_DEVICE_AUDIO_CAPTURE, "Mic", "Mic"));
  std::unique_ptr<MockMediaStreamUI> ui(new MockMediaStreamUI());
  EXPECT_CALL(*ui, OnStarted(_)).WillOnce(Return(0));
  callback.Run(devices, MEDIA_DEVICE_OK, std::move(ui));

  MediaStreamDevices response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
    .WillOnce(SaveArg<0>(&response));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(response.empty());

  proxy_->OnStarted(base::Closure(), MediaStreamUIProxy::WindowIdCallback());
  base::RunLoop().RunUntilIdle();
}

// Verify that the proxy can be deleted before the request is processed.
TEST_F(MediaStreamUIProxyTest, DeleteBeforeAccepted) {
  std::unique_ptr<MediaStreamRequest> request(new MediaStreamRequest(
      0, 0, 0, GURL("http://origin/"), false, MEDIA_GENERATE_STREAM,
      std::string(), std::string(), MEDIA_DEVICE_AUDIO_CAPTURE,
      MEDIA_DEVICE_VIDEO_CAPTURE, false));
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::Bind(&MockResponseCallback::OnAccessRequestResponse,
                 base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_, RequestMediaAccessPermission(SameRequest(request_ptr)
                                                      , _))
    .WillOnce(SaveArg<1>(&callback));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  proxy_.reset();

  MediaStreamDevices devices;
  std::unique_ptr<MediaStreamUI> ui;
  callback.Run(devices, MEDIA_DEVICE_OK, std::move(ui));
}

TEST_F(MediaStreamUIProxyTest, StopFromUI) {
  std::unique_ptr<MediaStreamRequest> request(new MediaStreamRequest(
      0, 0, 0, GURL("http://origin/"), false, MEDIA_GENERATE_STREAM,
      std::string(), std::string(), MEDIA_DEVICE_AUDIO_CAPTURE,
      MEDIA_DEVICE_VIDEO_CAPTURE, false));
  MediaStreamRequest* request_ptr = request.get();
  proxy_->RequestAccess(
      std::move(request),
      base::Bind(&MockResponseCallback::OnAccessRequestResponse,
                 base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_, RequestMediaAccessPermission(SameRequest(request_ptr)
                                                      , _))
    .WillOnce(SaveArg<1>(&callback));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.is_null());

  base::Closure stop_callback;

  MediaStreamDevices devices;
  devices.push_back(
      MediaStreamDevice(MEDIA_DEVICE_AUDIO_CAPTURE, "Mic", "Mic"));
  std::unique_ptr<MockMediaStreamUI> ui(new MockMediaStreamUI());
  EXPECT_CALL(*ui, OnStarted(_))
      .WillOnce(testing::DoAll(SaveArg<0>(&stop_callback), Return(0)));
  callback.Run(devices, MEDIA_DEVICE_OK, std::move(ui));

  MediaStreamDevices response;
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _))
    .WillOnce(SaveArg<0>(&response));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(response.empty());

  MockStopStreamHandler stop_handler;
  proxy_->OnStarted(base::Bind(&MockStopStreamHandler::OnStop,
                               base::Unretained(&stop_handler)),
                    MediaStreamUIProxy::WindowIdCallback());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(stop_callback.is_null());
  EXPECT_CALL(stop_handler, OnStop());
  stop_callback.Run();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamUIProxyTest, WindowIdCallbackCalled) {
  std::unique_ptr<MediaStreamRequest> request(new MediaStreamRequest(
      0, 0, 0, GURL("http://origin/"), false, MEDIA_GENERATE_STREAM,
      std::string(), std::string(), MEDIA_NO_SERVICE,
      MEDIA_DESKTOP_VIDEO_CAPTURE, false));
  MediaStreamRequest* request_ptr = request.get();

  proxy_->RequestAccess(
      std::move(request),
      base::Bind(&MockResponseCallback::OnAccessRequestResponse,
                 base::Unretained(&response_callback_)));
  MediaResponseCallback callback;
  EXPECT_CALL(delegate_, RequestMediaAccessPermission(SameRequest(request_ptr),
                                                      _))
      .WillOnce(SaveArg<1>(&callback));
  base::RunLoop().RunUntilIdle();

  const int kWindowId = 1;
  std::unique_ptr<MockMediaStreamUI> ui(new MockMediaStreamUI());
  EXPECT_CALL(*ui, OnStarted(_)).WillOnce(Return(kWindowId));

  callback.Run(MediaStreamDevices(), MEDIA_DEVICE_OK, std::move(ui));
  EXPECT_CALL(response_callback_, OnAccessRequestResponse(_, _));

  MockStopStreamHandler handler;
  EXPECT_CALL(handler, OnWindowId(kWindowId));

  proxy_->OnStarted(
      base::Bind(&MockStopStreamHandler::OnStop, base::Unretained(&handler)),
      base::Bind(&MockStopStreamHandler::OnWindowId,
                 base::Unretained(&handler)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamUIProxyTest, CheckAccess) {
  proxy_->CheckAccess(url::Origin(GURL("http://origin/")),
                      MEDIA_DEVICE_AUDIO_CAPTURE, 0, 0,
                      base::Bind(&MockResponseCallback::OnCheckResponse,
                                 base::Unretained(&response_callback_)));
  EXPECT_CALL(delegate_, CheckMediaAccessPermission(_, _));
  EXPECT_CALL(response_callback_, OnCheckResponse(_));
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
