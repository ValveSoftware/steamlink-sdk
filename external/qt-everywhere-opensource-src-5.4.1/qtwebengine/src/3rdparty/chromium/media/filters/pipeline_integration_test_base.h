// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_PIPELINE_INTEGRATION_TEST_BASE_H_
#define MEDIA_FILTERS_PIPELINE_INTEGRATION_TEST_BASE_H_

#include "base/md5.h"
#include "base/message_loop/message_loop.h"
#include "media/audio/clockless_audio_sink.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/demuxer.h"
#include "media/base/filter_collection.h"
#include "media/base/media_keys.h"
#include "media/base/pipeline.h"
#include "media/base/video_frame.h"
#include "media/filters/video_renderer_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class FilePath;
}

namespace media {

class Decryptor;

// Empty MD5 hash string.  Used to verify empty video tracks.
extern const char kNullVideoHash[];

// Empty hash string.  Used to verify empty audio tracks.
extern const char kNullAudioHash[];

// Dummy tick clock which advances extremely quickly (1 minute every time
// NowTicks() is called).
class DummyTickClock : public base::TickClock {
 public:
  DummyTickClock() : now_() {}
  virtual ~DummyTickClock() {}
  virtual base::TimeTicks NowTicks() OVERRIDE;
 private:
  base::TimeTicks now_;
};

// Integration tests for Pipeline. Real demuxers, real decoders, and
// base renderer implementations are used to verify pipeline functionality. The
// renderers used in these tests rely heavily on the AudioRendererBase &
// VideoRendererImpl implementations which contain a majority of the code used
// in the real AudioRendererImpl & SkCanvasVideoRenderer implementations used in
// the browser. The renderers in this test don't actually write data to a
// display or audio device. Both of these devices are simulated since they have
// little effect on verifying pipeline behavior and allow tests to run faster
// than real-time.
class PipelineIntegrationTestBase {
 public:
  PipelineIntegrationTestBase();
  virtual ~PipelineIntegrationTestBase();

  bool WaitUntilOnEnded();
  PipelineStatus WaitUntilEndedOrError();
  bool Start(const base::FilePath& file_path, PipelineStatus expected_status);
  // Enable playback with audio and video hashing enabled, or clockless
  // playback (audio only). Frame dropping and audio underflow will be disabled
  // if hashing enabled to ensure consistent hashes.
  enum kTestType { kHashed, kClockless };
  bool Start(const base::FilePath& file_path,
             PipelineStatus expected_status,
             kTestType test_type);
  // Initialize the pipeline and ignore any status updates.  Useful for testing
  // invalid audio/video clips which don't have deterministic results.
  bool Start(const base::FilePath& file_path);
  bool Start(const base::FilePath& file_path, Decryptor* decryptor);

  void Play();
  void Pause();
  bool Seek(base::TimeDelta seek_time);
  void Stop();
  bool WaitUntilCurrentTimeIsAfter(const base::TimeDelta& wait_time);
  scoped_ptr<FilterCollection> CreateFilterCollection(
      const base::FilePath& file_path, Decryptor* decryptor);

  // Returns the MD5 hash of all video frames seen.  Should only be called once
  // after playback completes.  First time hashes should be generated with
  // --video-threads=1 to ensure correctness.  Pipeline must have been started
  // with hashing enabled.
  std::string GetVideoHash();

  // Returns the hash of all audio frames seen.  Should only be called once
  // after playback completes.  Pipeline must have been started with hashing
  // enabled.
  std::string GetAudioHash();

  // Returns the time taken to render the complete audio file.
  // Pipeline must have been started with clockless playback enabled.
  base::TimeDelta GetAudioTime();

 protected:
  base::MessageLoop message_loop_;
  base::MD5Context md5_context_;
  bool hashing_enabled_;
  bool clockless_playback_;
  scoped_ptr<Demuxer> demuxer_;
  scoped_ptr<DataSource> data_source_;
  scoped_ptr<Pipeline> pipeline_;
  scoped_refptr<NullAudioSink> audio_sink_;
  scoped_refptr<ClocklessAudioSink> clockless_audio_sink_;
  bool ended_;
  PipelineStatus pipeline_status_;
  Demuxer::NeedKeyCB need_key_cb_;
  VideoFrame::Format last_video_frame_format_;
  DummyTickClock dummy_clock_;
  AudioHardwareConfig hardware_config_;
  PipelineMetadata metadata_;

  void OnStatusCallbackChecked(PipelineStatus expected_status,
                               PipelineStatus status);
  void OnStatusCallback(PipelineStatus status);
  PipelineStatusCB QuitOnStatusCB(PipelineStatus expected_status);
  void DemuxerNeedKeyCB(const std::string& type,
                        const std::vector<uint8>& init_data);
  void set_need_key_cb(const Demuxer::NeedKeyCB& need_key_cb) {
    need_key_cb_ = need_key_cb;
  }

  void OnEnded();
  void OnError(PipelineStatus status);
  void QuitAfterCurrentTimeTask(const base::TimeDelta& quit_time);
  scoped_ptr<FilterCollection> CreateFilterCollection(
      scoped_ptr<Demuxer> demuxer, Decryptor* decryptor);

  void SetDecryptor(Decryptor* decryptor,
                    const DecryptorReadyCB& decryptor_ready_cb);
  void OnVideoRendererPaint(const scoped_refptr<VideoFrame>& frame);

  MOCK_METHOD1(OnMetadata, void(PipelineMetadata));
  MOCK_METHOD0(OnPrerollCompleted, void());
};

}  // namespace media

#endif  // MEDIA_FILTERS_PIPELINE_INTEGRATION_TEST_BASE_H_
