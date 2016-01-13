// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_ALSA)
#include "media/audio/alsa/audio_manager_alsa.h"
#elif defined(OS_ANDROID)
#include "media/audio/android/audio_manager_android.h"
#elif defined(OS_MACOSX)
#include "media/audio/mac/audio_manager_mac.h"
#elif defined(OS_WIN)
#include "media/audio/win/audio_manager_win.h"
#else
#include "media/audio/fake_audio_manager.h"
#endif

using testing::_;

namespace content {

#if defined(USE_ALSA)
typedef media::AudioManagerAlsa AudioManagerPlatform;
#elif defined(OS_MACOSX)
typedef media::AudioManagerMac AudioManagerPlatform;
#elif defined(OS_WIN)
typedef media::AudioManagerWin AudioManagerPlatform;
#elif defined(OS_ANDROID)
typedef media::AudioManagerAndroid AudioManagerPlatform;
#else
typedef media::FakeAudioManager AudioManagerPlatform;
#endif


// This class mocks the audio manager and overrides the
// GetAudioInputDeviceNames() method to ensure that we can run our tests on
// the buildbots. media::AudioManagerBase
class MockAudioManager : public AudioManagerPlatform {
 public:
  MockAudioManager() : AudioManagerPlatform(&fake_audio_log_factory_) {}
  virtual ~MockAudioManager() {}

  virtual void GetAudioInputDeviceNames(
      media::AudioDeviceNames* device_names) OVERRIDE {
    DCHECK(device_names->empty());
    if (HasAudioInputDevices()) {
      AudioManagerBase::GetAudioInputDeviceNames(device_names);
    } else {
      device_names->push_back(media::AudioDeviceName("fake_device_name",
                                                     "fake_device_id"));
    }
  }

 private:
  media::FakeAudioLogFactory fake_audio_log_factory_;
  DISALLOW_COPY_AND_ASSIGN(MockAudioManager);
};

class MediaStreamManagerTest : public ::testing::Test {
 public:
  MediaStreamManagerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        message_loop_(base::MessageLoopProxy::current()) {
    // Create our own MediaStreamManager. Use fake devices to run on the bots.
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    audio_manager_.reset(new MockAudioManager());
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));
}

  virtual ~MediaStreamManagerTest() {
  }

  MOCK_METHOD1(Response, void(int index));
  void ResponseCallback(int index,
                        const MediaStreamDevices& devices,
                        scoped_ptr<MediaStreamUIProxy> ui_proxy) {
    Response(index);
    message_loop_->PostTask(FROM_HERE, run_loop_.QuitClosure());
  }

 protected:
  std::string MakeMediaAccessRequest(int index) {
    const int render_process_id = 1;
    const int render_view_id = 1;
    const int page_request_id = 1;
    const GURL security_origin;
    MediaStreamManager::MediaRequestResponseCallback callback =
        base::Bind(&MediaStreamManagerTest::ResponseCallback,
                   base::Unretained(this), index);
    StreamOptions options(true, true);
    return media_stream_manager_->MakeMediaAccessRequest(render_process_id,
                                                         render_view_id,
                                                         page_request_id,
                                                         options,
                                                         security_origin,
                                                         callback);
  }

  scoped_ptr<media::AudioManager> audio_manager_;
  scoped_ptr<MediaStreamManager> media_stream_manager_;
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::RunLoop run_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamManagerTest);
};

TEST_F(MediaStreamManagerTest, MakeMediaAccessRequest) {
  MakeMediaAccessRequest(0);

  // Expecting the callback will be triggered and quit the test.
  EXPECT_CALL(*this, Response(0));
  run_loop_.Run();
}

TEST_F(MediaStreamManagerTest, MakeAndCancelMediaAccessRequest) {
  std::string label = MakeMediaAccessRequest(0);
  // No callback is expected.
  media_stream_manager_->CancelRequest(label);
  run_loop_.RunUntilIdle();
  media_stream_manager_->WillDestroyCurrentMessageLoop();
}

TEST_F(MediaStreamManagerTest, MakeMultipleRequests) {
  // First request.
  std::string label1 =  MakeMediaAccessRequest(0);

  // Second request.
  int render_process_id = 2;
  int render_view_id = 2;
  int page_request_id = 2;
  GURL security_origin;
  StreamOptions options(true, true);
  MediaStreamManager::MediaRequestResponseCallback callback =
      base::Bind(&MediaStreamManagerTest::ResponseCallback,
                 base::Unretained(this), 1);
  std::string label2 = media_stream_manager_->MakeMediaAccessRequest(
      render_process_id,
      render_view_id,
      page_request_id,
      options,
      security_origin,
      callback);

  // Expecting the callbackS from requests will be triggered and quit the test.
  // Note, the callbacks might come in a different order depending on the
  // value of labels.
  EXPECT_CALL(*this, Response(0));
  EXPECT_CALL(*this, Response(1));
  run_loop_.Run();
}

TEST_F(MediaStreamManagerTest, MakeAndCancelMultipleRequests) {
  std::string label1 = MakeMediaAccessRequest(0);
  std::string label2 = MakeMediaAccessRequest(1);
  media_stream_manager_->CancelRequest(label1);

  // Expecting the callback from the second request will be triggered and
  // quit the test.
  EXPECT_CALL(*this, Response(1));
  run_loop_.Run();
}

}  // namespace content
