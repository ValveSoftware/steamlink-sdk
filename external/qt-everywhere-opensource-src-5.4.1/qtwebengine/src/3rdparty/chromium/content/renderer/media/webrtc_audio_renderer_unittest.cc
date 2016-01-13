// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/media_stream_audio_renderer.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "media/audio/audio_output_device.h"
#include "media/audio/audio_output_ipc.h"
#include "media/base/audio_bus.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

using testing::Return;

namespace content {

namespace {

class MockAudioOutputIPC : public media::AudioOutputIPC {
 public:
  MockAudioOutputIPC() {}
  virtual ~MockAudioOutputIPC() {}

  MOCK_METHOD3(CreateStream, void(media::AudioOutputIPCDelegate* delegate,
                                  const media::AudioParameters& params,
                                  int session_id));
  MOCK_METHOD0(PlayStream, void());
  MOCK_METHOD0(PauseStream, void());
  MOCK_METHOD0(CloseStream, void());
  MOCK_METHOD1(SetVolume, void(double volume));
};

class FakeAudioOutputDevice
    : NON_EXPORTED_BASE(public media::AudioOutputDevice) {
 public:
  FakeAudioOutputDevice(
      scoped_ptr<media::AudioOutputIPC> ipc,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
      : AudioOutputDevice(ipc.Pass(),
                          io_task_runner) {}
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD0(Play, void());
  MOCK_METHOD1(SetVolume, bool(double volume));

 protected:
  virtual ~FakeAudioOutputDevice() {}
};

class MockAudioDeviceFactory : public AudioDeviceFactory {
 public:
  MockAudioDeviceFactory() {}
  virtual ~MockAudioDeviceFactory() {}
  MOCK_METHOD1(CreateOutputDevice, media::AudioOutputDevice*(int));
  MOCK_METHOD1(CreateInputDevice, media::AudioInputDevice*(int));
};

class MockAudioRendererSource : public WebRtcAudioRendererSource {
 public:
  MockAudioRendererSource() {}
  virtual ~MockAudioRendererSource() {}
  MOCK_METHOD4(RenderData, void(media::AudioBus* audio_bus,
                                int sample_rate,
                                int audio_delay_milliseconds,
                                base::TimeDelta* current_time));
  MOCK_METHOD1(RemoveAudioRenderer, void(WebRtcAudioRenderer* renderer));
};

}  // namespace

class WebRtcAudioRendererTest : public testing::Test {
 protected:
  WebRtcAudioRendererTest()
      : message_loop_(new base::MessageLoopForIO),
        mock_ipc_(new MockAudioOutputIPC()),
        mock_output_device_(new FakeAudioOutputDevice(
            scoped_ptr<media::AudioOutputIPC>(mock_ipc_),
            message_loop_->message_loop_proxy())),
        factory_(new MockAudioDeviceFactory()),
        source_(new MockAudioRendererSource()),
        stream_(new talk_base::RefCountedObject<MockMediaStream>("label")),
        renderer_(new WebRtcAudioRenderer(stream_, 1, 1, 1, 44100, 441)) {
    EXPECT_CALL(*factory_.get(), CreateOutputDevice(1))
        .WillOnce(Return(mock_output_device_));
    EXPECT_CALL(*mock_output_device_, Start());
    EXPECT_TRUE(renderer_->Initialize(source_.get()));
    renderer_proxy_ = renderer_->CreateSharedAudioRendererProxy(stream_);
  }

  // Used to construct |mock_output_device_|.
  scoped_ptr<base::MessageLoopForIO> message_loop_;
  MockAudioOutputIPC* mock_ipc_;  // Owned by AudioOuputDevice.

  scoped_refptr<FakeAudioOutputDevice> mock_output_device_;
  scoped_ptr<MockAudioDeviceFactory> factory_;
  scoped_ptr<MockAudioRendererSource> source_;
  scoped_refptr<webrtc::MediaStreamInterface> stream_;
  scoped_refptr<WebRtcAudioRenderer> renderer_;
  scoped_refptr<MediaStreamAudioRenderer> renderer_proxy_;
};

// Verify that the renderer will be stopped if the only proxy is stopped.
TEST_F(WebRtcAudioRendererTest, StopRenderer) {
  renderer_proxy_->Start();

  // |renderer_| has only one proxy, stopping the proxy should stop the sink of
  // |renderer_|.
  EXPECT_CALL(*mock_output_device_, Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  renderer_proxy_->Stop();
}

// Verify that the renderer will not be stopped unless the last proxy is
// stopped.
TEST_F(WebRtcAudioRendererTest, MultipleRenderers) {
  renderer_proxy_->Start();

  // Create a vector of renderer proxies from the |renderer_|.
  std::vector<scoped_refptr<MediaStreamAudioRenderer> > renderer_proxies_;
  static const int kNumberOfRendererProxy = 5;
  for (int i = 0; i < kNumberOfRendererProxy; ++i) {
    scoped_refptr<MediaStreamAudioRenderer> renderer_proxy(
        renderer_->CreateSharedAudioRendererProxy(stream_));
    renderer_proxy->Start();
    renderer_proxies_.push_back(renderer_proxy);
  }

  // Stop the |renderer_proxy_| should not stop the sink since it is used by
  // other proxies.
  EXPECT_CALL(*mock_output_device_, Stop()).Times(0);
  renderer_proxy_->Stop();

  for (int i = 0; i < kNumberOfRendererProxy; ++i) {
    if (i != kNumberOfRendererProxy -1) {
      EXPECT_CALL(*mock_output_device_, Stop()).Times(0);
    } else {
      // When the last proxy is stopped, the sink will stop.
      EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
      EXPECT_CALL(*mock_output_device_, Stop());
    }
    renderer_proxies_[i]->Stop();
  }
}

}  // namespace content
