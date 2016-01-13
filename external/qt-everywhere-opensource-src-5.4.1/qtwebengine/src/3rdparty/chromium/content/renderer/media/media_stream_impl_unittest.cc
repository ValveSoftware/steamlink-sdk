// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_impl.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/mock_media_stream_dispatcher.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaDeviceInfo.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

class MockMediaStreamVideoCapturerSource : public MockMediaStreamVideoSource {
 public:
  MockMediaStreamVideoCapturerSource(
      const StreamDeviceInfo& device,
      const SourceStoppedCallback& stop_callback,
      PeerConnectionDependencyFactory* factory)
  : MockMediaStreamVideoSource(false) {
    SetDeviceInfo(device);
    SetStopCallback(stop_callback);
  }
};

class MediaStreamImplUnderTest : public MediaStreamImpl {
 public:
  enum RequestState {
    REQUEST_NOT_STARTED,
    REQUEST_NOT_COMPLETE,
    REQUEST_SUCCEEDED,
    REQUEST_FAILED,
  };

  MediaStreamImplUnderTest(MediaStreamDispatcher* media_stream_dispatcher,
                           PeerConnectionDependencyFactory* dependency_factory)
      : MediaStreamImpl(NULL, media_stream_dispatcher, dependency_factory),
        state_(REQUEST_NOT_STARTED),
        result_(NUM_MEDIA_REQUEST_RESULTS),
        factory_(dependency_factory),
        video_source_(NULL) {
  }

  void RequestUserMedia() {
    blink::WebUserMediaRequest user_media_request;
    state_ = REQUEST_NOT_COMPLETE;
    requestUserMedia(user_media_request);
  }

  void RequestMediaDevices() {
    blink::WebMediaDevicesRequest media_devices_request;
    state_ = REQUEST_NOT_COMPLETE;
    requestMediaDevices(media_devices_request);
  }

  virtual void GetUserMediaRequestSucceeded(
      const blink::WebMediaStream& stream,
      blink::WebUserMediaRequest* request_info) OVERRIDE {
    last_generated_stream_ = stream;
    state_ = REQUEST_SUCCEEDED;
  }

  virtual void GetUserMediaRequestFailed(
      blink::WebUserMediaRequest* request_info,
      content::MediaStreamRequestResult result) OVERRIDE {
    last_generated_stream_.reset();
    state_ = REQUEST_FAILED;
    result_ = result;
  }

  virtual void EnumerateDevicesSucceded(
      blink::WebMediaDevicesRequest* request,
      blink::WebVector<blink::WebMediaDeviceInfo>& devices) OVERRIDE {
    state_ = REQUEST_SUCCEEDED;
    last_devices_ = devices;
  }

  virtual MediaStreamVideoSource* CreateVideoSource(
      const StreamDeviceInfo& device,
      const MediaStreamSource::SourceStoppedCallback& stop_callback) OVERRIDE {
    video_source_ = new MockMediaStreamVideoCapturerSource(device,
                                                           stop_callback,
                                                           factory_);
    return video_source_;
  }

  const blink::WebMediaStream& last_generated_stream() {
    return last_generated_stream_;
  }

  const blink::WebVector<blink::WebMediaDeviceInfo>& last_devices() {
    return last_devices_;
  }

  void ClearLastGeneratedStream() {
    last_generated_stream_.reset();
  }

  MockMediaStreamVideoCapturerSource* last_created_video_source() const {
    return video_source_;
  }

  RequestState request_state() const { return state_; }
  content::MediaStreamRequestResult error_reason() const { return result_; }

 private:
  blink::WebMediaStream last_generated_stream_;
  RequestState state_;
  content::MediaStreamRequestResult result_;
  blink::WebVector<blink::WebMediaDeviceInfo> last_devices_;
  PeerConnectionDependencyFactory* factory_;
  MockMediaStreamVideoCapturerSource* video_source_;
};

class MediaStreamImplTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    // Create our test object.
    child_process_.reset(new ChildProcess());
    ms_dispatcher_.reset(new MockMediaStreamDispatcher());
    dependency_factory_.reset(new MockPeerConnectionDependencyFactory());
    ms_impl_.reset(new MediaStreamImplUnderTest(ms_dispatcher_.get(),
                                                dependency_factory_.get()));
  }

  blink::WebMediaStream RequestLocalMediaStream() {
    ms_impl_->RequestUserMedia();
    FakeMediaStreamDispatcherRequestUserMediaComplete();
    StartMockedVideoSource();

    EXPECT_EQ(MediaStreamImplUnderTest::REQUEST_SUCCEEDED,
              ms_impl_->request_state());

    blink::WebMediaStream desc = ms_impl_->last_generated_stream();
    content::MediaStream* native_stream =
        content::MediaStream::GetMediaStream(desc);
    if (!native_stream) {
      ADD_FAILURE();
      return desc;
    }

    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
    desc.audioTracks(audio_tracks);
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    desc.videoTracks(video_tracks);

    EXPECT_EQ(1u, audio_tracks.size());
    EXPECT_EQ(1u, video_tracks.size());
    EXPECT_NE(audio_tracks[0].id(), video_tracks[0].id());
    return desc;
  }

  void FakeMediaStreamDispatcherRequestUserMediaComplete() {
    // Audio request ID is used as the shared request ID.
    ms_impl_->OnStreamGenerated(ms_dispatcher_->audio_input_request_id(),
                                ms_dispatcher_->stream_label(),
                                ms_dispatcher_->audio_input_array(),
                                ms_dispatcher_->video_array());
  }

  void FakeMediaStreamDispatcherRequestMediaDevicesComplete() {
    ms_impl_->OnDevicesEnumerated(ms_dispatcher_->audio_input_request_id(),
                                  ms_dispatcher_->audio_input_array());
    ms_impl_->OnDevicesEnumerated(ms_dispatcher_->audio_output_request_id(),
                                  ms_dispatcher_->audio_output_array());
    ms_impl_->OnDevicesEnumerated(ms_dispatcher_->video_request_id(),
                                  ms_dispatcher_->video_array());
  }

  void StartMockedVideoSource() {
    MockMediaStreamVideoCapturerSource* video_source =
        ms_impl_->last_created_video_source();
    if (video_source->SourceHasAttemptedToStart())
      video_source->StartMockedSource();
  }

  void FailToStartMockedVideoSource() {
    MockMediaStreamVideoCapturerSource* video_source =
        ms_impl_->last_created_video_source();
    if (video_source->SourceHasAttemptedToStart())
      video_source->FailToStartMockedSource();
  }

  void FailToCreateNextAudioCapturer() {
    dependency_factory_->FailToCreateNextAudioCapturer();
  }

 protected:
  base::MessageLoop message_loop_;
  scoped_ptr<ChildProcess> child_process_;
  scoped_ptr<MockMediaStreamDispatcher> ms_dispatcher_;
  scoped_ptr<MediaStreamImplUnderTest> ms_impl_;
  scoped_ptr<MockPeerConnectionDependencyFactory> dependency_factory_;
};

TEST_F(MediaStreamImplTest, GenerateMediaStream) {
  // Generate a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();
}

// Test that the same source object is used if two MediaStreams are generated
// using the same source.
TEST_F(MediaStreamImplTest, GenerateTwoMediaStreamsWithSameSource) {
  blink::WebMediaStream desc1 = RequestLocalMediaStream();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> desc1_video_tracks;
  desc1.videoTracks(desc1_video_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_video_tracks;
  desc2.videoTracks(desc2_video_tracks);
  EXPECT_EQ(desc1_video_tracks[0].source().id(),
            desc2_video_tracks[0].source().id());

  EXPECT_EQ(desc1_video_tracks[0].source().extraData(),
            desc2_video_tracks[0].source().extraData());

  blink::WebVector<blink::WebMediaStreamTrack> desc1_audio_tracks;
  desc1.audioTracks(desc1_audio_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_audio_tracks;
  desc2.audioTracks(desc2_audio_tracks);
  EXPECT_EQ(desc1_audio_tracks[0].source().id(),
            desc2_audio_tracks[0].source().id());

  EXPECT_EQ(desc1_audio_tracks[0].source().extraData(),
            desc2_audio_tracks[0].source().extraData());
}

// Test that the same source object is not used if two MediaStreams are
// generated using different sources.
TEST_F(MediaStreamImplTest, GenerateTwoMediaStreamsWithDifferentSources) {
  blink::WebMediaStream desc1 = RequestLocalMediaStream();
  // Make sure another device is selected (another |session_id|) in  the next
  // gUM request.
  ms_dispatcher_->IncrementSessionId();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> desc1_video_tracks;
  desc1.videoTracks(desc1_video_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_video_tracks;
  desc2.videoTracks(desc2_video_tracks);
  EXPECT_NE(desc1_video_tracks[0].source().id(),
            desc2_video_tracks[0].source().id());

  EXPECT_NE(desc1_video_tracks[0].source().extraData(),
            desc2_video_tracks[0].source().extraData());

  blink::WebVector<blink::WebMediaStreamTrack> desc1_audio_tracks;
  desc1.audioTracks(desc1_audio_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_audio_tracks;
  desc2.audioTracks(desc2_audio_tracks);
  EXPECT_NE(desc1_audio_tracks[0].source().id(),
            desc2_audio_tracks[0].source().id());

  EXPECT_NE(desc1_audio_tracks[0].source().extraData(),
            desc2_audio_tracks[0].source().extraData());
}

TEST_F(MediaStreamImplTest, StopLocalTracks) {
  // Generate a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  mixed_desc.audioTracks(audio_tracks);
  MediaStreamTrack* audio_track = MediaStreamTrack::GetTrack(audio_tracks[0]);
  audio_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  mixed_desc.videoTracks(video_tracks);
  MediaStreamTrack* video_track = MediaStreamTrack::GetTrack(video_tracks[0]);
  video_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// This test that a source is not stopped even if the tracks in a
// MediaStream is stopped if there are two MediaStreams with tracks using the
// same device. The source is stopped
// if there are no more MediaStream tracks using the device.
TEST_F(MediaStreamImplTest, StopLocalTracksWhenTwoStreamUseSameDevices) {
  // Generate a stream with both audio and video.
  blink::WebMediaStream desc1 = RequestLocalMediaStream();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks1;
  desc1.audioTracks(audio_tracks1);
  MediaStreamTrack* audio_track1 = MediaStreamTrack::GetTrack(audio_tracks1[0]);
  audio_track1->Stop();
  EXPECT_EQ(0, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks2;
  desc2.audioTracks(audio_tracks2);
  MediaStreamTrack* audio_track2 = MediaStreamTrack::GetTrack(audio_tracks2[0]);
  audio_track2->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks1;
  desc1.videoTracks(video_tracks1);
  MediaStreamTrack* video_track1 = MediaStreamTrack::GetTrack(video_tracks1[0]);
  video_track1->Stop();
  EXPECT_EQ(0, ms_dispatcher_->stop_video_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks2;
  desc2.videoTracks(video_tracks2);
  MediaStreamTrack* video_track2 = MediaStreamTrack::GetTrack(video_tracks2[0]);
  video_track2->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

TEST_F(MediaStreamImplTest, StopSourceWhenMediaStreamGoesOutOfScope) {
  // Generate a stream with both audio and video.
  RequestLocalMediaStream();
  // Makes sure the test itself don't hold a reference to the created
  // MediaStream.
  ms_impl_->ClearLastGeneratedStream();

  // Expect the sources to be stopped when the MediaStream goes out of scope.
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// Test that the MediaStreams are deleted if the owning WebFrame is deleted.
// In the unit test the owning frame is NULL.
TEST_F(MediaStreamImplTest, FrameWillClose) {
  // Test a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();

  // Test that the MediaStreams are deleted if the owning WebFrame is deleted.
  // In the unit test the owning frame is NULL.
  ms_impl_->FrameWillClose(NULL);
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// This test what happens if a video source to a MediaSteam fails to start.
TEST_F(MediaStreamImplTest, MediaVideoSourceFailToStart) {
  ms_impl_->RequestUserMedia();
  FakeMediaStreamDispatcherRequestUserMediaComplete();
  FailToStartMockedVideoSource();
  EXPECT_EQ(MediaStreamImplUnderTest::REQUEST_FAILED,
            ms_impl_->request_state());
  EXPECT_EQ(MEDIA_DEVICE_TRACK_START_FAILURE,
            ms_impl_->error_reason());
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// This test what happens if an audio source fail to initialize.
TEST_F(MediaStreamImplTest, MediaAudioSourceFailToInitialize) {
  FailToCreateNextAudioCapturer();
  ms_impl_->RequestUserMedia();
  FakeMediaStreamDispatcherRequestUserMediaComplete();
  StartMockedVideoSource();
  EXPECT_EQ(MediaStreamImplUnderTest::REQUEST_FAILED,
            ms_impl_->request_state());
  EXPECT_EQ(MEDIA_DEVICE_TRACK_START_FAILURE,
            ms_impl_->error_reason());
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// This test what happens if MediaStreamImpl is deleted before a source has
// started.
TEST_F(MediaStreamImplTest, MediaStreamImplShutDown) {
  ms_impl_->RequestUserMedia();
  FakeMediaStreamDispatcherRequestUserMediaComplete();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(MediaStreamImplUnderTest::REQUEST_NOT_COMPLETE,
            ms_impl_->request_state());
  ms_impl_.reset();
}

// This test what happens if the WebFrame is closed while the MediaStream is
// being generated by the MediaStreamDispatcher.
TEST_F(MediaStreamImplTest, ReloadFrameWhileGeneratingStream) {
  ms_impl_->RequestUserMedia();
  ms_impl_->FrameWillClose(NULL);
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(0, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(0, ms_dispatcher_->stop_video_device_counter());
  EXPECT_EQ(MediaStreamImplUnderTest::REQUEST_NOT_COMPLETE,
            ms_impl_->request_state());
}

// This test what happens if the WebFrame is closed while the sources are being
// started.
TEST_F(MediaStreamImplTest, ReloadFrameWhileGeneratingSources) {
  ms_impl_->RequestUserMedia();
  FakeMediaStreamDispatcherRequestUserMediaComplete();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  ms_impl_->FrameWillClose(NULL);
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
  EXPECT_EQ(MediaStreamImplUnderTest::REQUEST_NOT_COMPLETE,
            ms_impl_->request_state());
}

// This test what happens if stop is called on a track after the frame has
// been reloaded.
TEST_F(MediaStreamImplTest, StopTrackAfterReload) {
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  ms_impl_->FrameWillClose(NULL);
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  mixed_desc.audioTracks(audio_tracks);
  MediaStreamTrack* audio_track = MediaStreamTrack::GetTrack(audio_tracks[0]);
  audio_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  mixed_desc.videoTracks(video_tracks);
  MediaStreamTrack* video_track = MediaStreamTrack::GetTrack(video_tracks[0]);
  video_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

TEST_F(MediaStreamImplTest, EnumerateMediaDevices) {
  ms_impl_->RequestMediaDevices();
  FakeMediaStreamDispatcherRequestMediaDevicesComplete();

  EXPECT_EQ(MediaStreamImplUnderTest::REQUEST_SUCCEEDED,
            ms_impl_->request_state());

  // Audio input device with matched output ID.
  EXPECT_FALSE(ms_impl_->last_devices()[0].deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindAudioInput,
            ms_impl_->last_devices()[0].kind());
  EXPECT_FALSE(ms_impl_->last_devices()[0].label().isEmpty());
  EXPECT_FALSE(ms_impl_->last_devices()[0].groupId().isEmpty());

  // Audio input device without matched output ID.
  EXPECT_FALSE(ms_impl_->last_devices()[1].deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindAudioInput,
            ms_impl_->last_devices()[1].kind());
  EXPECT_FALSE(ms_impl_->last_devices()[1].label().isEmpty());
  EXPECT_FALSE(ms_impl_->last_devices()[1].groupId().isEmpty());

  // Video input device.
  EXPECT_FALSE(ms_impl_->last_devices()[2].deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindVideoInput,
            ms_impl_->last_devices()[2].kind());
  EXPECT_FALSE(ms_impl_->last_devices()[2].label().isEmpty());
  EXPECT_TRUE(ms_impl_->last_devices()[2].groupId().isEmpty());

  // Audio output device.
  EXPECT_FALSE(ms_impl_->last_devices()[3].deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindAudioOutput,
            ms_impl_->last_devices()[3].kind());
  EXPECT_FALSE(ms_impl_->last_devices()[3].label().isEmpty());
  EXPECT_FALSE(ms_impl_->last_devices()[3].groupId().isEmpty());

  // Verfify group IDs.
  EXPECT_TRUE(ms_impl_->last_devices()[0].groupId().equals(
                  ms_impl_->last_devices()[3].groupId()));
  EXPECT_FALSE(ms_impl_->last_devices()[1].groupId().equals(
                   ms_impl_->last_devices()[3].groupId()));
}

}  // namespace content
