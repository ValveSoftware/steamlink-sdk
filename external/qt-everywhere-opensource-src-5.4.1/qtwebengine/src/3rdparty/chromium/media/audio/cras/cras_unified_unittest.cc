// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "media/audio/cras/audio_manager_cras.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/mock_audio_source_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// cras_util.h defines custom min/max macros which break compilation, so ensure
// it's not included until last.  #if avoids presubmit errors.
#if defined(USE_CRAS)
#include "media/audio/cras/cras_unified.h"
#endif

using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace media {

class MockAudioManagerCras : public AudioManagerCras {
 public:
  MockAudioManagerCras() : AudioManagerCras(&fake_audio_log_factory_) {}

  MOCK_METHOD0(Init, void());
  MOCK_METHOD0(HasAudioOutputDevices, bool());
  MOCK_METHOD0(HasAudioInputDevices, bool());
  MOCK_METHOD1(MakeLinearOutputStream, AudioOutputStream*(
      const AudioParameters& params));
  MOCK_METHOD2(MakeLowLatencyOutputStream,
               AudioOutputStream*(const AudioParameters& params,
                                  const std::string& device_id));
  MOCK_METHOD2(MakeLinearOutputStream, AudioInputStream*(
      const AudioParameters& params, const std::string& device_id));
  MOCK_METHOD2(MakeLowLatencyInputStream, AudioInputStream*(
      const AudioParameters& params, const std::string& device_id));

  // We need to override this function in order to skip the checking the number
  // of active output streams. It is because the number of active streams
  // is managed inside MakeAudioOutputStream, and we don't use
  // MakeAudioOutputStream to create the stream in the tests.
  virtual void ReleaseOutputStream(AudioOutputStream* stream) OVERRIDE {
    DCHECK(stream);
    delete stream;
  }

 private:
  FakeAudioLogFactory fake_audio_log_factory_;
};

class CrasUnifiedStreamTest : public testing::Test {
 protected:
  CrasUnifiedStreamTest() {
    mock_manager_.reset(new StrictMock<MockAudioManagerCras>());
  }

  virtual ~CrasUnifiedStreamTest() {
  }

  CrasUnifiedStream* CreateStream(ChannelLayout layout) {
    return CreateStream(layout, kTestFramesPerPacket);
  }

  CrasUnifiedStream* CreateStream(ChannelLayout layout,
                                 int32 samples_per_packet) {
    AudioParameters params(kTestFormat, layout, kTestSampleRate,
                           kTestBitsPerSample, samples_per_packet);
    return new CrasUnifiedStream(params, mock_manager_.get());
  }

  MockAudioManagerCras& mock_manager() {
    return *(mock_manager_.get());
  }

  static const ChannelLayout kTestChannelLayout;
  static const int kTestSampleRate;
  static const int kTestBitsPerSample;
  static const AudioParameters::Format kTestFormat;
  static const uint32 kTestFramesPerPacket;

  scoped_ptr<StrictMock<MockAudioManagerCras> > mock_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrasUnifiedStreamTest);
};

const ChannelLayout CrasUnifiedStreamTest::kTestChannelLayout =
    CHANNEL_LAYOUT_STEREO;
const int CrasUnifiedStreamTest::kTestSampleRate =
    AudioParameters::kAudioCDSampleRate;
const int CrasUnifiedStreamTest::kTestBitsPerSample = 16;
const AudioParameters::Format CrasUnifiedStreamTest::kTestFormat =
    AudioParameters::AUDIO_PCM_LINEAR;
const uint32 CrasUnifiedStreamTest::kTestFramesPerPacket = 1000;

TEST_F(CrasUnifiedStreamTest, ConstructedState) {
  // Should support mono.
  CrasUnifiedStream* test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  test_stream->Close();

  // Should support stereo.
  test_stream = CreateStream(CHANNEL_LAYOUT_SURROUND);
  EXPECT_TRUE(test_stream->Open());
  test_stream->Close();

  // Bad bits per sample.
  AudioParameters bad_bps_params(kTestFormat, kTestChannelLayout,
                                 kTestSampleRate, kTestBitsPerSample - 1,
                                 kTestFramesPerPacket);
  test_stream = new CrasUnifiedStream(bad_bps_params, mock_manager_.get());
  EXPECT_FALSE(test_stream->Open());
  test_stream->Close();

  // Bad sample rate.
  AudioParameters bad_rate_params(kTestFormat, kTestChannelLayout,
                                  0, kTestBitsPerSample, kTestFramesPerPacket);
  test_stream = new CrasUnifiedStream(bad_rate_params, mock_manager_.get());
  EXPECT_FALSE(test_stream->Open());
  test_stream->Close();

  // Check that Mono works too.
  test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  ASSERT_TRUE(test_stream->Open());
  test_stream->Close();
}

TEST_F(CrasUnifiedStreamTest, RenderFrames) {
  CrasUnifiedStream* test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  MockAudioSourceCallback mock_callback;

  ASSERT_TRUE(test_stream->Open());

  base::WaitableEvent event(false, false);

  EXPECT_CALL(mock_callback, OnMoreData(_, _))
      .WillRepeatedly(DoAll(
          InvokeWithoutArgs(&event, &base::WaitableEvent::Signal),
                            Return(kTestFramesPerPacket)));

  test_stream->Start(&mock_callback);

  // Wait for samples to be captured.
  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_timeout()));

  test_stream->Stop();

  test_stream->Close();
}

}  // namespace media
