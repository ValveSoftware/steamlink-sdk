// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TEST_HELPERS_H_
#define MEDIA_BASE_TEST_HELPERS_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "media/base/channel_layout.h"
#include "media/base/pipeline_status.h"
#include "media/base/sample_format.h"
#include "media/base/video_decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/size.h"

namespace base {
class MessageLoop;
class TimeDelta;
}

namespace media {

class AudioBuffer;
class DecoderBuffer;

// Return a callback that expects to be run once.
base::Closure NewExpectedClosure();
PipelineStatusCB NewExpectedStatusCB(PipelineStatus status);

// Helper class for running a message loop until a callback has run. Useful for
// testing classes that run on more than a single thread.
//
// Events are intended for single use and cannot be reset.
class WaitableMessageLoopEvent {
 public:
  WaitableMessageLoopEvent();
  ~WaitableMessageLoopEvent();

  // Returns a thread-safe closure that will signal |this| when executed.
  base::Closure GetClosure();
  PipelineStatusCB GetPipelineStatusCB();

  // Runs the current message loop until |this| has been signaled.
  //
  // Fails the test if the timeout is reached.
  void RunAndWait();

  // Runs the current message loop until |this| has been signaled and asserts
  // that the |expected| status was received.
  //
  // Fails the test if the timeout is reached.
  void RunAndWaitForStatus(PipelineStatus expected);

 private:
  void OnCallback(PipelineStatus status);
  void OnTimeout();

  base::MessageLoop* message_loop_;
  bool signaled_;
  PipelineStatus status_;

  DISALLOW_COPY_AND_ASSIGN(WaitableMessageLoopEvent);
};

// Provides pre-canned VideoDecoderConfig. These types are used for tests that
// don't care about detailed parameters of the config.
class TestVideoConfig {
 public:
  // Returns a configuration that is invalid.
  static VideoDecoderConfig Invalid();

  static VideoDecoderConfig Normal();
  static VideoDecoderConfig NormalEncrypted();

  // Returns a configuration that is larger in dimensions than Normal().
  static VideoDecoderConfig Large();
  static VideoDecoderConfig LargeEncrypted();

  // Returns coded size for Normal and Large config.
  static gfx::Size NormalCodedSize();
  static gfx::Size LargeCodedSize();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TestVideoConfig);
};

// Create an AudioBuffer containing |frames| frames of data, where each sample
// is of type T.  |start| and |increment| are used to specify the values for the
// samples, which are created in channel order.  The value for frame and channel
// is determined by:
//
//   |start| + |channel| * |frames| * |increment| + index * |increment|
//
// E.g., for a stereo buffer the values in channel 0 will be:
//   start
//   start + increment
//   start + 2 * increment, ...
//
// While, values in channel 1 will be:
//   start + frames * increment
//   start + (frames + 1) * increment
//   start + (frames + 2) * increment, ...
//
// |start_time| will be used as the start time for the samples.
template <class T>
scoped_refptr<AudioBuffer> MakeAudioBuffer(SampleFormat format,
                                           ChannelLayout channel_layout,
                                           size_t channel_count,
                                           int sample_rate,
                                           T start,
                                           T increment,
                                           size_t frames,
                                           base::TimeDelta timestamp);

// Create a fake video DecoderBuffer for testing purpose. The buffer contains
// part of video decoder config info embedded so that the testing code can do
// some sanity check.
scoped_refptr<DecoderBuffer> CreateFakeVideoBufferForTest(
    const VideoDecoderConfig& config,
    base::TimeDelta timestamp,
    base::TimeDelta duration);

// Verify if a fake video DecoderBuffer is valid.
bool VerifyFakeVideoBufferForTest(const scoped_refptr<DecoderBuffer>& buffer,
                                  const VideoDecoderConfig& config);

}  // namespace media

#endif  // MEDIA_BASE_TEST_HELPERS_H_
