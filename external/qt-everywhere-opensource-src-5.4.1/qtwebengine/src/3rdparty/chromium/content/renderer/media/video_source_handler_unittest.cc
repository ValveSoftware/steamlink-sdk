// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/common/media/video_capture.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_registry_interface.h"
#include "content/renderer/media/mock_media_stream_registry.h"
#include "content/renderer/media/video_source_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

static const std::string kTestStreamUrl = "stream_url";
static const std::string kTestVideoTrackId = "video_track_id";
static const std::string kUnknownStreamUrl = "unknown_stream_url";

class FakeFrameReader : public FrameReaderInterface {
 public:
  virtual bool GotFrame(
      const scoped_refptr<media::VideoFrame>& frame) OVERRIDE {
    last_frame_ = frame;
    return true;
  }

  const media::VideoFrame* last_frame() {
    return last_frame_.get();
  }

 private:
  scoped_refptr<media::VideoFrame> last_frame_;
};

class VideoSourceHandlerTest : public ::testing::Test {
 public:
  VideoSourceHandlerTest()
       : child_process_(new ChildProcess()),
         registry_() {
    handler_.reset(new VideoSourceHandler(&registry_));
    registry_.Init(kTestStreamUrl);
    registry_.AddVideoTrack(kTestVideoTrackId);
  }

 protected:
  base::MessageLoop message_loop_;
  scoped_ptr<ChildProcess> child_process_;
  scoped_ptr<VideoSourceHandler> handler_;
  MockMediaStreamRegistry registry_;
};

TEST_F(VideoSourceHandlerTest, OpenClose) {
  FakeFrameReader reader;
  // Unknow url will return false.
  EXPECT_FALSE(handler_->Open(kUnknownStreamUrl, &reader));
  EXPECT_TRUE(handler_->Open(kTestStreamUrl, &reader));

  int width = 640;
  int height = 360;
  base::TimeDelta ts = base::TimeDelta::FromInternalValue(789012);

  // A new frame is captured.
  scoped_refptr<media::VideoFrame> captured_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(width, height));
  captured_frame->set_timestamp(ts);

  // The frame is delivered to VideoSourceHandler.
  handler_->DeliverFrameForTesting(&reader, captured_frame);

  // Compare |frame| to |captured_frame|.
  const media::VideoFrame* frame = reader.last_frame();
  ASSERT_TRUE(frame != NULL);
  EXPECT_EQ(width, frame->coded_size().width());
  EXPECT_EQ(height, frame->coded_size().height());
  EXPECT_EQ(ts, frame->timestamp());
  EXPECT_EQ(captured_frame->data(media::VideoFrame::kYPlane),
            frame->data(media::VideoFrame::kYPlane));

  EXPECT_FALSE(handler_->Close(NULL));
  EXPECT_TRUE(handler_->Close(&reader));
}

TEST_F(VideoSourceHandlerTest, OpenWithoutClose) {
  FakeFrameReader reader;
  EXPECT_TRUE(handler_->Open(kTestStreamUrl, &reader));
}

}  // namespace content
