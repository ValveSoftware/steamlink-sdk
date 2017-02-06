// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/media/html_video_element_capturer_source.h"
#include "media/base/limits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebString.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;

namespace content {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

// An almost empty WebMediaPlayer to override paint() method.
class MockWebMediaPlayer : public blink::WebMediaPlayer,
                           public base::SupportsWeakPtr<MockWebMediaPlayer> {
 public:
  MockWebMediaPlayer()  = default;
  ~MockWebMediaPlayer() override = default;

  void load(LoadType, const blink::WebMediaPlayerSource&, CORSMode) override {}
  void play() override {}
  void pause() override {}
  bool supportsSave() const override { return true; }
  void seek(double seconds) override {}
  void setRate(double) override {}
  void setVolume(double) override {}
  blink::WebTimeRanges buffered() const override {
    return blink::WebTimeRanges();
  }
  blink::WebTimeRanges seekable() const override {
    return blink::WebTimeRanges();
  }
  void setSinkId(const blink::WebString& sinkId,
                 const blink::WebSecurityOrigin&,
                 blink::WebSetSinkIdCallbacks*) override {}
  bool hasVideo() const override { return true; }
  bool hasAudio() const override { return false; }
  blink::WebSize naturalSize() const override {
    return blink::WebSize(16, 10);
  }
  bool paused() const override { return false; }
  bool seeking() const override { return false; }
  double duration() const override { return 0.0; }
  double currentTime() const override { return 0.0; }
  NetworkState getNetworkState() const override {
    return NetworkStateEmpty;
  }
  ReadyState getReadyState() const override {
    return ReadyStateHaveNothing;
  }
  blink::WebString getErrorMessage() override { return blink::WebString(); }
  bool didLoadingProgress() override { return true; }
  bool hasSingleSecurityOrigin() const override { return true; }
  bool didPassCORSAccessCheck() const override { return true; }
  double mediaTimeForTimeValue(double timeValue) const override { return 0.0; }
  unsigned decodedFrameCount() const override { return 0; }
  unsigned droppedFrameCount() const override { return 0; }
  unsigned corruptedFrameCount() const override { return 0; }
  size_t audioDecodedByteCount() const override { return 0; }
  size_t videoDecodedByteCount() const override { return 0; }

  void paint(blink::WebCanvas* canvas,
             const blink::WebRect& paint_rectangle,
             unsigned char alpha,
             SkXfermode::Mode mode) override {
    // We could fill in |canvas| with a meaningful pattern in ARGB and verify
    // that is correctly captured (as I420) by HTMLVideoElementCapturerSource
    // but I don't think that'll be easy/useful/robust, so just let go here.
    return;
  }
};

class HTMLVideoElementCapturerSourceTest : public testing::Test {
 public:
  HTMLVideoElementCapturerSourceTest()
      : web_media_player_(new MockWebMediaPlayer()),
        html_video_capturer_(new HtmlVideoElementCapturerSource(
            web_media_player_->AsWeakPtr(),
            base::ThreadTaskRunnerHandle::Get())) {}

  // Necessary callbacks and MOCK_METHODS for them.
  MOCK_METHOD2(DoOnDeliverFrame,
               void(const scoped_refptr<media::VideoFrame>&, base::TimeTicks));
  void OnDeliverFrame(const scoped_refptr<media::VideoFrame>& video_frame,
                    base::TimeTicks estimated_capture_time) {
    DoOnDeliverFrame(video_frame, estimated_capture_time);
  }

  MOCK_METHOD1(DoOnVideoCaptureDeviceFormats,
               void(const media::VideoCaptureFormats&));
  void OnVideoCaptureDeviceFormats(const media::VideoCaptureFormats& formats) {
    DoOnVideoCaptureDeviceFormats(formats);
  }

  MOCK_METHOD1(DoOnRunning, void(bool));
  void OnRunning(bool state) { DoOnRunning(state); }

 protected:
  // We need some kind of message loop to allow |html_video_capturer_| to
  // schedule capture events.
  const base::MessageLoopForUI message_loop_;

  std::unique_ptr<MockWebMediaPlayer> web_media_player_;
  std::unique_ptr<HtmlVideoElementCapturerSource> html_video_capturer_;
};

// Constructs and destructs all objects, in particular |html_video_capturer_|
// and its inner object(s). This is a non trivial sequence.
TEST_F(HTMLVideoElementCapturerSourceTest, ConstructAndDestruct) {}

// Checks that the usual sequence of GetCurrentSupportedFormats() ->
// StartCapture() -> StopCapture() works as expected and let it capture two
// frames.
TEST_F(HTMLVideoElementCapturerSourceTest, GetFormatsAndStartAndStop) {
  InSequence s;
  media::VideoCaptureFormats formats;
  EXPECT_CALL(*this, DoOnVideoCaptureDeviceFormats(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&formats));

  html_video_capturer_->GetCurrentSupportedFormats(
      media::limits::kMaxCanvas /* max_requesteed_width */,
      media::limits::kMaxCanvas /* max_requesteed_height */,
      media::limits::kMaxFramesPerSecond /* max_requested_frame_rate */,
      base::Bind(
          &HTMLVideoElementCapturerSourceTest::OnVideoCaptureDeviceFormats,
          base::Unretained(this)));
  ASSERT_EQ(1u, formats.size());
  EXPECT_EQ(web_media_player_->naturalSize().width,
            formats[0].frame_size.width());
  EXPECT_EQ(web_media_player_->naturalSize().height,
            formats[0].frame_size.height());

  media::VideoCaptureParams params;
  params.requested_format = formats[0];

  EXPECT_CALL(*this, DoOnRunning(true)).Times(1);

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, DoOnDeliverFrame(_, _)).Times(1);
  EXPECT_CALL(*this, DoOnDeliverFrame(_, _))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));

  html_video_capturer_->StartCapture(
      params, base::Bind(&HTMLVideoElementCapturerSourceTest::OnDeliverFrame,
                         base::Unretained(this)),
      base::Bind(&HTMLVideoElementCapturerSourceTest::OnRunning,
                 base::Unretained(this)));

  run_loop.Run();

  html_video_capturer_->StopCapture();
  Mock::VerifyAndClearExpectations(this);
}

}  // namespace content
