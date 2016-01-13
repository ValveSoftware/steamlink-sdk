// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/audio_mirroring_manager.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/browser_thread_impl.h"
#include "media/audio/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::AudioOutputStream;
using media::AudioParameters;
using testing::_;
using testing::NotNull;
using testing::Ref;
using testing::Return;
using testing::ReturnRef;

namespace content {

namespace {

class MockDiverter : public AudioMirroringManager::Diverter {
 public:
  MOCK_METHOD0(GetAudioParameters, const AudioParameters&());
  MOCK_METHOD1(StartDiverting, void(AudioOutputStream*));
  MOCK_METHOD0(StopDiverting, void());
};

class MockMirroringDestination
    : public AudioMirroringManager::MirroringDestination {
 public:
  MOCK_METHOD1(AddInput,
               media::AudioOutputStream*(const media::AudioParameters& params));
};

}  // namespace

class AudioMirroringManagerTest : public testing::Test {
 public:
  AudioMirroringManagerTest()
      : io_thread_(BrowserThread::IO, &message_loop_),
        params_(AudioParameters::AUDIO_FAKE, media::CHANNEL_LAYOUT_STEREO,
                AudioParameters::kAudioCDSampleRate, 16,
                AudioParameters::kAudioCDSampleRate / 10) {}

  MockDiverter* CreateStream(
      int render_process_id, int render_view_id, int expected_times_diverted) {
    MockDiverter* const diverter = new MockDiverter();
    if (expected_times_diverted > 0) {
      EXPECT_CALL(*diverter, GetAudioParameters())
          .Times(expected_times_diverted)
          .WillRepeatedly(ReturnRef(params_));
      EXPECT_CALL(*diverter, StartDiverting(NotNull()))
          .Times(expected_times_diverted);
      EXPECT_CALL(*diverter, StopDiverting())
          .Times(expected_times_diverted);
    }

    mirroring_manager_.AddDiverter(render_process_id, render_view_id, diverter);

    return diverter;
  }

  void KillStream(
      int render_process_id, int render_view_id, MockDiverter* diverter) {
    mirroring_manager_.RemoveDiverter(
        render_process_id, render_view_id, diverter);

    delete diverter;
  }

  MockMirroringDestination* StartMirroringTo(
      int render_process_id, int render_view_id, int expected_inputs_added) {
    MockMirroringDestination* const dest = new MockMirroringDestination();
    if (expected_inputs_added > 0) {
      static AudioOutputStream* const kNonNullPointer =
          reinterpret_cast<AudioOutputStream*>(0x11111110);
      EXPECT_CALL(*dest, AddInput(Ref(params_)))
          .Times(expected_inputs_added)
          .WillRepeatedly(Return(kNonNullPointer));
    }

    mirroring_manager_.StartMirroring(render_process_id, render_view_id, dest);

    return dest;
  }

  void StopMirroringTo(int render_process_id, int render_view_id,
                       MockMirroringDestination* dest) {
    mirroring_manager_.StopMirroring(render_process_id, render_view_id, dest);

    delete dest;
}

 private:
  base::MessageLoopForIO message_loop_;
  BrowserThreadImpl io_thread_;
  AudioParameters params_;
  AudioMirroringManager mirroring_manager_;

  DISALLOW_COPY_AND_ASSIGN(AudioMirroringManagerTest);
};

namespace {
const int kRenderProcessId = 123;
const int kRenderViewId = 456;
const int kAnotherRenderProcessId = 789;
const int kAnotherRenderViewId = 1234;
const int kYetAnotherRenderProcessId = 4560;
const int kYetAnotherRenderViewId = 7890;
}

TEST_F(AudioMirroringManagerTest, MirroringSessionOfNothing) {
  MockMirroringDestination* const destination =
      StartMirroringTo(kRenderProcessId, kRenderViewId, 0);
  StopMirroringTo(kRenderProcessId, kRenderViewId, destination);
}

TEST_F(AudioMirroringManagerTest, TwoMirroringSessionsOfNothing) {
  MockMirroringDestination* const destination =
      StartMirroringTo(kRenderProcessId, kRenderViewId, 0);
  StopMirroringTo(kRenderProcessId, kRenderViewId, destination);

  MockMirroringDestination* const another_destination =
      StartMirroringTo(kAnotherRenderProcessId, kAnotherRenderViewId, 0);
  StopMirroringTo(kAnotherRenderProcessId, kAnotherRenderViewId,
                  another_destination);
}

TEST_F(AudioMirroringManagerTest, SwitchMirroringDestinationNoStreams) {
  MockMirroringDestination* const destination =
      StartMirroringTo(kRenderProcessId, kRenderViewId, 0);
  MockMirroringDestination* const new_destination =
      StartMirroringTo(kRenderProcessId, kRenderViewId, 0);
  StopMirroringTo(kRenderProcessId, kRenderViewId, destination);
  StopMirroringTo(kRenderProcessId, kRenderViewId, new_destination);
}

TEST_F(AudioMirroringManagerTest, StreamLifetimeAroundMirroringSession) {
  MockDiverter* const stream = CreateStream(kRenderProcessId, kRenderViewId, 1);
  MockMirroringDestination* const destination =
      StartMirroringTo(kRenderProcessId, kRenderViewId, 1);
  StopMirroringTo(kRenderProcessId, kRenderViewId, destination);
  KillStream(kRenderProcessId, kRenderViewId, stream);
}

TEST_F(AudioMirroringManagerTest, StreamLifetimeWithinMirroringSession) {
  MockMirroringDestination* const destination =
      StartMirroringTo(kRenderProcessId, kRenderViewId, 1);
  MockDiverter* const stream = CreateStream(kRenderProcessId, kRenderViewId, 1);
  KillStream(kRenderProcessId, kRenderViewId, stream);
  StopMirroringTo(kRenderProcessId, kRenderViewId, destination);
}

TEST_F(AudioMirroringManagerTest, StreamLifetimeAroundTwoMirroringSessions) {
  MockDiverter* const stream = CreateStream(kRenderProcessId, kRenderViewId, 2);
  MockMirroringDestination* const destination =
      StartMirroringTo(kRenderProcessId, kRenderViewId, 1);
  StopMirroringTo(kRenderProcessId, kRenderViewId, destination);
  MockMirroringDestination* const new_destination =
      StartMirroringTo(kRenderProcessId, kRenderViewId, 1);
  StopMirroringTo(kRenderProcessId, kRenderViewId, new_destination);
  KillStream(kRenderProcessId, kRenderViewId, stream);
}

TEST_F(AudioMirroringManagerTest, StreamLifetimeWithinTwoMirroringSessions) {
  MockMirroringDestination* const destination =
      StartMirroringTo(kRenderProcessId, kRenderViewId, 1);
  MockDiverter* const stream = CreateStream(kRenderProcessId, kRenderViewId, 2);
  StopMirroringTo(kRenderProcessId, kRenderViewId, destination);
  MockMirroringDestination* const new_destination =
      StartMirroringTo(kRenderProcessId, kRenderViewId, 1);
  KillStream(kRenderProcessId, kRenderViewId, stream);
  StopMirroringTo(kRenderProcessId, kRenderViewId, new_destination);
}

TEST_F(AudioMirroringManagerTest, MultipleStreamsInOneMirroringSession) {
  MockDiverter* const stream1 =
      CreateStream(kRenderProcessId, kRenderViewId, 1);
  MockMirroringDestination* const destination =
      StartMirroringTo(kRenderProcessId, kRenderViewId, 3);
  MockDiverter* const stream2 =
      CreateStream(kRenderProcessId, kRenderViewId, 1);
  MockDiverter* const stream3 =
      CreateStream(kRenderProcessId, kRenderViewId, 1);
  KillStream(kRenderProcessId, kRenderViewId, stream2);
  StopMirroringTo(kRenderProcessId, kRenderViewId, destination);
  KillStream(kRenderProcessId, kRenderViewId, stream3);
  KillStream(kRenderProcessId, kRenderViewId, stream1);
}

// A random interleaving of operations for three separate targets, each of which
// has one stream mirrored to one destination.
TEST_F(AudioMirroringManagerTest, ThreeSeparateMirroringSessions) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderViewId, 1);
  MockMirroringDestination* const destination =
      StartMirroringTo(kRenderProcessId, kRenderViewId, 1);

  MockMirroringDestination* const another_destination =
      StartMirroringTo(kAnotherRenderProcessId, kAnotherRenderViewId, 1);
  MockDiverter* const another_stream =
      CreateStream(kAnotherRenderProcessId, kAnotherRenderViewId, 1);

  KillStream(kRenderProcessId, kRenderViewId, stream);

  MockDiverter* const yet_another_stream =
      CreateStream(kYetAnotherRenderProcessId, kYetAnotherRenderViewId, 1);
  MockMirroringDestination* const yet_another_destination =
      StartMirroringTo(kYetAnotherRenderProcessId, kYetAnotherRenderViewId, 1);

  StopMirroringTo(kAnotherRenderProcessId, kAnotherRenderViewId,
                  another_destination);

  StopMirroringTo(kYetAnotherRenderProcessId, kYetAnotherRenderViewId,
                  yet_another_destination);

  StopMirroringTo(kRenderProcessId, kRenderViewId, destination);

  KillStream(kAnotherRenderProcessId, kAnotherRenderViewId, another_stream);
  KillStream(kYetAnotherRenderProcessId, kYetAnotherRenderViewId,
             yet_another_stream);
}

}  // namespace content
