// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/test_helpers.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/audio_buffer.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "ui/gfx/geometry/rect.h"

using ::testing::_;
using ::testing::StrictMock;

namespace media {

// Utility mock for testing methods expecting Closures and PipelineStatusCBs.
class MockCallback : public base::RefCountedThreadSafe<MockCallback> {
 public:
  MockCallback();
  MOCK_METHOD0(Run, void());
  MOCK_METHOD1(RunWithBool, void(bool));
  MOCK_METHOD1(RunWithStatus, void(PipelineStatus));

 protected:
  friend class base::RefCountedThreadSafe<MockCallback>;
  virtual ~MockCallback();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCallback);
};

MockCallback::MockCallback() {}
MockCallback::~MockCallback() {}

base::Closure NewExpectedClosure() {
  StrictMock<MockCallback>* callback = new StrictMock<MockCallback>();
  EXPECT_CALL(*callback, Run());
  return base::Bind(&MockCallback::Run, callback);
}

base::Callback<void(bool)> NewExpectedBoolCB(bool success) {
  StrictMock<MockCallback>* callback = new StrictMock<MockCallback>();
  EXPECT_CALL(*callback, RunWithBool(success));
  return base::Bind(&MockCallback::RunWithBool, callback);
}

PipelineStatusCB NewExpectedStatusCB(PipelineStatus status) {
  StrictMock<MockCallback>* callback = new StrictMock<MockCallback>();
  EXPECT_CALL(*callback, RunWithStatus(status));
  return base::Bind(&MockCallback::RunWithStatus, callback);
}

WaitableMessageLoopEvent::WaitableMessageLoopEvent()
    : WaitableMessageLoopEvent(TestTimeouts::action_timeout()) {}

WaitableMessageLoopEvent::WaitableMessageLoopEvent(base::TimeDelta timeout)
    : signaled_(false), status_(PIPELINE_OK), timeout_(timeout) {}

WaitableMessageLoopEvent::~WaitableMessageLoopEvent() {
  DCHECK(CalledOnValidThread());
}

base::Closure WaitableMessageLoopEvent::GetClosure() {
  DCHECK(CalledOnValidThread());
  return BindToCurrentLoop(base::Bind(
      &WaitableMessageLoopEvent::OnCallback, base::Unretained(this),
      PIPELINE_OK));
}

PipelineStatusCB WaitableMessageLoopEvent::GetPipelineStatusCB() {
  DCHECK(CalledOnValidThread());
  return BindToCurrentLoop(base::Bind(
      &WaitableMessageLoopEvent::OnCallback, base::Unretained(this)));
}

void WaitableMessageLoopEvent::RunAndWait() {
  DCHECK(CalledOnValidThread());
  RunAndWaitForStatus(PIPELINE_OK);
}

void WaitableMessageLoopEvent::RunAndWaitForStatus(PipelineStatus expected) {
  DCHECK(CalledOnValidThread());
  if (signaled_) {
    EXPECT_EQ(expected, status_);
    return;
  }

  run_loop_.reset(new base::RunLoop());
  base::Timer timer(false, false);
  timer.Start(
      FROM_HERE, timeout_,
      base::Bind(&WaitableMessageLoopEvent::OnTimeout, base::Unretained(this)));

  run_loop_->Run();
  EXPECT_TRUE(signaled_);
  EXPECT_EQ(expected, status_);
  run_loop_.reset();
}

void WaitableMessageLoopEvent::OnCallback(PipelineStatus status) {
  DCHECK(CalledOnValidThread());
  signaled_ = true;
  status_ = status;

  // |run_loop_| may be null if the callback fires before RunAndWaitForStatus().
  if (run_loop_)
    run_loop_->Quit();
}

void WaitableMessageLoopEvent::OnTimeout() {
  DCHECK(CalledOnValidThread());
  ADD_FAILURE() << "Timed out waiting for message loop to quit";
  run_loop_->Quit();
}

static VideoDecoderConfig GetTestConfig(VideoCodec codec,
                                        gfx::Size coded_size,
                                        bool is_encrypted) {
  gfx::Rect visible_rect(coded_size.width(), coded_size.height());
  gfx::Size natural_size = coded_size;

  return VideoDecoderConfig(
      codec, VIDEO_CODEC_PROFILE_UNKNOWN, PIXEL_FORMAT_YV12,
      COLOR_SPACE_UNSPECIFIED, coded_size, visible_rect, natural_size,
      EmptyExtraData(),
      is_encrypted ? AesCtrEncryptionScheme() : Unencrypted());
}

static const gfx::Size kNormalSize(320, 240);
static const gfx::Size kLargeSize(640, 480);

// static
VideoDecoderConfig TestVideoConfig::Invalid() {
  return GetTestConfig(kUnknownVideoCodec, kNormalSize, false);
}

// static
VideoDecoderConfig TestVideoConfig::Normal() {
  return GetTestConfig(kCodecVP8, kNormalSize, false);
}

// static
VideoDecoderConfig TestVideoConfig::NormalEncrypted() {
  return GetTestConfig(kCodecVP8, kNormalSize, true);
}

// static
VideoDecoderConfig TestVideoConfig::Large() {
  return GetTestConfig(kCodecVP8, kLargeSize, false);
}

// static
VideoDecoderConfig TestVideoConfig::LargeEncrypted() {
  return GetTestConfig(kCodecVP8, kLargeSize, true);
}

// static
gfx::Size TestVideoConfig::NormalCodedSize() {
  return kNormalSize;
}

// static
gfx::Size TestVideoConfig::LargeCodedSize() {
  return kLargeSize;
}

// static
AudioParameters TestAudioParameters::Normal() {
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_STEREO, 48000, 16, 2048);
}

template <class T>
scoped_refptr<AudioBuffer> MakeAudioBuffer(SampleFormat format,
                                           ChannelLayout channel_layout,
                                           size_t channel_count,
                                           int sample_rate,
                                           T start,
                                           T increment,
                                           size_t frames,
                                           base::TimeDelta timestamp) {
  const size_t channels = ChannelLayoutToChannelCount(channel_layout);
  scoped_refptr<AudioBuffer> output =
      AudioBuffer::CreateBuffer(format,
                                channel_layout,
                                static_cast<int>(channel_count),
                                sample_rate,
                                static_cast<int>(frames));
  output->set_timestamp(timestamp);

  const bool is_planar =
      format == kSampleFormatPlanarS16 || format == kSampleFormatPlanarF32;

  // Values in channel 0 will be:
  //   start
  //   start + increment
  //   start + 2 * increment, ...
  // While, values in channel 1 will be:
  //   start + frames * increment
  //   start + (frames + 1) * increment
  //   start + (frames + 2) * increment, ...
  for (size_t ch = 0; ch < channels; ++ch) {
    T* buffer =
        reinterpret_cast<T*>(output->channel_data()[is_planar ? ch : 0]);
    const T v = static_cast<T>(start + ch * frames * increment);
    for (size_t i = 0; i < frames; ++i) {
      buffer[is_planar ? i : ch + i * channels] =
          static_cast<T>(v + i * increment);
    }
  }
  return output;
}

// Instantiate all the types of MakeAudioBuffer() and
// MakeAudioBuffer() needed.
#define DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(type)              \
  template scoped_refptr<AudioBuffer> MakeAudioBuffer<type>( \
      SampleFormat format,                                   \
      ChannelLayout channel_layout,                          \
      size_t channel_count,                                  \
      int sample_rate,                                       \
      type start,                                            \
      type increment,                                        \
      size_t frames,                                         \
      base::TimeDelta start_time)
DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(uint8_t);
DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(int16_t);
DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(int32_t);
DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(float);

static const char kFakeVideoBufferHeader[] = "FakeVideoBufferForTest";

scoped_refptr<DecoderBuffer> CreateFakeVideoBufferForTest(
    const VideoDecoderConfig& config,
    base::TimeDelta timestamp, base::TimeDelta duration) {
  base::Pickle pickle;
  pickle.WriteString(kFakeVideoBufferHeader);
  pickle.WriteInt(config.coded_size().width());
  pickle.WriteInt(config.coded_size().height());
  pickle.WriteInt64(timestamp.InMilliseconds());

  scoped_refptr<DecoderBuffer> buffer =
      DecoderBuffer::CopyFrom(static_cast<const uint8_t*>(pickle.data()),
                              static_cast<int>(pickle.size()));
  buffer->set_timestamp(timestamp);
  buffer->set_duration(duration);
  buffer->set_is_key_frame(true);

  return buffer;
}

bool VerifyFakeVideoBufferForTest(
    const scoped_refptr<DecoderBuffer>& buffer,
    const VideoDecoderConfig& config) {
  // Check if the input |buffer| matches the |config|.
  base::PickleIterator pickle(
      base::Pickle(reinterpret_cast<const char*>(buffer->data()),
                   static_cast<int>(buffer->data_size())));
  std::string header;
  int width = 0;
  int height = 0;
  bool success = pickle.ReadString(&header) && pickle.ReadInt(&width) &&
                 pickle.ReadInt(&height);
  return (success && header == kFakeVideoBufferHeader &&
          width == config.coded_size().width() &&
          height == config.coded_size().height());
}

}  // namespace media
