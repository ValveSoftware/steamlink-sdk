// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_capturer_source.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "media/base/bind_to_current_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebHeap.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::WithArgs;

namespace content {

class MockVideoCapturerSource : public media::VideoCapturerSource {
 public:
  MockVideoCapturerSource() {
    ON_CALL(*this, GetCurrentSupportedFormats(_, _, _, _))
        .WillByDefault(WithArgs<3>(
            Invoke(this, &MockVideoCapturerSource::EnumerateDeviceFormats)));
  }

  MOCK_METHOD0(RequestRefreshFrame, void());
  MOCK_METHOD4(GetCurrentSupportedFormats,
              void(int max_requested_width,
                   int max_requested_height,
                   double max_requested_frame_rate,
                   const VideoCaptureDeviceFormatsCB& callback));
  MOCK_METHOD3(StartCapture,
               void(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& new_frame_callback,
                    const RunningCallback& running_callback));
  MOCK_METHOD0(StopCapture, void());

  void EnumerateDeviceFormats(const VideoCaptureDeviceFormatsCB& callback) {
    media::VideoCaptureFormat kFormatSmall(gfx::Size(640, 480), 30.0,
                                           media::PIXEL_FORMAT_I420);
    media::VideoCaptureFormat kFormatLarge(gfx::Size(1920, 1080), 30.0,
                                           media::PIXEL_FORMAT_I420);
    media::VideoCaptureFormats formats;
    formats.push_back(kFormatSmall);
    formats.push_back(kFormatLarge);
    callback.Run(formats);
  }
};

class MediaStreamVideoCapturerSourceTest : public testing::Test {
 public:
  MediaStreamVideoCapturerSourceTest()
      : child_process_(new ChildProcess()),
        source_(nullptr),
        delegate_(nullptr),
        source_stopped_(false) {}

  void TearDown() override {
    webkit_source_.reset();
    blink::WebHeap::collectAllGarbageForTesting();
  }

  void InitWithDeviceInfo(const StreamDeviceInfo& device_info) {
    std::unique_ptr<MockVideoCapturerSource> delegate(
        new MockVideoCapturerSource());
    delegate_ = delegate.get();
    source_ = new MediaStreamVideoCapturerSource(
        base::Bind(&MediaStreamVideoCapturerSourceTest::OnSourceStopped,
                   base::Unretained(this)),
        std::move(delegate));
    source_->SetDeviceInfo(device_info);

    webkit_source_.initialize(base::UTF8ToUTF16("dummy_source_id"),
                              blink::WebMediaStreamSource::TypeVideo,
                              base::UTF8ToUTF16("dummy_source_name"),
                              false /* remote */);
    webkit_source_.setExtraData(source_);
    webkit_source_id_ = webkit_source_.id();
  }

  MockConstraintFactory* constraint_factory() { return &constraint_factory_; }

  blink::WebMediaStreamTrack StartSource() {
    bool enabled = true;
    // CreateVideoTrack will trigger OnConstraintsApplied.
    return MediaStreamVideoTrack::CreateVideoTrack(
        source_, constraint_factory_.CreateWebMediaConstraints(),
        base::Bind(&MediaStreamVideoCapturerSourceTest::OnConstraintsApplied,
                   base::Unretained(this)),
        enabled);
  }

  MockVideoCapturerSource& mock_delegate() { return *delegate_; }

  const char* GetPowerLineFrequencyForTesting() const {
    return source_->GetPowerLineFrequencyForTesting();
  }

  void OnSourceStopped(const blink::WebMediaStreamSource& source) {
    source_stopped_ = true;
    EXPECT_EQ(source.id(), webkit_source_id_);
  }
  void OnStarted(bool result) { source_->OnStarted(result); }

 protected:
  void OnConstraintsApplied(MediaStreamSource* source,
                            MediaStreamRequestResult result,
                            const blink::WebString& result_name) {}

  // A ChildProcess and a MessageLoopForUI are both needed to fool the Tracks
  // and Sources below into believing they are on the right threads.
  base::MessageLoopForUI message_loop_;
  std::unique_ptr<ChildProcess> child_process_;

  blink::WebMediaStreamSource webkit_source_;
  MediaStreamVideoCapturerSource* source_;  // owned by |webkit_source_|.
  MockVideoCapturerSource* delegate_;     // owned by |source|.
  blink::WebString webkit_source_id_;
  bool source_stopped_;
  MockConstraintFactory constraint_factory_;
};

TEST_F(MediaStreamVideoCapturerSourceTest, TabCaptureFixedResolutionByDefault) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_TAB_VIDEO_CAPTURE;
  InitWithDeviceInfo(device_info);

  // No constraints are being provided to the implementation, so expect only
  // default values.
  media::VideoCaptureParams expected_params;
  expected_params.requested_format.frame_size.SetSize(
      MediaStreamVideoSource::kDefaultWidth,
      MediaStreamVideoSource::kDefaultHeight);
  expected_params.requested_format.frame_rate =
      MediaStreamVideoSource::kDefaultFrameRate;
  expected_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  expected_params.resolution_change_policy =
      media::RESOLUTION_POLICY_FIXED_RESOLUTION;

  InSequence s;
  EXPECT_CALL(mock_delegate(), GetCurrentSupportedFormats(_, _, _, _));
  EXPECT_CALL(mock_delegate(), StartCapture(expected_params, _, _));
  blink::WebMediaStreamTrack track = StartSource();
  // When the track goes out of scope, the source will be stopped.
  EXPECT_CALL(mock_delegate(), StopCapture());
}

TEST_F(MediaStreamVideoCapturerSourceTest,
       DesktopCaptureAllowAnyResolutionChangeByDefault) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_DESKTOP_VIDEO_CAPTURE;
  InitWithDeviceInfo(device_info);

  // No constraints are being provided to the implementation, so expect only
  // default values.
  media::VideoCaptureParams expected_params;
  expected_params.requested_format.frame_size.SetSize(
      MediaStreamVideoSource::kDefaultWidth,
      MediaStreamVideoSource::kDefaultHeight);
  expected_params.requested_format.frame_rate =
      MediaStreamVideoSource::kDefaultFrameRate;
  expected_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  expected_params.resolution_change_policy =
      media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT;

  InSequence s;
  EXPECT_CALL(mock_delegate(), GetCurrentSupportedFormats(_, _, _, _));
  EXPECT_CALL(mock_delegate(), StartCapture(expected_params, _, _));
  blink::WebMediaStreamTrack track = StartSource();
  // When the track goes out of scope, the source will be stopped.
  EXPECT_CALL(mock_delegate(), StopCapture());
}

TEST_F(MediaStreamVideoCapturerSourceTest,
       TabCaptureConstraintsImplyFixedAspectRatio) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_TAB_VIDEO_CAPTURE;
  InitWithDeviceInfo(device_info);

  // Specify max and min size constraints that have the same ~16:9 aspect ratio.
  constraint_factory()->basic().width.setMax(1920);
  constraint_factory()->basic().height.setMax(1080);
  constraint_factory()->basic().width.setMin(854);
  constraint_factory()->basic().height.setMin(480);
  constraint_factory()->basic().frameRate.setMax(60.0);

  media::VideoCaptureParams expected_params;
  expected_params.requested_format.frame_size.SetSize(1920, 1080);
  expected_params.requested_format.frame_rate = 60.0;
  expected_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  expected_params.resolution_change_policy =
      media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO;

  InSequence s;
  EXPECT_CALL(mock_delegate(), GetCurrentSupportedFormats(_, _, _, _));
  EXPECT_CALL(
      mock_delegate(),
      StartCapture(
          testing::Field(&media::VideoCaptureParams::resolution_change_policy,
                         media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO),
          _, _));
  blink::WebMediaStreamTrack track = StartSource();
  // When the track goes out of scope, the source will be stopped.
  EXPECT_CALL(mock_delegate(), StopCapture());
}

TEST_F(MediaStreamVideoCapturerSourceTest,
       TabCaptureConstraintsImplyAllowingAnyResolutionChange) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_TAB_VIDEO_CAPTURE;
  InitWithDeviceInfo(device_info);

  // Specify max and min size constraints with different aspect ratios.
  constraint_factory()->basic().width.setMax(1920);
  constraint_factory()->basic().height.setMax(1080);
  constraint_factory()->basic().width.setMin(0);
  constraint_factory()->basic().height.setMin(0);
  constraint_factory()->basic().frameRate.setMax(60.0);

  media::VideoCaptureParams expected_params;
  expected_params.requested_format.frame_size.SetSize(1920, 1080);
  expected_params.requested_format.frame_rate = 60.0;
  expected_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  expected_params.resolution_change_policy =
      media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT;

  InSequence s;
  EXPECT_CALL(mock_delegate(), GetCurrentSupportedFormats(_, _, _, _));
  EXPECT_CALL(
      mock_delegate(),
      StartCapture(
          testing::Field(&media::VideoCaptureParams::resolution_change_policy,
                         media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT),
          _, _));
  blink::WebMediaStreamTrack track = StartSource();
  // When the track goes out of scope, the source will be stopped.
  EXPECT_CALL(mock_delegate(), StopCapture());
}

TEST_F(MediaStreamVideoCapturerSourceTest,
       DeviceCaptureConstraintsSupportPowerLineFrequency) {
  for (int frequency = -100; frequency < 100; ++frequency) {
    StreamDeviceInfo device_info;
    device_info.device.type = MEDIA_DEVICE_VIDEO_CAPTURE;
    InitWithDeviceInfo(device_info);
    constraint_factory_.Reset();

    constraint_factory()->AddAdvanced().googPowerLineFrequency.setExact(
        frequency);

    media::VideoCaptureParams expected_params;
    expected_params.requested_format.frame_size.SetSize(
        MediaStreamVideoSource::kDefaultWidth,
        MediaStreamVideoSource::kDefaultHeight);
    expected_params.requested_format.frame_rate =
        MediaStreamVideoSource::kDefaultFrameRate;
    expected_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
    expected_params.resolution_change_policy =
        media::RESOLUTION_POLICY_FIXED_RESOLUTION;
    if (frequency == 50) {
      expected_params.power_line_frequency =
          media::PowerLineFrequency::FREQUENCY_50HZ;
    } else if (frequency == 60) {
      expected_params.power_line_frequency =
          media::PowerLineFrequency::FREQUENCY_60HZ;
    } else {
      expected_params.power_line_frequency =
          media::PowerLineFrequency::FREQUENCY_DEFAULT;
    }

    InSequence s;
    EXPECT_CALL(mock_delegate(), GetCurrentSupportedFormats(_, _, _, _));
    EXPECT_CALL(mock_delegate(), StartCapture(expected_params, _, _));
    blink::WebMediaStreamTrack track = StartSource();
    // When the track goes out of scope, the source will be stopped.
    EXPECT_CALL(mock_delegate(), StopCapture());
  }
}

TEST_F(MediaStreamVideoCapturerSourceTest, Ended) {
  std::unique_ptr<MockVideoCapturerSource> delegate(
      new MockVideoCapturerSource());
  delegate_ = delegate.get();
  source_ = new MediaStreamVideoCapturerSource(
      base::Bind(&MediaStreamVideoCapturerSourceTest::OnSourceStopped,
                 base::Unretained(this)),
      std::move(delegate));
  webkit_source_.initialize(base::UTF8ToUTF16("dummy_source_id"),
                            blink::WebMediaStreamSource::TypeVideo,
                            base::UTF8ToUTF16("dummy_source_name"),
                            false /* remote */);
  webkit_source_.setExtraData(source_);
  webkit_source_id_ = webkit_source_.id();

  InSequence s;
  EXPECT_CALL(mock_delegate(), GetCurrentSupportedFormats(_, _, _, _));
  EXPECT_CALL(mock_delegate(), StartCapture(_, _, _));
  blink::WebMediaStreamTrack track = StartSource();
  message_loop_.RunUntilIdle();

  OnStarted(true);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive,
            webkit_source_.getReadyState());

  EXPECT_FALSE(source_stopped_);

  EXPECT_CALL(mock_delegate(), StopCapture());
  OnStarted(false);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded,
            webkit_source_.getReadyState());
  // Verify that MediaStreamSource::SourceStoppedCallback has been triggered.
  EXPECT_TRUE(source_stopped_);
}

class FakeMediaStreamVideoSink : public MediaStreamVideoSink {
 public:
  FakeMediaStreamVideoSink(base::TimeTicks* capture_time,
                           media::VideoFrameMetadata* metadata,
                           base::Closure got_frame_cb)
      : capture_time_(capture_time),
        metadata_(metadata),
        got_frame_cb_(got_frame_cb) {}

  void ConnectToTrack(const blink::WebMediaStreamTrack& track) {
    MediaStreamVideoSink::ConnectToTrack(
        track, base::Bind(&FakeMediaStreamVideoSink::OnVideoFrame,
                          base::Unretained(this)),
        true);
  }

  void DisconnectFromTrack() {
    MediaStreamVideoSink::DisconnectFromTrack();
  }

  void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame,
                    base::TimeTicks capture_time) {
    *capture_time_ = capture_time;
    metadata_->Clear();
    base::DictionaryValue tmp;
    frame->metadata()->MergeInternalValuesInto(&tmp);
    metadata_->MergeInternalValuesFrom(tmp);
    base::ResetAndReturn(&got_frame_cb_).Run();
  }

 private:
  base::TimeTicks* const capture_time_;
  media::VideoFrameMetadata* const metadata_;
  base::Closure got_frame_cb_;
};

TEST_F(MediaStreamVideoCapturerSourceTest, CaptureTimeAndMetadataPlumbing) {
  StreamDeviceInfo device_info;
  device_info.device.type = MEDIA_DESKTOP_VIDEO_CAPTURE;
  InitWithDeviceInfo(device_info);

  VideoCaptureDeliverFrameCB deliver_frame_cb;
  media::VideoCapturerSource::RunningCallback running_cb;

  InSequence s;
  EXPECT_CALL(mock_delegate(), GetCurrentSupportedFormats(_, _, _, _));
  EXPECT_CALL(mock_delegate(), StartCapture(_, _, _))
      .WillOnce(testing::DoAll(testing::SaveArg<1>(&deliver_frame_cb),
                               testing::SaveArg<2>(&running_cb)));
  EXPECT_CALL(mock_delegate(), RequestRefreshFrame());
  EXPECT_CALL(mock_delegate(), StopCapture());
  blink::WebMediaStreamTrack track = StartSource();
  running_cb.Run(true);

  base::RunLoop run_loop;
  base::TimeTicks reference_capture_time =
      base::TimeTicks::FromInternalValue(60013);
  base::TimeTicks capture_time;
  media::VideoFrameMetadata metadata;
  FakeMediaStreamVideoSink fake_sink(
      &capture_time, &metadata,
      media::BindToCurrentLoop(run_loop.QuitClosure()));
  fake_sink.ConnectToTrack(track);
  const scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(2, 2));
  frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE, 30.0);
  child_process_->io_task_runner()->PostTask(
      FROM_HERE, base::Bind(deliver_frame_cb, frame, reference_capture_time));
  run_loop.Run();
  fake_sink.DisconnectFromTrack();
  EXPECT_EQ(reference_capture_time, capture_time);
  double metadata_value;
  EXPECT_TRUE(metadata.GetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                 &metadata_value));
  EXPECT_EQ(30.0, metadata_value);
}

}  // namespace content
