// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_stream_registry.h"
#include "content/renderer/media/mock_media_stream_video_sink.h"
#include "content/renderer/media/webrtc/video_destination_handler.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "content/test/ppapi_unittest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"

using ::testing::_;

namespace content {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

static const std::string kTestStreamUrl = "stream_url";
static const std::string kUnknownStreamUrl = "unknown_stream_url";

class VideoDestinationHandlerTest : public PpapiUnittest {
 public:
  VideoDestinationHandlerTest()
     : child_process_(new ChildProcess()),
       registry_(new MockMediaStreamRegistry()) {
    registry_->Init(kTestStreamUrl);
  }

  virtual void TearDown() {
    registry_.reset();
    PpapiUnittest::TearDown();
  }

  base::MessageLoop* io_message_loop() const {
    return child_process_->io_message_loop();
  }

 protected:
  scoped_ptr<ChildProcess> child_process_;
  scoped_ptr<MockMediaStreamRegistry> registry_;
};

TEST_F(VideoDestinationHandlerTest, Open) {
  FrameWriterInterface* frame_writer = NULL;
  // Unknow url will return false.
  EXPECT_FALSE(VideoDestinationHandler::Open(registry_.get(),
                                             kUnknownStreamUrl, &frame_writer));
  EXPECT_TRUE(VideoDestinationHandler::Open(registry_.get(),
                                            kTestStreamUrl, &frame_writer));
  // The |frame_writer| is a proxy and is owned by whoever call Open.
  delete frame_writer;
}

TEST_F(VideoDestinationHandlerTest, PutFrame) {
  FrameWriterInterface* frame_writer = NULL;
  EXPECT_TRUE(VideoDestinationHandler::Open(registry_.get(),
                                            kTestStreamUrl, &frame_writer));
  ASSERT_TRUE(frame_writer);

  // Verify the video track has been added.
  const blink::WebMediaStream test_stream = registry_->test_stream();
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  test_stream.videoTracks(video_tracks);
  ASSERT_EQ(1u, video_tracks.size());

  // Verify the native video track has been added.
  MediaStreamVideoTrack* native_track =
      MediaStreamVideoTrack::GetVideoTrack(video_tracks[0]);
  ASSERT_TRUE(native_track != NULL);

  MockMediaStreamVideoSink sink;
  native_track->AddSink(&sink, sink.GetDeliverFrameCB());
   scoped_refptr<PPB_ImageData_Impl> image(
      new PPB_ImageData_Impl(instance()->pp_instance(),
                             PPB_ImageData_Impl::ForTest()));
  image->Init(PP_IMAGEDATAFORMAT_BGRA_PREMUL, 640, 360, true);
  {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();

    EXPECT_CALL(sink, OnVideoFrame()).WillOnce(
        RunClosure(quit_closure));
    frame_writer->PutFrame(image, 10);
    run_loop.Run();
  }
  // TODO(perkj): Verify that the track output I420 when
  // https://codereview.chromium.org/213423006/ is landed.
  EXPECT_EQ(1, sink.number_of_frames());
  native_track->RemoveSink(&sink);

  // The |frame_writer| is a proxy and is owned by whoever call Open.
  delete frame_writer;
}

}  // namespace content
