// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_constraint_factory.h"
#include "media/base/bind_to_current_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MockVideoCapturerDelegate : public VideoCapturerDelegate {
 public:
  explicit MockVideoCapturerDelegate(const StreamDeviceInfo& device_info)
      : VideoCapturerDelegate(device_info) {}

  MOCK_METHOD3(StartCapture,
               void(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& new_frame_callback,
                    const RunningCallback& running_callback));
  MOCK_METHOD0(StopCapture, void());

 private:
  virtual ~MockVideoCapturerDelegate() {}
};

class MediaStreamVideoCapturerSourceTest : public testing::Test {
 public:
  MediaStreamVideoCapturerSourceTest()
     : child_process_(new ChildProcess()),
       source_(NULL) {
  }

  void InitWithDeviceInfo(const StreamDeviceInfo& device_info) {
    delegate_ = new MockVideoCapturerDelegate(device_info);
    source_ = new MediaStreamVideoCapturerSource(
        device_info,
        MediaStreamSource::SourceStoppedCallback(),
        delegate_);

    webkit_source_.initialize(base::UTF8ToUTF16("dummy_source_id"),
                              blink::WebMediaStreamSource::TypeVideo,
                              base::UTF8ToUTF16("dummy_source_name"));
    webkit_source_.setExtraData(source_);
  }

  blink::WebMediaStreamTrack StartSource() {
    MockMediaConstraintFactory factory;
    bool enabled = true;
    // CreateVideoTrack will trigger OnConstraintsApplied.
    return MediaStreamVideoTrack::CreateVideoTrack(
        source_, factory.CreateWebMediaConstraints(),
        base::Bind(
            &MediaStreamVideoCapturerSourceTest::OnConstraintsApplied,
            base::Unretained(this)),
            enabled);
  }

  MockVideoCapturerDelegate& mock_delegate() {
    return *static_cast<MockVideoCapturerDelegate*>(delegate_.get());
  }

 protected:
  void OnConstraintsApplied(MediaStreamSource* source, bool success) {
  }

  base::MessageLoopForUI message_loop_;
  scoped_ptr<ChildProcess> child_process_;
  blink::WebMediaStreamSource webkit_source_;
  MediaStreamVideoCapturerSource* source_;  // owned by webkit_source.
  scoped_refptr<VideoCapturerDelegate> delegate_;
};

TEST_F(MediaStreamVideoCapturerSourceTest, TabCaptureAllowResolutionChange) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_TAB_VIDEO_CAPTURE;
  InitWithDeviceInfo(device_info);

  EXPECT_CALL(mock_delegate(), StartCapture(
      testing::Field(&media::VideoCaptureParams::allow_resolution_change, true),
      testing::_,
      testing::_)).Times(1);
  blink::WebMediaStreamTrack track = StartSource();
  // When the track goes out of scope, the source will be stopped.
  EXPECT_CALL(mock_delegate(), StopCapture());
}

TEST_F(MediaStreamVideoCapturerSourceTest,
       DesktopCaptureAllowResolutionChange) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_DESKTOP_VIDEO_CAPTURE;
  InitWithDeviceInfo(device_info);

  EXPECT_CALL(mock_delegate(), StartCapture(
      testing::Field(&media::VideoCaptureParams::allow_resolution_change, true),
      testing::_,
      testing::_)).Times(1);
  blink::WebMediaStreamTrack track = StartSource();
  // When the track goes out of scope, the source will be stopped.
  EXPECT_CALL(mock_delegate(), StopCapture());
}

TEST_F(MediaStreamVideoCapturerSourceTest, Ended) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_DESKTOP_VIDEO_CAPTURE;
  delegate_ = new VideoCapturerDelegate(device_info);
  source_ = new MediaStreamVideoCapturerSource(
      device_info,
      MediaStreamSource::SourceStoppedCallback(),
      delegate_);
  webkit_source_.initialize(base::UTF8ToUTF16("dummy_source_id"),
                            blink::WebMediaStreamSource::TypeVideo,
                            base::UTF8ToUTF16("dummy_source_name"));
  webkit_source_.setExtraData(source_);
  blink::WebMediaStreamTrack track = StartSource();
  message_loop_.RunUntilIdle();

  delegate_->OnStateUpdateOnRenderThread(VIDEO_CAPTURE_STATE_STARTED);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive,
            webkit_source_.readyState());

  delegate_->OnStateUpdateOnRenderThread(VIDEO_CAPTURE_STATE_ERROR);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded,
            webkit_source_.readyState());
}

class FakeMediaStreamVideoSink : public MediaStreamVideoSink {
 public:
  FakeMediaStreamVideoSink(base::TimeTicks* capture_time,
                           base::Closure got_frame_cb)
      : capture_time_(capture_time),
        got_frame_cb_(got_frame_cb) {
  }

  void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame,
                    const media::VideoCaptureFormat& format,
                    const base::TimeTicks& capture_time) {
    *capture_time_ = capture_time;
    base::ResetAndReturn(&got_frame_cb_).Run();
  }

 private:
  base::TimeTicks* capture_time_;
  base::Closure got_frame_cb_;
};

TEST_F(MediaStreamVideoCapturerSourceTest, CaptureTime) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_DESKTOP_VIDEO_CAPTURE;
  InitWithDeviceInfo(device_info);

  VideoCaptureDeliverFrameCB deliver_frame_cb;
  VideoCapturerDelegate::RunningCallback running_cb;

  EXPECT_CALL(mock_delegate(), StartCapture(
      testing::_,
      testing::_,
      testing::_))
      .Times(1)
      .WillOnce(testing::DoAll(testing::SaveArg<1>(&deliver_frame_cb),
                               testing::SaveArg<2>(&running_cb)));
  EXPECT_CALL(mock_delegate(), StopCapture());
  blink::WebMediaStreamTrack track = StartSource();
  running_cb.Run(true);

  base::RunLoop run_loop;
  base::TimeTicks reference_capture_time =
      base::TimeTicks::FromInternalValue(60013);
  base::TimeTicks capture_time;
  FakeMediaStreamVideoSink fake_sink(
      &capture_time,
      media::BindToCurrentLoop(run_loop.QuitClosure()));
  FakeMediaStreamVideoSink::AddToVideoTrack(
      &fake_sink,
      base::Bind(&FakeMediaStreamVideoSink::OnVideoFrame,
                 base::Unretained(&fake_sink)),
      track);
  child_process_->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(deliver_frame_cb,
                 media::VideoFrame::CreateBlackFrame(gfx::Size(2, 2)),
                 media::VideoCaptureFormat(),
                 reference_capture_time));
  run_loop.Run();
  FakeMediaStreamVideoSink::RemoveFromVideoTrack(&fake_sink, track);
  EXPECT_EQ(reference_capture_time, capture_time);
}

}  // namespace content
