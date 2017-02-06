// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_source_player.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "media/base/android/audio_decoder_job.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/android/media_url_interceptor.h"
#include "media/base/android/sdk_media_codec_bridge.h"
#include "media/base/android/video_decoder_job.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/timestamp_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gl/android/surface_texture.h"

namespace media {

const base::TimeDelta kDefaultDuration =
    base::TimeDelta::FromMilliseconds(10000);

// TODO(wolenetz/qinmin): Simplify tests with more effective mock usage, and
// fix flaky pointer-based MDJ inequality testing. See http://crbug.com/327839.

// Mock of MediaPlayerManager for testing purpose.
class MockMediaPlayerManager : public MediaPlayerManager {
 public:
  explicit MockMediaPlayerManager(base::MessageLoop* message_loop)
      : message_loop_(message_loop),
        playback_completed_(false),
        num_metadata_changes_(0),
        timestamp_updated_(false),
        allow_play_(true) {}
  ~MockMediaPlayerManager() override {}

  // MediaPlayerManager implementation.
  MediaResourceGetter* GetMediaResourceGetter() override { return NULL; }
  MediaUrlInterceptor* GetMediaUrlInterceptor() override { return NULL; }
  void OnTimeUpdate(int player_id,
                    base::TimeDelta current_time,
                    base::TimeTicks current_time_ticks) override {
    timestamp_updated_ = true;
  }
  void OnMediaMetadataChanged(int player_id,
                              base::TimeDelta duration,
                              int width,
                              int height,
                              bool success) override {
    num_metadata_changes_++;
  }
  void OnPlaybackComplete(int player_id) override {
    playback_completed_ = true;
    if (message_loop_->is_running())
      message_loop_->QuitWhenIdle();
  }
  void OnMediaInterrupted(int player_id) override {}
  void OnBufferingUpdate(int player_id, int percentage) override {}
  void OnSeekComplete(int player_id,
                      const base::TimeDelta& current_time) override {}
  void OnError(int player_id, int error) override {}
  void OnVideoSizeChanged(int player_id, int width, int height) override {}
  void OnWaitingForDecryptionKey(int player_id) override {}
  MediaPlayerAndroid* GetFullscreenPlayer() override { return NULL; }
  MediaPlayerAndroid* GetPlayer(int player_id) override { return NULL; }
  void OnDecorderResourcesReleased(int player_id) {}

  bool RequestPlay(int player_id,
                   base::TimeDelta duration,
                   bool has_audio) override {
    return allow_play_;
  }

  bool playback_completed() const {
    return playback_completed_;
  }

  int num_metadata_changes() const {
    return num_metadata_changes_;
  }

  bool timestamp_updated() const {
    return timestamp_updated_;
  }

  void ResetTimestampUpdated() {
    timestamp_updated_ = false;
  }

  void set_allow_play(bool value) {
    allow_play_ = value;
  }

 private:
  base::MessageLoop* message_loop_;
  bool playback_completed_;
  // The number of metadata changes reported by the player.
  int num_metadata_changes_;
  // Playback timestamp was updated.
  bool timestamp_updated_;
  // Whether the manager will allow players that request playing.
  bool allow_play_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaPlayerManager);
};

class MockDemuxerAndroid : public DemuxerAndroid {
 public:
  explicit MockDemuxerAndroid(base::MessageLoop* message_loop)
      : message_loop_(message_loop),
        num_data_requests_(0),
        num_seek_requests_(0),
        num_browser_seek_requests_(0) {}
  ~MockDemuxerAndroid() override {}

  void Initialize(DemuxerAndroidClient* client) override {}
  void RequestDemuxerData(DemuxerStream::Type type) override {
    num_data_requests_++;
    if (message_loop_->is_running())
      message_loop_->QuitWhenIdle();
  }
  void RequestDemuxerSeek(const base::TimeDelta& time_to_seek,
                          bool is_browser_seek) override {
    num_seek_requests_++;
    if (is_browser_seek)
      num_browser_seek_requests_++;
  }

  int num_data_requests() const { return num_data_requests_; }
  int num_seek_requests() const { return num_seek_requests_; }
  int num_browser_seek_requests() const { return num_browser_seek_requests_; }

 private:
  base::MessageLoop* message_loop_;

  // The number of encoded data requests this object has seen.
  int num_data_requests_;

  // The number of regular and browser seek requests this object has seen.
  int num_seek_requests_;

  // The number of browser seek requests this object has seen.
  int num_browser_seek_requests_;

  DISALLOW_COPY_AND_ASSIGN(MockDemuxerAndroid);
};

class MediaSourcePlayerTest : public testing::Test {
 public:
  MediaSourcePlayerTest()
      : manager_(&message_loop_),
        demuxer_(new MockDemuxerAndroid(&message_loop_)),
        player_(0,
                &manager_,
                base::Bind(&MockMediaPlayerManager::OnDecorderResourcesReleased,
                           base::Unretained(&manager_)),
                base::WrapUnique(demuxer_),
                GURL(),
                kDefaultMediaSessionId),
        decoder_callback_hook_executed_(false),
        surface_texture_a_is_next_(true) {}

  ~MediaSourcePlayerTest() override {}

 protected:
  // Get the decoder job from the MediaSourcePlayer. The return value must not
  // be NULL.
  MediaDecoderJob* GetMediaDecoderJob(bool is_audio) {
    if (is_audio) {
      return reinterpret_cast<MediaDecoderJob*>(
          player_.audio_decoder_job_.get());
    }
    return reinterpret_cast<MediaDecoderJob*>(
        player_.video_decoder_job_.get());
  }

  // Get the MediaCodecBridge from the decoder job. The return value could be
  // NULL if the decoder is not yet created.
  MediaCodecBridge* GetMediaCodecBridge(bool is_audio) {
    if (is_audio)
      return player_.audio_decoder_job_->media_codec_bridge_.get();
    return player_.video_decoder_job_->media_codec_bridge_.get();
  }

  // Get the per-job prerolling status from the MediaSourcePlayer's job matching
  // |is_audio|. Caller must guard against NPE if the player's job is NULL.
  bool IsPrerolling(bool is_audio) {
    return GetMediaDecoderJob(is_audio)->prerolling_;
  }

  // Get the preroll timestamp from the MediaSourcePlayer.
  base::TimeDelta GetPrerollTimestamp() {
    return player_.preroll_timestamp_;
  }

  // Simulate player has reached starvation timeout.
  void TriggerPlayerStarvation() {
    player_.decoder_starvation_callback_.Cancel();
    player_.OnDecoderStarved();
  }

  // Release() the player.
  void ReleasePlayer() {
    EXPECT_TRUE(player_.IsPlaying());
    player_.Release();
    EXPECT_FALSE(player_.IsPlaying());
  }

  // Upon the next successful decode callback, post a task to call Release()
  // on the |player_|. TEST_F's do not have access to the private player
  // members, hence this helper method.
  // Prevent usage creep of MSP::set_decode_callback_for_testing() by
  // only using it for the ReleaseWithOnPrefetchDoneAlreadyPosted test.
  void OnNextTestDecodeCallbackPostTaskToReleasePlayer() {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    player_.set_decode_callback_for_testing(media::BindToCurrentLoop(
      base::Bind(
          &MediaSourcePlayerTest::ReleaseWithPendingPrefetchDoneVerification,
          base::Unretained(this))));
  }

  // Asynch test callback posted upon decode completion to verify that a pending
  // prefetch done event is not cleared across |player_|'s Release(). This helps
  // ensure the ReleaseWithOnPrefetchDoneAlreadyPosted test scenario is met.
  void ReleaseWithPendingPrefetchDoneVerification() {
    EXPECT_TRUE(player_.IsEventPending(player_.PREFETCH_DONE_EVENT_PENDING));
    ReleasePlayer();
    EXPECT_TRUE(player_.IsEventPending(player_.PREFETCH_DONE_EVENT_PENDING));
    EXPECT_FALSE(decoder_callback_hook_executed_);
    EXPECT_FALSE(GetMediaCodecBridge(true));
    decoder_callback_hook_executed_ = true;
  }

  DemuxerConfigs CreateAudioDemuxerConfigs(AudioCodec audio_codec,
                                           bool use_low_sample_rate) {
    DemuxerConfigs configs;
    configs.audio_codec = audio_codec;
    configs.audio_channels = 2;
    configs.is_audio_encrypted = false;
    configs.duration = kDefaultDuration;

    if (audio_codec == kCodecVorbis) {
      configs.audio_sampling_rate = use_low_sample_rate ? 11025 : 44100;
      scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(
          "vorbis-extradata");
      configs.audio_extra_data = std::vector<uint8_t>(
          buffer->data(), buffer->data() + buffer->data_size());
      return configs;
    }

    // Other codecs are not yet supported by this helper.
    EXPECT_EQ(audio_codec, kCodecAAC);

    configs.audio_sampling_rate = 48000;
    uint8_t aac_extra_data[] = {0x13, 0x10};
    configs.audio_extra_data =
        std::vector<uint8_t>(aac_extra_data, aac_extra_data + 2);
    return configs;
  }

  DemuxerConfigs CreateVideoDemuxerConfigs(bool use_larger_size) {
    DemuxerConfigs configs;
    configs.video_codec = kCodecVP8;
    configs.video_size =
        use_larger_size ? gfx::Size(640, 240) : gfx::Size(320, 240);
    configs.is_video_encrypted = false;
    configs.duration = kDefaultDuration;
    return configs;
  }

  DemuxerConfigs CreateAudioVideoDemuxerConfigs() {
    DemuxerConfigs configs = CreateAudioDemuxerConfigs(kCodecVorbis, false);
    configs.video_codec = kCodecVP8;
    configs.video_size = gfx::Size(320, 240);
    configs.is_video_encrypted = false;
    return configs;
  }

  DemuxerConfigs CreateDemuxerConfigs(bool have_audio, bool have_video) {
    DCHECK(have_audio || have_video);

    if (have_audio && !have_video)
      return CreateAudioDemuxerConfigs(kCodecVorbis, false);

    if (have_video && !have_audio)
      return CreateVideoDemuxerConfigs(false);

    return CreateAudioVideoDemuxerConfigs();
  }

  // Starts an audio decoder job.
  void StartAudioDecoderJob() {
    Start(CreateAudioDemuxerConfigs(kCodecVorbis, false));
  }

  // Starts a video decoder job.
  void StartVideoDecoderJob() {
    Start(CreateVideoDemuxerConfigs(false));
  }

  // Starts decoding the data.
  void Start(const DemuxerConfigs& configs) {
    EXPECT_EQ(demuxer_->num_data_requests(), 0);
    player_.OnDemuxerConfigsAvailable(configs);
    player_.Start();

    EXPECT_TRUE(player_.IsPlaying());
    int expected_num_requests = (player_.HasAudio() ? 1 : 0) +
        (player_.HasVideo() ? 1 : 0);
    EXPECT_EQ(expected_num_requests, demuxer_->num_data_requests());
  }

  // Resumes decoding the data. Verifies player behavior relative to
  // |expect_player_requests_audio_data| and
  // |expect_player_requests_video_data|.
  void Resume(bool expect_player_requests_audio_data,
              bool expect_player_requests_video_data) {
    EXPECT_FALSE(player_.IsPlaying());
    EXPECT_TRUE(player_.HasVideo() || player_.HasAudio());
    int original_num_data_requests = demuxer_->num_data_requests();
    int expected_request_delta =
        (expect_player_requests_audio_data ? 1 : 0) +
        (expect_player_requests_video_data ? 1 : 0);

    player_.Start();

    EXPECT_TRUE(player_.IsPlaying());
    EXPECT_EQ(original_num_data_requests + expected_request_delta,
              demuxer_->num_data_requests());
  }

  // Keeps decoding audio data until the decoder starts to output samples.
  // Gives up if no audio output after decoding 10 frames.
  void DecodeAudioDataUntilOutputBecomesAvailable() {
    EXPECT_TRUE(player_.IsPlaying());
    base::TimeDelta current_time = player_.GetCurrentTime();
    base::TimeDelta start_timestamp = current_time;
    for (int i = 0; i < 10; ++i) {
      manager_.ResetTimestampUpdated();
      player_.OnDemuxerDataAvailable(
          CreateReadFromDemuxerAckForAudio(i > 3 ? 3 : i));
      WaitForAudioDecodeDone();
      base::TimeDelta new_current_time = player_.GetCurrentTime();
      EXPECT_LE(current_time.InMilliseconds(),
                new_current_time.InMilliseconds());
      current_time = new_current_time;
      if (manager_.timestamp_updated()) {
        // TODO(qinmin): the current time is from the decoder thread and it does
        // not take the delay from posting the task into consideration.
        // http://crbug.com/421616.
        EXPECT_LE(start_timestamp.InMillisecondsF(),
                  new_current_time.InMillisecondsF());
        return;
      }
    }
    EXPECT_TRUE(false);
  }

  AccessUnit CreateAccessUnitWithData(bool is_audio, int audio_packet_id,
                                      bool use_large_size_video) {
    AccessUnit unit;

    unit.status = DemuxerStream::kOk;
    scoped_refptr<DecoderBuffer> buffer;
    if (is_audio) {
      buffer = ReadTestDataFile(
          base::StringPrintf("vorbis-packet-%d", audio_packet_id));
    } else {
      buffer = ReadTestDataFile(
          use_large_size_video ? "vp8-I-frame-640x240" : "vp8-I-frame-320x240");
    }
    unit.data = std::vector<uint8_t>(buffer->data(),
                                     buffer->data() + buffer->data_size());

    if (is_audio) {
      // Vorbis needs 4 extra bytes padding on Android to decode properly. Check
      // NuMediaExtractor.cpp in Android source code.
      uint8_t padding[4] = {0xff, 0xff, 0xff, 0xff};
      unit.data.insert(unit.data.end(), padding, padding + 4);
    }

    return unit;
  }

  DemuxerData CreateReadFromDemuxerAckForAudio(int packet_id) {
    DemuxerData data;
    data.type = DemuxerStream::AUDIO;
    data.access_units.resize(1);
    data.access_units[0] = CreateAccessUnitWithData(true, packet_id, false);

    return data;
  }

  DemuxerData CreateReadFromDemuxerAckForVideo(bool use_large_size) {
    DemuxerData data;
    data.type = DemuxerStream::VIDEO;
    data.access_units.resize(1);
    data.access_units[0] = CreateAccessUnitWithData(false, 0, use_large_size);
    return data;
  }

  DemuxerData CreateEOSAck(bool is_audio) {
    DemuxerData data;
    data.type = is_audio ? DemuxerStream::AUDIO : DemuxerStream::VIDEO;
    data.access_units.resize(1);
    data.access_units[0].status = DemuxerStream::kOk;
    data.access_units[0].is_end_of_stream = true;
    return data;
  }

  DemuxerData CreateAbortedAck(bool is_audio) {
    DemuxerData data;
    data.type = is_audio ? DemuxerStream::AUDIO : DemuxerStream::VIDEO;
    data.access_units.resize(1);
    data.access_units[0].status = DemuxerStream::kAborted;
    return data;
  }

  bool HasData(bool is_audio) {
    return GetMediaDecoderJob(is_audio)->HasData();
  }

  // Helper method for use at test start. It starts an audio decoder job and
  // immediately feeds it some data to decode. Then, without letting the decoder
  // job complete a decode cycle, it also starts player SeekTo(). Upon return,
  // the player should not yet have sent the DemuxerSeek IPC request, though
  // seek event should be pending. The audio decoder job will also still be
  // decoding.
  void StartAudioDecoderJobAndSeekToWhileDecoding(
      const base::TimeDelta& seek_time) {
    EXPECT_FALSE(GetMediaCodecBridge(true));
    EXPECT_FALSE(player_.IsPlaying());
    EXPECT_EQ(0, demuxer_->num_data_requests());
    EXPECT_EQ(0.0, GetPrerollTimestamp().InMillisecondsF());
    EXPECT_EQ(player_.GetCurrentTime(), GetPrerollTimestamp());
    StartAudioDecoderJob();
    EXPECT_FALSE(GetMediaDecoderJob(true)->is_decoding());
    player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
    EXPECT_EQ(2, demuxer_->num_data_requests());
    EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
    player_.SeekTo(seek_time);
    EXPECT_EQ(0.0, GetPrerollTimestamp().InMillisecondsF());
    EXPECT_EQ(0, demuxer_->num_seek_requests());
    player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
  }

  // Seek, including simulated receipt of |kAborted| read between SeekTo() and
  // OnDemuxerSeekDone(). Use this helper method only when the player already
  // has created the media codec bridge. Exactly one request for more data is
  // expected following the seek, so use this helper for players with only audio
  // or only video.
  void SeekPlayerWithAbort(bool is_audio, const base::TimeDelta& seek_time) {
    int original_num_seeks = demuxer_->num_seek_requests();
    int original_num_data_requests = demuxer_->num_data_requests();

    // Initiate a seek. Skip the round-trip of requesting seek from renderer.
    // Instead behave as if the renderer has asked us to seek.
    player_.SeekTo(seek_time);

    // Verify that the seek does not occur until previously outstanding data
    // request is satisfied.
    EXPECT_EQ(original_num_seeks, demuxer_->num_seek_requests());

    // Simulate seeking causes the demuxer to abort the outstanding read
    // caused by the seek.
    player_.OnDemuxerDataAvailable(CreateAbortedAck(is_audio));

    // Wait for the decode job to finish so we can process the seek request.
    WaitForDecodeDone(is_audio, !is_audio);

    // Verify that the seek is requested.
    EXPECT_EQ(original_num_seeks + 1, demuxer_->num_seek_requests());

    // Send back the seek done notification. This should trigger the player to
    // call OnReadFromDemuxer() again.
    EXPECT_EQ(original_num_data_requests, demuxer_->num_data_requests());
    player_.OnDemuxerSeekDone(kNoTimestamp());
    EXPECT_EQ(original_num_data_requests + 1, demuxer_->num_data_requests());

    // No other seek should have been requested.
    EXPECT_EQ(original_num_seeks + 1, demuxer_->num_seek_requests());
  }

  // Preroll the decoder job to |target_timestamp|. The first access unit
  // to decode will have a timestamp equal to |start_timestamp|.
  // |is_clock_manager| indicates whether the decoder serves as the clock
  // manager for the player.
  // TODO(qinmin): Add additional test cases for out-of-order decodes.
  // See http://crbug.com/331421.
  void PrerollDecoderToTime(bool is_audio,
                            const base::TimeDelta& start_timestamp,
                            const base::TimeDelta& target_timestamp,
                            bool is_clock_manager) {
    // For streams with both audio and video, it is possible that audio rolls
    // past the |target_timestamp|. As a result, the current time may be larger
    // than the |target_timestamp| for video as it may not be the clock manager.
    EXPECT_TRUE(!is_clock_manager ||
                target_timestamp == player_.GetCurrentTime());
    // |start_timestamp| must be smaller than |target_timestamp|.
    EXPECT_LE(start_timestamp, target_timestamp);
    DemuxerData data = is_audio ? CreateReadFromDemuxerAckForAudio(1) :
        CreateReadFromDemuxerAckForVideo(false);
    int current_timestamp = start_timestamp.InMilliseconds();

    // Send some data with access unit timestamps before the |target_timestamp|,
    // and continue sending the data until preroll finishes.
    // This simulates the common condition that AUs received after browser
    // seek begin with timestamps before the seek target, and don't
    // immediately complete preroll.
    while (IsPrerolling(is_audio)) {
      data.access_units[0].timestamp =
          base::TimeDelta::FromMilliseconds(current_timestamp);
      player_.OnDemuxerDataAvailable(data);
      EXPECT_TRUE(GetMediaDecoderJob(is_audio)->is_decoding());
      EXPECT_TRUE(GetMediaCodecBridge(is_audio));
      EXPECT_TRUE(!is_clock_manager ||
                  target_timestamp == player_.GetCurrentTime());
      current_timestamp += 30;
      WaitForDecodeDone(is_audio, !is_audio);
    }
    EXPECT_LE(target_timestamp, player_.GetCurrentTime());
  }

  DemuxerData CreateReadFromDemuxerAckWithConfigChanged(
      bool is_audio,
      int config_unit_index,
      const DemuxerConfigs& configs) {
    DemuxerData data;
    data.type = is_audio ? DemuxerStream::AUDIO : DemuxerStream::VIDEO;
    data.access_units.resize(config_unit_index + 1);

    for (int i = 0; i < config_unit_index; ++i)
      data.access_units[i] = CreateAccessUnitWithData(is_audio, i, false);

    data.access_units[config_unit_index].status = DemuxerStream::kConfigChanged;
    data.demuxer_configs.resize(1);
    data.demuxer_configs[0] = configs;
    return data;
  }

  // Valid only for video-only player tests. If |trigger_with_release_start| is
  // true, triggers the browser seek with a Release() + video data received +
  // Start() with a new surface. If false, triggers the browser seek by
  // setting a new video surface after beginning decode of received video data.
  // Such data receipt causes possibility that an I-frame is not next, and
  // browser seek results once decode completes and surface change processing
  // begins.
  void BrowserSeekPlayer(bool trigger_with_release_start) {
    int expected_num_data_requests = demuxer_->num_data_requests() + 2;
    int expected_num_seek_requests = demuxer_->num_seek_requests();
    int expected_num_browser_seek_requests =
        demuxer_->num_browser_seek_requests();

    CreateNextTextureAndSetVideoSurface();
    StartVideoDecoderJob();
    if (trigger_with_release_start) {
      // Consume the first frame, so that the next VideoDecoderJob will not
      // inherit the I-frame from the previous decoder.
      player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
      ReleasePlayer();
      WaitForVideoDecodeDone();

      // Simulate demuxer's response to the video data request. The data will be
      // passed to the next MediaCodecBridge.
      player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
      EXPECT_FALSE(GetMediaCodecBridge(false));
      EXPECT_FALSE(player_.IsPlaying());
      EXPECT_EQ(expected_num_seek_requests, demuxer_->num_seek_requests());

      CreateNextTextureAndSetVideoSurface();
      Resume(false, false);
      EXPECT_FALSE(GetMediaCodecBridge(false));

      // Run the message loop so that prefetch will complete.
      while (expected_num_seek_requests == demuxer_->num_seek_requests())
        message_loop_.RunUntilIdle();
    } else {
      // Simulate demuxer's response to the video data request.
      player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));

      // While the decoder is decoding, trigger a browser seek by changing
      // surface. Demuxer does not know of browser seek in advance, so no
      // |kAborted| data is required (though |kAborted| can certainly occur for
      // any pending read in reality due to renderer preparing for a regular
      // seek).
      CreateNextTextureAndSetVideoSurface();

      // Browser seek should not begin until decoding has completed.
      EXPECT_TRUE(GetMediaCodecBridge(false));
      EXPECT_EQ(expected_num_seek_requests, demuxer_->num_seek_requests());

      // Wait for the media codec bridge to finish decoding and be reset pending
      // the browser seek.
      WaitForVideoDecodeDone();
      player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
    }

    // Only one browser seek should have been initiated, and no further data
    // should have been requested.
    expected_num_seek_requests++;
    expected_num_browser_seek_requests++;
    EXPECT_EQ(expected_num_seek_requests, demuxer_->num_seek_requests());
    EXPECT_EQ(expected_num_browser_seek_requests,
              demuxer_->num_browser_seek_requests());
    EXPECT_EQ(expected_num_data_requests, demuxer_->num_data_requests());
  }

  // Creates a new media codec bridge and feeds it data ending with a
  // |kConfigChanged| access unit. If |config_unit_in_prefetch| is true, sends
  // feeds the config change AU in response to the job's first read request
  // (prefetch). If false, regular data is fed and decoded prior to feeding the
  // config change AU in response to the second data request (after prefetch
  // completed). |config_unit_index| controls which access unit is
  // |kConfigChanged|. If |enable_adaptive_playback| is true, config change will
  // not cause the decoder to recreate the media codec bridge. Otherwise, the
  // decoder has to drain all its data before recreating the new codec.
  void SendConfigChangeToDecoder(bool is_audio,
                                 bool config_unit_in_prefetch,
                                 int config_unit_index,
                                 bool enable_adaptive_playback) {
    EXPECT_FALSE(GetMediaCodecBridge(is_audio));
    if (is_audio) {
      StartAudioDecoderJob();
    } else {
      CreateNextTextureAndSetVideoSurface();
      StartVideoDecoderJob();
    }

    int expected_num_data_requests = demuxer_->num_data_requests();
    // Feed and decode a standalone access unit so the player exits prefetch.
    if (!config_unit_in_prefetch) {
      if (is_audio) {
        player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
      } else {
        player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
        EnableAdaptiveVideoPlayback(enable_adaptive_playback);
      }

      WaitForDecodeDone(is_audio, !is_audio);

      // We should have completed the prefetch phase at this point.
      expected_num_data_requests++;
      EXPECT_EQ(expected_num_data_requests, demuxer_->num_data_requests());
    }

    DemuxerConfigs configs = is_audio ?
        CreateAudioDemuxerConfigs(kCodecVorbis, true) :
        CreateVideoDemuxerConfigs(true);
    // Feed and decode access units with data for any units prior to
    // |config_unit_index|, and a |kConfigChanged| unit at that index.
    // Player should prepare to reconfigure the decoder job, and should request
    // new demuxer configs.
    player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckWithConfigChanged(
        is_audio, config_unit_index, configs));

    expected_num_data_requests++;
    EXPECT_EQ(expected_num_data_requests, demuxer_->num_data_requests());
    if (is_audio)
      player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
    else
      player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(true));

    // If the adaptive playback setting was not passed to the MediaCodecBridge
    // earlier, do it here.
    if (config_unit_in_prefetch && !is_audio)
      EnableAdaptiveVideoPlayback(enable_adaptive_playback);
  }

  // Send a config change to the decoder job and drain the decoder so that the
  // config change is processed.
  void StartConfigChange(bool is_audio,
                         bool config_unit_in_prefetch,
                         int config_unit_index,
                         bool enable_adaptive_playback) {
    SendConfigChangeToDecoder(is_audio, config_unit_in_prefetch,
                              config_unit_index, enable_adaptive_playback);

    EXPECT_EQ(!config_unit_in_prefetch && !enable_adaptive_playback &&
              config_unit_index == 0, IsDrainingDecoder(is_audio));
    int expected_num_data_requests = demuxer_->num_data_requests();
    // Run until decoder starts to request new data.
    while (demuxer_->num_data_requests() == expected_num_data_requests)
      message_loop_.RunUntilIdle();
    EXPECT_FALSE(IsDrainingDecoder(is_audio));
  }

  void EnableAdaptiveVideoPlayback(bool enable) {
    EXPECT_TRUE(GetMediaCodecBridge(false));
    static_cast<VideoCodecBridge*>(GetMediaCodecBridge(false))->
        set_adaptive_playback_supported_for_testing(
            enable ? 1 : 0);
  }

  void CreateNextTextureAndSetVideoSurface() {
    gl::SurfaceTexture* surface_texture;
    if (surface_texture_a_is_next_) {
      surface_texture_a_ = gl::SurfaceTexture::Create(next_texture_id_++);
      surface_texture = surface_texture_a_.get();
    } else {
      surface_texture_b_ = gl::SurfaceTexture::Create(next_texture_id_++);
      surface_texture = surface_texture_b_.get();
    }

    surface_texture_a_is_next_ = !surface_texture_a_is_next_;
    gl::ScopedJavaSurface surface = gl::ScopedJavaSurface(surface_texture);
    player_.SetVideoSurface(std::move(surface));
  }

  // Wait for one or both of the jobs to complete decoding. Media codec bridges
  // are assumed to exist for any stream whose decode completion is awaited.
  void WaitForDecodeDone(bool wait_for_audio, bool wait_for_video) {
    DCHECK(wait_for_audio || wait_for_video);
    while ((wait_for_audio && GetMediaCodecBridge(true) &&
               GetMediaDecoderJob(true)->HasData() &&
               GetMediaDecoderJob(true)->is_decoding()) ||
           (wait_for_video && GetMediaCodecBridge(false) &&
               GetMediaDecoderJob(false)->HasData() &&
               GetMediaDecoderJob(false)->is_decoding())) {
      message_loop_.RunUntilIdle();
    }
  }

  void WaitForAudioDecodeDone() {
    WaitForDecodeDone(true, false);
  }

  void WaitForVideoDecodeDone() {
    WaitForDecodeDone(false, true);
  }

  void WaitForAudioVideoDecodeDone() {
    WaitForDecodeDone(true, true);
  }

  // If |send_eos| is true, generates EOS for the stream corresponding to
  // |eos_for_audio|. Verifies that playback completes and no further data
  // is requested.
  // If |send_eos| is false, then it is assumed that caller previously arranged
  // for player to receive EOS for each stream, but the player has not yet
  // decoded all of them. In this case, |eos_for_audio| is ignored.
  void VerifyPlaybackCompletesOnEOSDecode(bool send_eos, bool eos_for_audio) {
    int original_num_data_requests = demuxer_->num_data_requests();
    if (send_eos)
      player_.OnDemuxerDataAvailable(CreateEOSAck(eos_for_audio));
    EXPECT_FALSE(manager_.playback_completed());
    message_loop_.Run();
    EXPECT_TRUE(manager_.playback_completed());
    EXPECT_EQ(original_num_data_requests, demuxer_->num_data_requests());
  }

  void VerifyCompletedPlaybackResumesOnSeekPlusStart(bool have_audio,
                                                     bool have_video) {
    DCHECK(have_audio || have_video);

    EXPECT_TRUE(manager_.playback_completed());

    player_.SeekTo(base::TimeDelta());
    player_.OnDemuxerSeekDone(kNoTimestamp());
    Resume(have_audio, have_video);
  }

  // Starts the appropriate decoder jobs according to |have_audio| and
  // |have_video|. Then starts seek during decode of EOS or non-EOS according to
  // |eos_audio| and |eos_video|. Simulates seek completion and verifies that
  // playback never completed. |eos_{audio,video}| is ignored if the
  // corresponding |have_{audio,video}| is false.
  void VerifySeekDuringEOSDecodePreventsPlaybackCompletion(bool have_audio,
                                                           bool have_video,
                                                           bool eos_audio,
                                                           bool eos_video) {
    DCHECK(have_audio || have_video);

    if (have_video)
      CreateNextTextureAndSetVideoSurface();

    Start(CreateDemuxerConfigs(have_audio, have_video));

    if (have_audio)
      player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));

    if (have_video)
      player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));

    // Run until more data is requested a number of times equal to the number of
    // media types configured. Since prefetching may be in progress, we cannot
    // reliably expect Run() to complete until we have sent demuxer data for all
    // configured media types, above.
    WaitForDecodeDone(have_audio, have_video);

    // Simulate seek while decoding EOS or non-EOS for the appropriate
    // stream(s).
    if (have_audio) {
      if (eos_audio)
        player_.OnDemuxerDataAvailable(CreateEOSAck(true));
      else
        player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(1));
    }

    if (have_video) {
      if (eos_video)
        player_.OnDemuxerDataAvailable(CreateEOSAck(false));
      else
        player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
    }

    player_.SeekTo(base::TimeDelta());
    EXPECT_EQ(0, demuxer_->num_seek_requests());
    WaitForDecodeDone(have_audio, have_video);
    EXPECT_EQ(1, demuxer_->num_seek_requests());

    player_.OnDemuxerSeekDone(kNoTimestamp());
    EXPECT_FALSE(manager_.playback_completed());
  }

  base::TimeTicks StartTimeTicks() {
    return player_.start_time_ticks_;
  }

  bool IsRequestingDemuxerData(bool is_audio) {
    return GetMediaDecoderJob(is_audio)->is_requesting_demuxer_data_;
  }

  bool IsDrainingDecoder(bool is_audio) {
    return GetMediaDecoderJob(is_audio)->drain_decoder_;
  }

 protected:
  base::MessageLoop message_loop_;
  MockMediaPlayerManager manager_;
  MockDemuxerAndroid* demuxer_;  // Owned by |player_|.
  MediaSourcePlayer player_;

  // Track whether a possibly async decoder callback test hook has run.
  bool decoder_callback_hook_executed_;

  // We need to keep the surface texture while the decoder is actively decoding.
  // Otherwise, it may trigger unexpected crashes on some devices. To switch
  // surfaces, tests need to create a new surface texture without releasing
  // their previous one. In CreateNextTextureAndSetVideoSurface(), we toggle
  // between two surface textures, only replacing the N-2 texture. Assumption is
  // that no more than N-1 texture is in use by decoder when
  // CreateNextTextureAndSetVideoSurface() is called.
  scoped_refptr<gl::SurfaceTexture> surface_texture_a_;
  scoped_refptr<gl::SurfaceTexture> surface_texture_b_;
  bool surface_texture_a_is_next_;
  int next_texture_id_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourcePlayerTest);
};

TEST_F(MediaSourcePlayerTest, StartAudioDecoderWithValidConfig) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test audio codec will be created when valid configs and data are passed to
  // the audio decoder job.
  StartAudioDecoderJob();
  EXPECT_EQ(0, demuxer_->num_seek_requests());
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
  EXPECT_TRUE(GetMediaCodecBridge(true));
}

TEST_F(MediaSourcePlayerTest, StartAudioDecoderWithInvalidConfig) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test audio decoder job will not be created when failed to start the codec.
  DemuxerConfigs configs = CreateAudioDemuxerConfigs(kCodecVorbis, false);
  // Replace with invalid |audio_extra_data|
  configs.audio_extra_data.clear();
  uint8_t invalid_codec_data[] = {0x00, 0xff, 0xff, 0xff, 0xff};
  configs.audio_extra_data.insert(configs.audio_extra_data.begin(),
                                 invalid_codec_data, invalid_codec_data + 4);
  Start(configs);

  // Decoder is not created after data is received.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
  EXPECT_FALSE(GetMediaCodecBridge(true));
}

TEST_F(MediaSourcePlayerTest, StartVideoCodecWithValidSurface) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test video codec will not be created until data is received.
  StartVideoDecoderJob();

  // Set both an initial and a later video surface without receiving any
  // demuxed data yet.
  CreateNextTextureAndSetVideoSurface();
  EXPECT_FALSE(GetMediaCodecBridge(false));
  CreateNextTextureAndSetVideoSurface();
  EXPECT_FALSE(GetMediaCodecBridge(false));

  // No seeks, even on setting surface, should have occurred. (Browser seeks can
  // occur on setting surface, but only after previously receiving video data.)
  EXPECT_EQ(0, demuxer_->num_seek_requests());

  // Send the first input chunk and verify that decoder will be created.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  EXPECT_TRUE(GetMediaCodecBridge(false));
  WaitForVideoDecodeDone();
}

TEST_F(MediaSourcePlayerTest, StartVideoCodecWithInvalidSurface) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test video codec will not be created when surface is invalid.
  scoped_refptr<gl::SurfaceTexture> surface_texture(
      gl::SurfaceTexture::Create(0));
  gl::ScopedJavaSurface surface(surface_texture.get());
  StartVideoDecoderJob();

  // Release the surface texture.
  surface_texture = NULL;
  player_.SetVideoSurface(std::move(surface));

  // Player should not seek the demuxer on setting initial surface.
  EXPECT_EQ(0, demuxer_->num_seek_requests());
  EXPECT_EQ(1, demuxer_->num_data_requests());

  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  EXPECT_FALSE(GetMediaCodecBridge(false));
}

TEST_F(MediaSourcePlayerTest, ReadFromDemuxerAfterSeek) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decoder job will resend a ReadFromDemuxer request after seek.
  StartAudioDecoderJob();
  SeekPlayerWithAbort(true, base::TimeDelta());
}

TEST_F(MediaSourcePlayerTest, SetSurfaceWhileSeeking) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test SetVideoSurface() will not cause an extra seek while the player is
  // waiting for demuxer to indicate seek is done.
  player_.OnDemuxerConfigsAvailable(
      CreateVideoDemuxerConfigs(false));

  // Initiate a seek. Skip requesting element seek of renderer.
  // Instead behave as if the renderer has asked us to seek.
  player_.SeekTo(base::TimeDelta());
  EXPECT_EQ(1, demuxer_->num_seek_requests());

  CreateNextTextureAndSetVideoSurface();
  EXPECT_EQ(1, demuxer_->num_seek_requests());
  player_.Start();

  // Send the seek done notification. The player should start requesting data.
  player_.OnDemuxerSeekDone(kNoTimestamp());
  EXPECT_FALSE(GetMediaCodecBridge(false));
  EXPECT_EQ(1, demuxer_->num_data_requests());
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  EXPECT_TRUE(GetMediaCodecBridge(false));

  // Reconfirm exactly 1 seek request has been made of demuxer, and that it
  // was not a browser seek request.
  EXPECT_EQ(1, demuxer_->num_seek_requests());
  EXPECT_EQ(0, demuxer_->num_browser_seek_requests());
  WaitForVideoDecodeDone();
}

TEST_F(MediaSourcePlayerTest, ChangeMultipleSurfaceWhileDecoding) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test MediaSourcePlayer can switch multiple surfaces during decoding.
  CreateNextTextureAndSetVideoSurface();
  StartVideoDecoderJob();
  EXPECT_EQ(0, demuxer_->num_seek_requests());

  // Send the first input chunk.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));

  // While the decoder is decoding, change multiple surfaces. Pass an empty
  // surface first.
  gl::ScopedJavaSurface empty_surface;
  player_.SetVideoSurface(std::move(empty_surface));
  // Next, pass a new non-empty surface.
  CreateNextTextureAndSetVideoSurface();

  // Wait for the media codec bridge to finish decoding and be reset pending a
  // browser seek.
  WaitForVideoDecodeDone();
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));

  // Only one browser seek should have been initiated. No further data request
  // should have been processed on |message_loop_| before surface change event
  // became pending, above.
  EXPECT_EQ(1, demuxer_->num_browser_seek_requests());
  EXPECT_EQ(2, demuxer_->num_data_requests());

  // Simulate browser seek is done and confirm player requests more data for new
  // video codec.
  player_.OnDemuxerSeekDone(player_.GetCurrentTime());
  EXPECT_FALSE(GetMediaCodecBridge(false));
  EXPECT_EQ(3, demuxer_->num_data_requests());
  EXPECT_EQ(1, demuxer_->num_seek_requests());

  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  EXPECT_TRUE(GetMediaCodecBridge(false));
  WaitForVideoDecodeDone();
}

TEST_F(MediaSourcePlayerTest, SetEmptySurfaceAndStarveWhileDecoding) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test player pauses if an empty surface is passed.
  CreateNextTextureAndSetVideoSurface();
  StartVideoDecoderJob();
  EXPECT_EQ(1, demuxer_->num_data_requests());

  // Send the first input chunk.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));

  // While the decoder is decoding, pass an empty surface.
  gl::ScopedJavaSurface empty_surface;
  player_.SetVideoSurface(std::move(empty_surface));
  // Let the player starve. However, it should not issue any new data request in
  // this case.
  TriggerPlayerStarvation();
  // Wait for the media codec bridge to finish decoding and be reset.
  while (GetMediaDecoderJob(false)->is_decoding())
    message_loop_.RunUntilIdle();

  // No further seek or data requests should have been received since the
  // surface is empty.
  EXPECT_EQ(0, demuxer_->num_browser_seek_requests());
  EXPECT_EQ(2, demuxer_->num_data_requests());
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));

  // Playback resumes once a non-empty surface is passed.
  CreateNextTextureAndSetVideoSurface();
  EXPECT_EQ(0, demuxer_->num_browser_seek_requests());
  while(demuxer_->num_browser_seek_requests() != 1)
    message_loop_.RunUntilIdle();
  WaitForVideoDecodeDone();
}

TEST_F(MediaSourcePlayerTest, ReleaseVideoDecoderResourcesWhileDecoding) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that if video decoder is released while decoding, the resources will
  // not be immediately released.
  CreateNextTextureAndSetVideoSurface();
  StartVideoDecoderJob();
  ReleasePlayer();
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));

  // Recreate the video decoder.
  CreateNextTextureAndSetVideoSurface();
  player_.Start();
  while (!GetMediaDecoderJob(false)->is_decoding())
    message_loop_.RunUntilIdle();
  EXPECT_EQ(0, demuxer_->num_browser_seek_requests());
  ReleasePlayer();
  // Wait for the media codec bridge to finish decoding and be reset.
  while (GetMediaDecoderJob(false)->is_decoding())
    message_loop_.RunUntilIdle();
}

TEST_F(MediaSourcePlayerTest, AudioOnlyStartAfterSeekFinish) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test audio decoder job will not start until pending seek event is handled.
  DemuxerConfigs configs = CreateAudioDemuxerConfigs(kCodecVorbis, false);
  player_.OnDemuxerConfigsAvailable(configs);

  // Initiate a seek. Skip requesting element seek of renderer.
  // Instead behave as if the renderer has asked us to seek.
  player_.SeekTo(base::TimeDelta());
  EXPECT_EQ(1, demuxer_->num_seek_requests());

  player_.Start();
  EXPECT_EQ(0, demuxer_->num_data_requests());

  // Sending back the seek done notification.
  player_.OnDemuxerSeekDone(kNoTimestamp());
  EXPECT_FALSE(GetMediaCodecBridge(true));
  EXPECT_EQ(1, demuxer_->num_data_requests());

  // Reconfirm exactly 1 seek request has been made of demuxer.
  EXPECT_EQ(1, demuxer_->num_seek_requests());

  // Decoder is created after data is received.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
  EXPECT_TRUE(GetMediaCodecBridge(true));
}

TEST_F(MediaSourcePlayerTest, VideoOnlyStartAfterSeekFinish) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test video decoder job will not start until pending seek event is handled.
  CreateNextTextureAndSetVideoSurface();
  DemuxerConfigs configs = CreateVideoDemuxerConfigs(false);
  player_.OnDemuxerConfigsAvailable(configs);

  // Initiate a seek. Skip requesting element seek of renderer.
  // Instead behave as if the renderer has asked us to seek.
  player_.SeekTo(base::TimeDelta());
  EXPECT_EQ(1, demuxer_->num_seek_requests());

  player_.Start();
  EXPECT_EQ(0, demuxer_->num_data_requests());

  // Sending back the seek done notification.
  player_.OnDemuxerSeekDone(kNoTimestamp());
  EXPECT_FALSE(GetMediaCodecBridge(false));
  EXPECT_EQ(1, demuxer_->num_data_requests());

  // Reconfirm exactly 1 seek request has been made of demuxer.
  EXPECT_EQ(1, demuxer_->num_seek_requests());

  // Decoder is created after data is received.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  EXPECT_TRUE(GetMediaCodecBridge(false));
  WaitForVideoDecodeDone();
}

TEST_F(MediaSourcePlayerTest, StartImmediatelyAfterPause) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that if the decoding job is not fully stopped after Pause(),
  // calling Start() will be a noop.
  StartAudioDecoderJob();

  MediaDecoderJob* decoder_job = GetMediaDecoderJob(true);
  EXPECT_FALSE(GetMediaDecoderJob(true)->is_decoding());

  // Sending data to player.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
  EXPECT_EQ(2, demuxer_->num_data_requests());

  // Decoder job will not immediately stop after Pause() since it is
  // running on another thread.
  player_.Pause(true);
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());

  // Nothing happens when calling Start() again.
  player_.Start();
  // Verify that Start() will not destroy and recreate the media codec bridge.
  EXPECT_EQ(decoder_job, GetMediaDecoderJob(true));

  while (GetMediaDecoderJob(true)->is_decoding())
    message_loop_.RunUntilIdle();
  // The decoder job should finish and wait for data.
  EXPECT_EQ(2, demuxer_->num_data_requests());
  EXPECT_TRUE(IsRequestingDemuxerData(true));
}

TEST_F(MediaSourcePlayerTest, DecoderJobsCannotStartWithoutAudio) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that when Start() is called, video decoder job will wait for audio
  // decoder job before start decoding the data.
  CreateNextTextureAndSetVideoSurface();
  Start(CreateAudioVideoDemuxerConfigs());
  MediaDecoderJob* audio_decoder_job = GetMediaDecoderJob(true);
  MediaDecoderJob* video_decoder_job = GetMediaDecoderJob(false);

  EXPECT_FALSE(audio_decoder_job->is_decoding());
  EXPECT_FALSE(video_decoder_job->is_decoding());

  // Sending video data to player, video decoder should not start.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  EXPECT_FALSE(video_decoder_job->is_decoding());

  // Sending audio data to player, both decoders should start now.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
  EXPECT_TRUE(audio_decoder_job->is_decoding());
  EXPECT_TRUE(video_decoder_job->is_decoding());

  // No seeks should have occurred.
  EXPECT_EQ(0, demuxer_->num_seek_requests());
  WaitForVideoDecodeDone();
}

TEST_F(MediaSourcePlayerTest, StartTimeTicksResetAfterDecoderUnderruns) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test start time ticks will reset after decoder job underruns.
  StartAudioDecoderJob();

  DecodeAudioDataUntilOutputBecomesAvailable();

  // The decoder job should finish prerolling and start prefetching.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(3));
  base::TimeTicks previous = StartTimeTicks();

  // Let the decoder starve.
  TriggerPlayerStarvation();
  WaitForAudioDecodeDone();
  EXPECT_TRUE(StartTimeTicks() == previous);

  // Send new data to the decoder so it can finish prefetching. This should
  // reset the start time ticks.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(3));
  EXPECT_TRUE(StartTimeTicks() != previous);

  base::TimeTicks current = StartTimeTicks();
  EXPECT_LE(0, (current - previous).InMillisecondsF());
}

TEST_F(MediaSourcePlayerTest, V_SecondAccessUnitIsEOSAndResumePlayAfterSeek) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test MediaSourcePlayer can replay video after input EOS is reached.
  CreateNextTextureAndSetVideoSurface();
  StartVideoDecoderJob();

  // Send the first input chunk.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  WaitForVideoDecodeDone();

  VerifyPlaybackCompletesOnEOSDecode(true, false);
  VerifyCompletedPlaybackResumesOnSeekPlusStart(false, true);
}

TEST_F(MediaSourcePlayerTest, A_FirstAccessUnitIsEOSAndResumePlayAfterSeek) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decode of audio EOS buffer without any prior decode. See also
  // http://b/11696552.
  // Also tests that seeking+Start() after completing audio playback resumes
  // playback.
  Start(CreateAudioDemuxerConfigs(kCodecAAC, false));
  VerifyPlaybackCompletesOnEOSDecode(true, true);
  VerifyCompletedPlaybackResumesOnSeekPlusStart(true, false);
}

TEST_F(MediaSourcePlayerTest, V_FirstAccessUnitAfterSeekIsEOS) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test decode of video EOS buffer, just after seeking, without any prior
  // decode (other than the simulated |kAborted| resulting from the seek
  // process.)
  CreateNextTextureAndSetVideoSurface();
  StartVideoDecoderJob();
  SeekPlayerWithAbort(false, base::TimeDelta());
  VerifyPlaybackCompletesOnEOSDecode(true, false);
}

TEST_F(MediaSourcePlayerTest, A_FirstAccessUnitAfterSeekIsEOS) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decode of audio EOS buffer, just after seeking, without any prior
  // decode (other than the simulated |kAborted| resulting from the seek
  // process.) See also http://b/11696552.
  Start(CreateAudioDemuxerConfigs(kCodecAAC, false));
  SeekPlayerWithAbort(true, base::TimeDelta());
  VerifyPlaybackCompletesOnEOSDecode(true, true);
}

TEST_F(MediaSourcePlayerTest, AV_PlaybackCompletionAcrossConfigChange) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that if one stream (audio) has completed decode of EOS and the other
  // stream (video) processes config change, that subsequent video EOS completes
  // A/V playback.
  // Also tests that seeking+Start() after completing playback resumes playback.
  CreateNextTextureAndSetVideoSurface();
  Start(CreateAudioVideoDemuxerConfigs());

  player_.OnDemuxerDataAvailable(CreateEOSAck(true));  // Audio EOS
  DemuxerConfigs configs = CreateVideoDemuxerConfigs(true);
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckWithConfigChanged(
      false, 0, configs));  // Video |kConfigChanged| as first unit.

  WaitForAudioVideoDecodeDone();

  EXPECT_EQ(3, demuxer_->num_data_requests());

  // At no time after completing audio EOS decode, above, should the
  // audio decoder job resume decoding. Send and decode video EOS.
  VerifyPlaybackCompletesOnEOSDecode(true, false);
  VerifyCompletedPlaybackResumesOnSeekPlusStart(true, true);
}

TEST_F(MediaSourcePlayerTest, VA_PlaybackCompletionAcrossConfigChange) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that if one stream (video) has completed decode of EOS and the other
  // stream (audio) processes config change, that subsequent audio EOS completes
  // A/V playback.
  // Also tests that seeking+Start() after completing playback resumes playback.
  CreateNextTextureAndSetVideoSurface();
  Start(CreateAudioVideoDemuxerConfigs());

  player_.OnDemuxerDataAvailable(CreateEOSAck(false));  // Video EOS
  // Audio |kConfigChanged| as first unit.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckWithConfigChanged(
      true, 0, CreateAudioDemuxerConfigs(kCodecVorbis, false)));

  WaitForAudioVideoDecodeDone();

  EXPECT_EQ(3, demuxer_->num_data_requests());

  // At no time after completing video EOS decode, above, should the
  // video decoder job resume decoding. Send and decode audio EOS.
  VerifyPlaybackCompletesOnEOSDecode(true, true);
  VerifyCompletedPlaybackResumesOnSeekPlusStart(true, true);
}

TEST_F(MediaSourcePlayerTest, AV_NoPrefetchForFinishedVideoOnAudioStarvation) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that if one stream (video) has completed decode of EOS, prefetch
  // resulting from player starvation occurs only for the other stream (audio),
  // and responding to that prefetch with EOS completes A/V playback, even if
  // another starvation occurs during the latter EOS's decode.
  CreateNextTextureAndSetVideoSurface();
  Start(CreateAudioVideoDemuxerConfigs());

  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
  player_.OnDemuxerDataAvailable(CreateEOSAck(false));  // Video EOS

  // Wait until video EOS is processed and more data (assumed to be audio) is
  // requested.
  WaitForAudioVideoDecodeDone();
  EXPECT_EQ(3, demuxer_->num_data_requests());

  // Simulate decoder underrun to trigger prefetch while still decoding audio.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(1));
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding() &&
              !GetMediaDecoderJob(false)->is_decoding());
  TriggerPlayerStarvation();

  // Complete the audio decode that was in progress when simulated player
  // starvation was triggered.
  WaitForAudioDecodeDone();
  EXPECT_EQ(4, demuxer_->num_data_requests());
  player_.OnDemuxerDataAvailable(CreateEOSAck(true));  // Audio EOS
  EXPECT_FALSE(GetMediaDecoderJob(false)->is_decoding());
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());

  // Simulate another decoder underrun to trigger prefetch while decoding EOS.
  TriggerPlayerStarvation();
  VerifyPlaybackCompletesOnEOSDecode(false, true /* ignored */);
}

TEST_F(MediaSourcePlayerTest, V_StarvationDuringEOSDecode) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that video-only playback completes without further data requested when
  // starvation occurs during EOS decode.
  CreateNextTextureAndSetVideoSurface();
  StartVideoDecoderJob();
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  WaitForVideoDecodeDone();

  // Simulate decoder underrun to trigger prefetch while decoding EOS.
  player_.OnDemuxerDataAvailable(CreateEOSAck(false));  // Video EOS
  EXPECT_TRUE(GetMediaDecoderJob(false)->is_decoding());
  TriggerPlayerStarvation();
  VerifyPlaybackCompletesOnEOSDecode(false, false /* ignored */);
}

TEST_F(MediaSourcePlayerTest, A_StarvationDuringEOSDecode) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that audio-only playback completes without further data requested when
  // starvation occurs during EOS decode.
  StartAudioDecoderJob();
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
  WaitForAudioDecodeDone();

  // Simulate decoder underrun to trigger prefetch while decoding EOS.
  player_.OnDemuxerDataAvailable(CreateEOSAck(true));  // Audio EOS
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
  TriggerPlayerStarvation();
  VerifyPlaybackCompletesOnEOSDecode(false, true /* ignored */);
}

TEST_F(MediaSourcePlayerTest, AV_SeekDuringEOSDecodePreventsCompletion) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that seek supercedes audio+video playback completion on simultaneous
  // audio and video EOS decode, if SeekTo() occurs during these EOS decodes.
  VerifySeekDuringEOSDecodePreventsPlaybackCompletion(true, true, true, true);
}

TEST_F(MediaSourcePlayerTest, AV_SeekDuringAudioEOSDecodePreventsCompletion) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that seek supercedes audio+video playback completion on simultaneous
  // audio EOS and video non-EOS decode, if SeekTo() occurs during these
  // decodes.
  VerifySeekDuringEOSDecodePreventsPlaybackCompletion(true, true, true, false);
}

TEST_F(MediaSourcePlayerTest, AV_SeekDuringVideoEOSDecodePreventsCompletion) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that seek supercedes audio+video playback completion on simultaneous
  // audio non-EOS and video EOS decode, if SeekTo() occurs during these
  // decodes.
  VerifySeekDuringEOSDecodePreventsPlaybackCompletion(true, true, false, true);
}

TEST_F(MediaSourcePlayerTest, V_SeekDuringEOSDecodePreventsCompletion) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that seek supercedes video-only playback completion on EOS decode, if
  // SeekTo() occurs during EOS decode.
  VerifySeekDuringEOSDecodePreventsPlaybackCompletion(false, true, false, true);
}

TEST_F(MediaSourcePlayerTest, A_SeekDuringEOSDecodePreventsCompletion) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that seek supercedes audio-only playback completion on EOS decode, if
  // SeekTo() occurs during EOS decode.
  VerifySeekDuringEOSDecodePreventsPlaybackCompletion(true, false, true, false);
}

TEST_F(MediaSourcePlayerTest, NoRequestForDataAfterAbort) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that the decoder will not request new data after receiving an aborted
  // access unit.
  StartAudioDecoderJob();

  // Send an aborted access unit.
  player_.OnDemuxerDataAvailable(CreateAbortedAck(true));
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
  WaitForAudioDecodeDone();

  // No request will be sent for new data.
  EXPECT_EQ(1, demuxer_->num_data_requests());

  // No seek requests should have occurred.
  EXPECT_EQ(0, demuxer_->num_seek_requests());
}

TEST_F(MediaSourcePlayerTest, DemuxerDataArrivesAfterRelease) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that the decoder should not crash if demuxer data arrives after
  // Release().
  StartAudioDecoderJob();

  ReleasePlayer();
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));

  // The media codec bridge should have been released.
  EXPECT_FALSE(player_.IsPlaying());

  // No further data should have been requested.
  EXPECT_EQ(1, demuxer_->num_data_requests());

  // No seek requests should have occurred.
  EXPECT_EQ(0, demuxer_->num_seek_requests());
}

TEST_F(MediaSourcePlayerTest, BrowserSeek_RegularSeekPendsBrowserSeekDone) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that a browser seek, once started, delays a newly arrived regular
  // SeekTo() request's demuxer seek until the browser seek is done.
  BrowserSeekPlayer(false);

  // Simulate renderer requesting a regular seek while browser seek in progress.
  player_.SeekTo(base::TimeDelta());

  // Simulate browser seek is done. Confirm player requests the regular seek,
  // still has no video codec configured, and has not requested any
  // further data since the surface change event became pending in
  // BrowserSeekPlayer().
  EXPECT_EQ(1, demuxer_->num_seek_requests());
  player_.OnDemuxerSeekDone(base::TimeDelta());
  EXPECT_EQ(2, demuxer_->num_seek_requests());
  EXPECT_EQ(1, demuxer_->num_browser_seek_requests());

  // Simulate regular seek is done and confirm player requests more data for
  // new video codec.
  player_.OnDemuxerSeekDone(kNoTimestamp());
  EXPECT_FALSE(GetMediaCodecBridge(false));
  EXPECT_EQ(3, demuxer_->num_data_requests());
  EXPECT_EQ(2, demuxer_->num_seek_requests());
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  EXPECT_TRUE(GetMediaCodecBridge(false));
  WaitForVideoDecodeDone();
}

TEST_F(MediaSourcePlayerTest, BrowserSeek_InitialReleaseAndStart) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that no browser seek is requested if player Release() + Start() occurs
  // prior to receiving any data.
  CreateNextTextureAndSetVideoSurface();
  StartVideoDecoderJob();
  ReleasePlayer();

  // Pass a new non-empty surface.
  CreateNextTextureAndSetVideoSurface();

  player_.Start();

  // No data request is issued since there is still one pending.
  EXPECT_EQ(1, demuxer_->num_data_requests());
  EXPECT_FALSE(GetMediaCodecBridge(false));

  // No browser seek is needed.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  EXPECT_EQ(0, demuxer_->num_browser_seek_requests());
  EXPECT_EQ(2, demuxer_->num_data_requests());
  WaitForVideoDecodeDone();
}

TEST_F(MediaSourcePlayerTest, BrowserSeek_MidStreamReleaseAndStart) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that one browser seek is requested if player Release() + Start(), with
  // video data received between Release() and Start().
  BrowserSeekPlayer(true);

  // Simulate browser seek is done and confirm player requests more data.
  player_.OnDemuxerSeekDone(base::TimeDelta());
  EXPECT_EQ(3, demuxer_->num_data_requests());
  EXPECT_EQ(1, demuxer_->num_seek_requests());
}

TEST_F(MediaSourcePlayerTest, NoBrowserSeekWithKeyFrameInCache) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that browser seek is not needed if a key frame is found in data
  // cache.
  CreateNextTextureAndSetVideoSurface();
  StartVideoDecoderJob();
  DemuxerData data = CreateReadFromDemuxerAckForVideo(false);
  data.access_units[0].is_key_frame = true;

  // Simulate demuxer's response to the video data request.
  player_.OnDemuxerDataAvailable(data);

  // Trigger decoder recreation later by changing surfaces.
  CreateNextTextureAndSetVideoSurface();

  // Wait for the media codec bridge to finish decoding and be reset.
  WaitForVideoDecodeDone();
  EXPECT_FALSE(HasData(false));

  // Send a non key frame to decoder so that decoder can continue. This will
  // not trigger any browser seeks as the previous key frame is still in the
  // buffer.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  WaitForVideoDecodeDone();
  EXPECT_EQ(0, demuxer_->num_browser_seek_requests());
}

TEST_F(MediaSourcePlayerTest, PrerollAudioAfterSeek) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decoder job will preroll the media to the seek position.
  StartAudioDecoderJob();

  SeekPlayerWithAbort(true, base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(IsPrerolling(true));
  PrerollDecoderToTime(
      true, base::TimeDelta(), base::TimeDelta::FromMilliseconds(100), true);
}

TEST_F(MediaSourcePlayerTest, PrerollVideoAfterSeek) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test decoder job will preroll the media to the seek position.
  CreateNextTextureAndSetVideoSurface();
  StartVideoDecoderJob();

  SeekPlayerWithAbort(false, base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(IsPrerolling(false));
  PrerollDecoderToTime(
      false, base::TimeDelta(), base::TimeDelta::FromMilliseconds(100), true);
}

TEST_F(MediaSourcePlayerTest, SeekingAfterCompletingPrerollRestartsPreroll) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decoder job will begin prerolling upon seek, when it was not
  // prerolling prior to the seek.
  StartAudioDecoderJob();
  MediaDecoderJob* decoder_job = GetMediaDecoderJob(true);
  EXPECT_TRUE(IsPrerolling(true));

  // Complete the initial preroll by feeding data to the decoder.
  DecodeAudioDataUntilOutputBecomesAvailable();
  EXPECT_FALSE(IsPrerolling(true));

  SeekPlayerWithAbort(true, base::TimeDelta::FromMilliseconds(500));

  // Prerolling should have begun again.
  EXPECT_TRUE(IsPrerolling(true));
  EXPECT_EQ(500.0, GetPrerollTimestamp().InMillisecondsF());

  // Send data at and after the seek position. Prerolling should complete.
  for (int i = 0; i < 4; ++i) {
    DemuxerData data = CreateReadFromDemuxerAckForAudio(i);
    data.access_units[0].timestamp = base::TimeDelta::FromMilliseconds(
        500 + 30 * (i - 1));
    player_.OnDemuxerDataAvailable(data);
    EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
    WaitForAudioDecodeDone();
  }
  EXPECT_LT(500.0, player_.GetCurrentTime().InMillisecondsF());
  EXPECT_FALSE(IsPrerolling(true));

  // Throughout this test, we should have not re-created the media codec bridge,
  // so IsPrerolling() transition from false to true was not due to constructor
  // initialization. It was due to BeginPrerolling().
  EXPECT_EQ(decoder_job, GetMediaDecoderJob(true));
}

TEST_F(MediaSourcePlayerTest, PrerollContinuesAcrossReleaseAndStart) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decoder job will resume media prerolling if interrupted by Release()
  // and Start().
  StartAudioDecoderJob();

  base::TimeDelta target_timestamp = base::TimeDelta::FromMilliseconds(100);
  SeekPlayerWithAbort(true, target_timestamp);
  EXPECT_TRUE(IsPrerolling(true));
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());

  // Send some data before the seek position.
  // Test uses 'large' number of iterations because decoder job may not get
  // MEDIA_CODEC_OK output status until after a few dequeue output attempts.
  // This allows decoder status to stabilize prior to AU timestamp reaching
  // the preroll target.
  DemuxerData data;
  for (int i = 0; i < 10; ++i) {
    data = CreateReadFromDemuxerAckForAudio(3);
    data.access_units[0].timestamp = base::TimeDelta::FromMilliseconds(i * 10);
    if (i == 1) {
      // While still prerolling, Release() and Start() the player.
      ReleasePlayer();
      // The decoder is still decoding and will not be immediately released.
      EXPECT_TRUE(GetMediaCodecBridge(true));
      Resume(false, false);
    } else {
      player_.OnDemuxerDataAvailable(data);
      EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
      WaitForAudioDecodeDone();
    }
    EXPECT_TRUE(IsPrerolling(true));
  }
  EXPECT_EQ(100.0, player_.GetCurrentTime().InMillisecondsF());
  EXPECT_TRUE(IsPrerolling(true));

  // Send data after the seek position.
  PrerollDecoderToTime(true, target_timestamp, target_timestamp, true);
}

// Flaky on Android: crbug.com/419122.
#if defined(OS_ANDROID)
#define MAYBE_PrerollContinuesAcrossConfigChange \
        DISABLED_PrerollContinuesAcrossConfigChange
#else
#define MAYBE_PrerollContinuesAcrossConfigChange \
        PrerollContinuesAcrossConfigChange
#endif
TEST_F(MediaSourcePlayerTest, MAYBE_PrerollContinuesAcrossConfigChange) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decoder job will resume media prerolling if interrupted by
  // |kConfigChanged| and OnDemuxerConfigsAvailable().
  StartAudioDecoderJob();

  SeekPlayerWithAbort(true, base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(IsPrerolling(true));
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());

  DemuxerConfigs configs = CreateAudioDemuxerConfigs(kCodecVorbis, true);

  // In response to data request, simulate that demuxer signals config change by
  // sending an AU with |kConfigChanged|.
  DemuxerData data = CreateReadFromDemuxerAckWithConfigChanged(
      true, 0, configs);
  player_.OnDemuxerDataAvailable(data);

  PrerollDecoderToTime(
      true, base::TimeDelta(), base::TimeDelta::FromMilliseconds(100), true);
}

TEST_F(MediaSourcePlayerTest, PrerollContinuesAfterUnchangedConfigs) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test decoder job will resume media prerolling if interrupted by a config
  // change access unit with unchanged configs.
  StartAudioDecoderJob();

  SeekPlayerWithAbort(true, base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(IsPrerolling(true));
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());

  DemuxerConfigs configs = CreateAudioDemuxerConfigs(kCodecVorbis, false);

  // In response to data request, simulate that demuxer signals config change by
  // sending an AU with |kConfigChanged|.
  DemuxerData data = CreateReadFromDemuxerAckWithConfigChanged(
      true, 0, configs);
  player_.OnDemuxerDataAvailable(data);
  PrerollDecoderToTime(
      true, base::TimeDelta(), base::TimeDelta::FromMilliseconds(100), true);
}

TEST_F(MediaSourcePlayerTest, AudioPrerollFinishesBeforeVideo) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that after audio finishes prerolling, it will wait for video to finish
  // prerolling before advancing together.
  CreateNextTextureAndSetVideoSurface();
  Start(CreateAudioVideoDemuxerConfigs());

  // Initiate a seek.
  base::TimeDelta seek_position = base::TimeDelta::FromMilliseconds(100);
  player_.SeekTo(seek_position);
  player_.OnDemuxerDataAvailable(CreateAbortedAck(true));
  player_.OnDemuxerDataAvailable(CreateAbortedAck(false));
  WaitForDecodeDone(true, true);

  // Verify that the seek is requested.
  EXPECT_EQ(1, demuxer_->num_seek_requests());
  player_.OnDemuxerSeekDone(kNoTimestamp());
  EXPECT_EQ(4, demuxer_->num_data_requests());
  EXPECT_EQ(player_.GetCurrentTime().InMillisecondsF(), 100.0);
  EXPECT_EQ(GetPrerollTimestamp().InMillisecondsF(), 100.0);

  // Send both audio and video data to finish prefetching.
  base::TimeDelta seek_ack_position = base::TimeDelta::FromMilliseconds(70);
  DemuxerData audio_data = CreateReadFromDemuxerAckForAudio(0);
  audio_data.access_units[0].timestamp = seek_ack_position;
  DemuxerData video_data = CreateReadFromDemuxerAckForVideo(false);
  video_data.access_units[0].timestamp = seek_ack_position;
  player_.OnDemuxerDataAvailable(audio_data);
  player_.OnDemuxerDataAvailable(video_data);
  WaitForAudioDecodeDone();
  WaitForVideoDecodeDone();

  // Send audio data at and after the seek position. Audio should finish
  // prerolling and stop decoding.
  EXPECT_EQ(6, demuxer_->num_data_requests());
  PrerollDecoderToTime(true, seek_position, seek_position, true);
  EXPECT_FALSE(GetMediaDecoderJob(true)->is_decoding());
  EXPECT_FALSE(IsPrerolling(true));
  EXPECT_TRUE(IsPrerolling(false));

  // Send video data to let video finish prerolling.
  PrerollDecoderToTime(false, seek_position, seek_position, false);
  EXPECT_FALSE(IsPrerolling(false));

  // Both audio and video decoders should start decoding again.
  player_.OnDemuxerDataAvailable(audio_data);
  player_.OnDemuxerDataAvailable(video_data);
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());
  EXPECT_TRUE(GetMediaDecoderJob(false)->is_decoding());
}

TEST_F(MediaSourcePlayerTest, SimultaneousAudioVideoConfigChange) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that the player allows simultaneous audio and video config change,
  // such as might occur during OnPrefetchDone() if next access unit for both
  // audio and video jobs is |kConfigChanged|.
  CreateNextTextureAndSetVideoSurface();
  Start(CreateAudioVideoDemuxerConfigs());
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  EXPECT_TRUE(GetMediaCodecBridge(true));
  EXPECT_TRUE(GetMediaCodecBridge(false));
  EnableAdaptiveVideoPlayback(false);
  WaitForAudioVideoDecodeDone();

  // If audio or video hasn't finished prerolling, let them finish it.
  if (IsPrerolling(true))
    PrerollDecoderToTime(true, base::TimeDelta(), base::TimeDelta(), true);
  if (IsPrerolling(false))
    PrerollDecoderToTime(false, base::TimeDelta(), base::TimeDelta(), false);
  int expected_num_data_requests = demuxer_->num_data_requests();

  // Simulate audio |kConfigChanged| prefetched as standalone access unit.
  DemuxerConfigs audio_configs = CreateAudioDemuxerConfigs(kCodecVorbis, true);
  player_.OnDemuxerDataAvailable(
      CreateReadFromDemuxerAckWithConfigChanged(true, 0, audio_configs));

  // Simulate video |kConfigChanged| prefetched as standalone access unit.
  player_.OnDemuxerDataAvailable(
      CreateReadFromDemuxerAckWithConfigChanged(
          false, 0, CreateVideoDemuxerConfigs(true)));
  EXPECT_EQ(expected_num_data_requests + 2, demuxer_->num_data_requests());
  EXPECT_TRUE(IsDrainingDecoder(true));
  EXPECT_TRUE(IsDrainingDecoder(false));

  // Waiting for decoder to finish draining.
  while (IsDrainingDecoder(true) || IsDrainingDecoder(false))
    message_loop_.RunUntilIdle();
}

TEST_F(MediaSourcePlayerTest,
       SimultaneousAudioVideoConfigChangeWithAdaptivePlayback) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that the player allows simultaneous audio and video config change with
  // adaptive video playback enabled.
  CreateNextTextureAndSetVideoSurface();
  Start(CreateAudioVideoDemuxerConfigs());
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  EXPECT_EQ(4, demuxer_->num_data_requests());
  EXPECT_TRUE(GetMediaCodecBridge(true));
  EXPECT_TRUE(GetMediaCodecBridge(false));
  EnableAdaptiveVideoPlayback(true);
  WaitForAudioVideoDecodeDone();

  // If audio or video hasn't finished prerolling, let them finish it.
  if (IsPrerolling(true))
    PrerollDecoderToTime(true, base::TimeDelta(), base::TimeDelta(), true);
  if (IsPrerolling(false))
    PrerollDecoderToTime(false, base::TimeDelta(), base::TimeDelta(), false);
  int expected_num_data_requests = demuxer_->num_data_requests();

  // Simulate audio |kConfigChanged| prefetched as standalone access unit.
  DemuxerConfigs audio_configs = CreateAudioDemuxerConfigs(kCodecVorbis, true);
  player_.OnDemuxerDataAvailable(
      CreateReadFromDemuxerAckWithConfigChanged(true, 0, audio_configs));

  // Simulate video |kConfigChanged| prefetched as standalone access unit.
  player_.OnDemuxerDataAvailable(
      CreateReadFromDemuxerAckWithConfigChanged(
          false, 0, CreateVideoDemuxerConfigs(true)));
  EXPECT_EQ(expected_num_data_requests + 2, demuxer_->num_data_requests());
  EXPECT_TRUE(IsDrainingDecoder(true));
  EXPECT_FALSE(IsDrainingDecoder(false));

  // Waiting for audio decoder to finish draining.
  while (IsDrainingDecoder(true))
    message_loop_.RunUntilIdle();
}

TEST_F(MediaSourcePlayerTest, DemuxerConfigRequestedIfInPrefetchUnit0) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that the player detects need for and requests demuxer configs if
  // the |kConfigChanged| unit is the very first unit in the set of units
  // received in OnDemuxerDataAvailable() ostensibly while
  // |PREFETCH_DONE_EVENT_PENDING|.
  StartConfigChange(true, true, 0, false);
  WaitForAudioDecodeDone();
}

TEST_F(MediaSourcePlayerTest, DemuxerConfigRequestedIfInPrefetchUnit1) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that the player detects need for and requests demuxer configs if
  // the |kConfigChanged| unit is not the first unit in the set of units
  // received in OnDemuxerDataAvailable() ostensibly while
  // |PREFETCH_DONE_EVENT_PENDING|.
  StartConfigChange(true, true, 1, false);
  WaitForAudioDecodeDone();
}

TEST_F(MediaSourcePlayerTest, DemuxerConfigRequestedIfInUnit0AfterPrefetch) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that the player detects need for and requests demuxer configs if
  // the |kConfigChanged| unit is the very first unit in the set of units
  // received in OnDemuxerDataAvailable() from data requested ostensibly while
  // not prefetching.
  StartConfigChange(true, false, 0, false);
  WaitForAudioDecodeDone();
}

TEST_F(MediaSourcePlayerTest, DemuxerConfigRequestedIfInUnit1AfterPrefetch) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that the player detects need for and requests demuxer configs if
  // the |kConfigChanged| unit is not the first unit in the set of units
  // received in OnDemuxerDataAvailable() from data requested ostensibly while
  // not prefetching.
  StartConfigChange(true, false, 1, false);
  WaitForAudioDecodeDone();
}

TEST_F(MediaSourcePlayerTest, BrowserSeek_PrerollAfterBrowserSeek) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test decoder job will preroll the media to the actual seek position
  // resulting from a browser seek.
  BrowserSeekPlayer(false);

  // Simulate browser seek is done, but to a later time than was requested.
  EXPECT_LT(player_.GetCurrentTime().InMillisecondsF(), 100);
  player_.OnDemuxerSeekDone(base::TimeDelta::FromMilliseconds(100));
  // Because next AU is not I-frame, MediaCodecBridge will not be recreated.
  EXPECT_FALSE(GetMediaCodecBridge(false));
  EXPECT_EQ(100.0, player_.GetCurrentTime().InMillisecondsF());
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());
  EXPECT_EQ(3, demuxer_->num_data_requests());

  PrerollDecoderToTime(
      false, base::TimeDelta(), base::TimeDelta::FromMilliseconds(100), true);
}

TEST_F(MediaSourcePlayerTest, VideoDemuxerConfigChange) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that video config change notification results in creating a new
  // video codec without any browser seek.
  StartConfigChange(false, true, 1, false);

  // New video codec should have been created and configured, without any
  // browser seek.
  EXPECT_TRUE(GetMediaCodecBridge(false));
  EXPECT_EQ(3, demuxer_->num_data_requests());
  EXPECT_EQ(0, demuxer_->num_seek_requests());
  WaitForVideoDecodeDone();
}

TEST_F(MediaSourcePlayerTest, VideoDemuxerConfigChangeWithAdaptivePlayback) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that if codec supports adaptive playback, no new codec should be
  // created beyond the one used to decode the prefetch media data prior to
  // the kConfigChanged.
  StartConfigChange(false, true, 1, true);

  // No browser seek should be needed.
  EXPECT_TRUE(GetMediaCodecBridge(false));
  EXPECT_EQ(3, demuxer_->num_data_requests());
  EXPECT_EQ(0, demuxer_->num_seek_requests());
  WaitForVideoDecodeDone();
}

TEST_F(MediaSourcePlayerTest, DecoderDrainInterruptedBySeek) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test if a decoder is being drained while receiving a seek request, draining
  // is canceled.
  SendConfigChangeToDecoder(true, false, 0, false);
  EXPECT_TRUE(IsDrainingDecoder(true));

  player_.SeekTo(base::TimeDelta::FromMilliseconds(100));
  WaitForAudioDecodeDone();
  EXPECT_FALSE(IsDrainingDecoder(true));
  player_.OnDemuxerSeekDone(kNoTimestamp());

  EXPECT_EQ(1, demuxer_->num_seek_requests());
  EXPECT_EQ(4, demuxer_->num_data_requests());
}

TEST_F(MediaSourcePlayerTest, DecoderDrainInterruptedByRelease) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test if a decoder is being drained while receiving a release request,
  // draining is canceled.
  SendConfigChangeToDecoder(true, false, 0, false);
  EXPECT_TRUE(IsDrainingDecoder(true));

  ReleasePlayer();
  WaitForAudioDecodeDone();
  EXPECT_EQ(3, demuxer_->num_data_requests());
  EXPECT_FALSE(IsDrainingDecoder(true));

  EXPECT_FALSE(GetMediaCodecBridge(true));
  EXPECT_FALSE(player_.IsPlaying());

  player_.Start();
  EXPECT_TRUE(player_.IsPlaying());
  EXPECT_EQ(3, demuxer_->num_data_requests());
}

TEST_F(MediaSourcePlayerTest, DecoderDrainInterruptedBySurfaceChange) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test if a video decoder is being drained while surface changes, draining
  // is canceled.
  SendConfigChangeToDecoder(false, false, 0, false);
  EXPECT_TRUE(IsDrainingDecoder(false));

  CreateNextTextureAndSetVideoSurface();
  WaitForVideoDecodeDone();

  EXPECT_FALSE(IsDrainingDecoder(false));
  EXPECT_TRUE(player_.IsPlaying());

  // The frame after the config change should always be an iframe, so no browser
  // seek is needed when recreating the video decoder due to surface change.
  EXPECT_TRUE(GetMediaCodecBridge(false));
  EXPECT_EQ(4, demuxer_->num_data_requests());
  EXPECT_EQ(0, demuxer_->num_seek_requests());
}

TEST_F(MediaSourcePlayerTest,
       BrowserSeek_DecoderStarvationWhilePendingSurfaceChange) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test video decoder starvation while handling a pending surface change
  // should not cause any crashes.
  CreateNextTextureAndSetVideoSurface();
  StartVideoDecoderJob();
  DemuxerData data = CreateReadFromDemuxerAckForVideo(false);
  player_.OnDemuxerDataAvailable(data);

  // Trigger a surface change and decoder starvation.
  CreateNextTextureAndSetVideoSurface();
  TriggerPlayerStarvation();
  WaitForVideoDecodeDone();
  EXPECT_EQ(0, demuxer_->num_browser_seek_requests());

  // Surface change should trigger a seek.
  player_.OnDemuxerDataAvailable(data);
  EXPECT_EQ(1, demuxer_->num_browser_seek_requests());
  player_.OnDemuxerSeekDone(base::TimeDelta());
  // After seek is done, prefetch is handled first. MediaCodecBridge is not
  // created at this moment.
  EXPECT_FALSE(GetMediaCodecBridge(false));

  // A new data request should be sent.
  EXPECT_EQ(3, demuxer_->num_data_requests());
}

TEST_F(MediaSourcePlayerTest, ReleaseWithOnPrefetchDoneAlreadyPosted) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test if OnPrefetchDone() had already been posted before and is executed
  // after Release(), then player does not DCHECK. This test is fragile to
  // change to MediaDecoderJob::Prefetch() implementation; it assumes task
  // is posted to run |prefetch_cb| if the job already HasData().
  // TODO(wolenetz): Remove MSP::set_decode_callback_for_testing() if this test
  // becomes obsolete. See http://crbug.com/304234.
  StartAudioDecoderJob();

  // Escape the original prefetch by decoding a single access unit.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(0));
  WaitForAudioDecodeDone();

  // Prime the job with a few more access units, so that a later prefetch,
  // triggered by starvation to simulate decoder underrun, can trivially
  // post task to run OnPrefetchDone().
  player_.OnDemuxerDataAvailable(
      CreateReadFromDemuxerAckWithConfigChanged(
          true, 4, CreateAudioDemuxerConfigs(kCodecVorbis, false)));
  EXPECT_TRUE(GetMediaDecoderJob(true)->is_decoding());

  // Simulate decoder underrun, so trivial prefetch starts while still decoding.
  // The prefetch and posting of OnPrefetchDone() will not occur until next
  // MediaDecoderCallBack() occurs.
  TriggerPlayerStarvation();

  // Upon the next successful decode callback, post a task to call Release() on
  // the |player_|, such that the trivial OnPrefetchDone() task posting also
  // occurs and should execute after the Release().
  OnNextTestDecodeCallbackPostTaskToReleasePlayer();

  WaitForAudioDecodeDone();
  EXPECT_TRUE(decoder_callback_hook_executed_);

  EXPECT_EQ(3, demuxer_->num_data_requests());

  // Player should not request any new data since the access units haven't
  // been fully decoded yet.
  Resume(false, false);
}

TEST_F(MediaSourcePlayerTest, SeekToThenReleaseThenDemuxerSeekAndDone) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test if Release() occurs after SeekTo(), but the DemuxerSeek IPC request
  // has not yet been sent, then the seek request is sent after Release(). Also,
  // test if OnDemuxerSeekDone() occurs prior to next Start(), then the player
  // will resume correct post-seek preroll upon Start().
  StartAudioDecoderJobAndSeekToWhileDecoding(
      base::TimeDelta::FromMilliseconds(100));
  ReleasePlayer();
  EXPECT_EQ(0, demuxer_->num_seek_requests());
  WaitForAudioDecodeDone();
  EXPECT_EQ(1, demuxer_->num_seek_requests());

  player_.OnDemuxerSeekDone(kNoTimestamp());
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());
  EXPECT_FALSE(GetMediaCodecBridge(true));
  EXPECT_FALSE(player_.IsPlaying());

  // Player should begin prefetch and resume preroll upon Start().
  EXPECT_EQ(2, demuxer_->num_data_requests());
  Resume(true, false);
  EXPECT_TRUE(IsPrerolling(true));
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());

  // No further seek should have been requested since Release(), above.
  EXPECT_EQ(1, demuxer_->num_seek_requests());
}

TEST_F(MediaSourcePlayerTest, SeekToThenReleaseThenDemuxerSeekThenStart) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test if Release() occurs after SeekTo(), but the DemuxerSeek IPC request
  // has not yet been sent, then the seek request is sent after Release(). Also,
  // test if OnDemuxerSeekDone() does not occur until after the next Start(),
  // then the player remains pending seek done until (and resumes correct
  // post-seek preroll after) OnDemuxerSeekDone().
  StartAudioDecoderJobAndSeekToWhileDecoding(
      base::TimeDelta::FromMilliseconds(100));
  ReleasePlayer();
  EXPECT_EQ(0, demuxer_->num_seek_requests());

  // Player should not prefetch upon Start() nor create the media codec bridge,
  // due to awaiting DemuxerSeekDone.
  EXPECT_EQ(2, demuxer_->num_data_requests());
  Resume(false, false);

  WaitForAudioDecodeDone();
  EXPECT_EQ(1, demuxer_->num_seek_requests());
  player_.OnDemuxerSeekDone(kNoTimestamp());
  EXPECT_TRUE(GetMediaDecoderJob(true));
  EXPECT_TRUE(IsPrerolling(true));
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());
  EXPECT_EQ(3, demuxer_->num_data_requests());

  // No further seek should have been requested since Release(), above.
  EXPECT_EQ(1, demuxer_->num_seek_requests());
}

TEST_F(MediaSourcePlayerTest, SeekToThenDemuxerSeekThenReleaseThenSeekDone) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test if Release() occurs after a SeekTo()'s subsequent DemuxerSeek IPC
  // request and OnDemuxerSeekDone() arrives prior to the next Start(), then the
  // player will resume correct post-seek preroll upon Start().
  StartAudioDecoderJobAndSeekToWhileDecoding(
      base::TimeDelta::FromMilliseconds(100));
  WaitForAudioDecodeDone();
  EXPECT_EQ(1, demuxer_->num_seek_requests());

  ReleasePlayer();
  player_.OnDemuxerSeekDone(kNoTimestamp());
  EXPECT_FALSE(player_.IsPlaying());
  EXPECT_FALSE(GetMediaCodecBridge(true));
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());

  // Player should begin prefetch and resume preroll upon Start().
  EXPECT_EQ(2, demuxer_->num_data_requests());
  Resume(true, false);
  EXPECT_TRUE(IsPrerolling(true));
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());

  // No further seek should have been requested since before Release(), above.
  EXPECT_EQ(1, demuxer_->num_seek_requests());
}

TEST_F(MediaSourcePlayerTest, SeekToThenReleaseThenStart) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test if Release() occurs after a SeekTo()'s subsequent DemuxerSeeK IPC
  // request and OnDemuxerSeekDone() does not occur until after the next
  // Start(), then the player remains pending seek done until (and resumes
  // correct post-seek preroll after) OnDemuxerSeekDone().
  StartAudioDecoderJobAndSeekToWhileDecoding(
      base::TimeDelta::FromMilliseconds(100));
  WaitForAudioDecodeDone();
  EXPECT_EQ(1, demuxer_->num_seek_requests());

  ReleasePlayer();
  EXPECT_EQ(2, demuxer_->num_data_requests());
  Resume(false, false);

  player_.OnDemuxerSeekDone(kNoTimestamp());
  EXPECT_FALSE(GetMediaCodecBridge(true));
  EXPECT_TRUE(IsPrerolling(true));
  EXPECT_EQ(100.0, GetPrerollTimestamp().InMillisecondsF());
  EXPECT_EQ(3, demuxer_->num_data_requests());

  // No further seek should have been requested since before Release(), above.
  EXPECT_EQ(1, demuxer_->num_seek_requests());
}

TEST_F(MediaSourcePlayerTest, ConfigChangedThenReleaseThenStart) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test if Release() occurs after |kConfigChanged| is processed, new data
  // requested of demuxer, and the requested data arrive before the next
  // Start(), then the player starts to decode the new data without any seek.
  StartConfigChange(true, true, 0, false);
  ReleasePlayer();

  EXPECT_TRUE(GetMediaCodecBridge(true));
  EXPECT_FALSE(player_.IsPlaying());
  EXPECT_EQ(3, demuxer_->num_data_requests());
  player_.OnDemuxerDataAvailable(
        CreateReadFromDemuxerAckWithConfigChanged(
            true, 4, CreateAudioDemuxerConfigs(kCodecVorbis, false)));
  WaitForAudioDecodeDone();
  EXPECT_FALSE(GetMediaCodecBridge(true));

  // Player should resume upon Start(), even without further configs supplied.
  player_.Start();
  EXPECT_TRUE(player_.IsPlaying());
  EXPECT_EQ(3, demuxer_->num_data_requests());
  EXPECT_EQ(0, demuxer_->num_seek_requests());
  WaitForAudioDecodeDone();
}

TEST_F(MediaSourcePlayerTest, BrowserSeek_ThenReleaseThenDemuxerSeekDone) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that Release() after a browser seek's DemuxerSeek IPC request has been
  // sent behaves similar to a regular seek: if OnDemuxerSeekDone() occurs
  // before the next Start()+SetVideoSurface(), then the player will resume
  // correct post-seek preroll upon Start()+SetVideoSurface().
  BrowserSeekPlayer(false);
  base::TimeDelta expected_preroll_timestamp = player_.GetCurrentTime();
  ReleasePlayer();

  player_.OnDemuxerSeekDone(expected_preroll_timestamp);
  EXPECT_FALSE(player_.IsPlaying());
  EXPECT_FALSE(GetMediaCodecBridge(false));
  EXPECT_EQ(expected_preroll_timestamp, GetPrerollTimestamp());

  // Player should begin prefetch and resume preroll upon Start().
  EXPECT_EQ(2, demuxer_->num_data_requests());
  CreateNextTextureAndSetVideoSurface();
  Resume(false, true);
  EXPECT_TRUE(IsPrerolling(false));
  EXPECT_EQ(expected_preroll_timestamp, GetPrerollTimestamp());
  EXPECT_EQ(expected_preroll_timestamp, player_.GetCurrentTime());

  // No further seek should have been requested since BrowserSeekPlayer().
  EXPECT_EQ(1, demuxer_->num_seek_requests());
}

TEST_F(MediaSourcePlayerTest, BrowserSeek_ThenReleaseThenStart) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that Release() after a browser seek's DemuxerSeek IPC request has been
  // sent behaves similar to a regular seek: if OnDemuxerSeekDone() does not
  // occur until after the next Start()+SetVideoSurface(), then the player
  // remains pending seek done until (and resumes correct post-seek preroll
  // after) OnDemuxerSeekDone().
  BrowserSeekPlayer(false);
  base::TimeDelta expected_preroll_timestamp = player_.GetCurrentTime();
  ReleasePlayer();

  EXPECT_EQ(2, demuxer_->num_data_requests());
  CreateNextTextureAndSetVideoSurface();
  Resume(false, false);

  player_.OnDemuxerSeekDone(expected_preroll_timestamp);
  // Prefetch takes place first, and the decoder is not created yet.
  EXPECT_FALSE(GetMediaCodecBridge(false));
  EXPECT_TRUE(IsPrerolling(false));
  EXPECT_EQ(expected_preroll_timestamp, GetPrerollTimestamp());
  EXPECT_EQ(expected_preroll_timestamp, player_.GetCurrentTime());
  EXPECT_EQ(3, demuxer_->num_data_requests());

  // No further seek should have been requested since BrowserSeekPlayer().
  EXPECT_EQ(1, demuxer_->num_seek_requests());

  // Decoder will be created once data is received.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForVideo(false));
  EXPECT_TRUE(GetMediaCodecBridge(false));
  WaitForVideoDecodeDone();
}

// TODO(xhwang): Once we add tests to cover DrmBridge, update this test to
// also verify that the job is successfully created if SetDrmBridge(), Start()
// and eventually OnMediaCrypto() occur. This would increase test coverage of
// http://crbug.com/313470 and allow us to remove inspection of internal player
// pending event state. See http://crbug.com/313860.
TEST_F(MediaSourcePlayerTest, SurfaceChangeClearedEvenIfMediaCryptoAbsent) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that |SURFACE_CHANGE_EVENT_PENDING| is not pending after
  // SetVideoSurface() for a player configured for encrypted video, when the
  // player has not yet received media crypto.
  DemuxerConfigs configs = CreateVideoDemuxerConfigs(false);
  configs.is_video_encrypted = true;

  player_.OnDemuxerConfigsAvailable(configs);
  CreateNextTextureAndSetVideoSurface();
  EXPECT_FALSE(GetMediaCodecBridge(false));
}

TEST_F(MediaSourcePlayerTest, CurrentTimeUpdatedWhileDecoderStarved) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that current time is updated while decoder is starved.
  StartAudioDecoderJob();
  DecodeAudioDataUntilOutputBecomesAvailable();

  // Trigger starvation while the decoder is decoding.
  player_.OnDemuxerDataAvailable(CreateReadFromDemuxerAckForAudio(3));
  manager_.ResetTimestampUpdated();
  TriggerPlayerStarvation();
  WaitForAudioDecodeDone();

  // Current time should be updated.
  EXPECT_TRUE(manager_.timestamp_updated());
}

TEST_F(MediaSourcePlayerTest, CurrentTimeKeepsIncreasingAfterConfigChange) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test current time keep on increasing after audio config change.
  // Test that current time is updated while decoder is starved.
  StartAudioDecoderJob();

  DecodeAudioDataUntilOutputBecomesAvailable();

  DemuxerConfigs configs = CreateAudioDemuxerConfigs(kCodecVorbis, true);
  DemuxerData data = CreateReadFromDemuxerAckWithConfigChanged(
      true, 0, configs);
  player_.OnDemuxerDataAvailable(data);
  WaitForAudioDecodeDone();
  DecodeAudioDataUntilOutputBecomesAvailable();
}

TEST_F(MediaSourcePlayerTest, VideoMetadataChangeAfterConfigChange) {
  // crbug.com/604602 and crbug.com/597836
  SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED();

  // Test that after a config change, metadata change will be happen
  // after decoder is drained.
  StartConfigChange(false, true, 2, false);
  EXPECT_EQ(1, manager_.num_metadata_changes());
  EXPECT_FALSE(IsDrainingDecoder(false));

  // Create video data with new resolutions.
  DemuxerData data = CreateReadFromDemuxerAckForVideo(true);

  // Wait for the metadata change.
  while(manager_.num_metadata_changes() == 1) {
    player_.OnDemuxerDataAvailable(data);
    WaitForVideoDecodeDone();
  }
  EXPECT_EQ(2, manager_.num_metadata_changes());
  WaitForVideoDecodeDone();
}

TEST_F(MediaSourcePlayerTest, RequestPlayDeniedDontPlay_Audio) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  EXPECT_EQ(demuxer_->num_data_requests(), 0);
  player_.OnDemuxerConfigsAvailable(CreateDemuxerConfigs(true, false));

  manager_.set_allow_play(false);
  player_.Start();
  EXPECT_FALSE(player_.IsPlaying());
}

TEST_F(MediaSourcePlayerTest, RequestPlayDeniedDontPlay_Video) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  EXPECT_EQ(demuxer_->num_data_requests(), 0);
  player_.OnDemuxerConfigsAvailable(CreateDemuxerConfigs(false, true));

  manager_.set_allow_play(false);
  player_.Start();
  EXPECT_FALSE(player_.IsPlaying());
}

TEST_F(MediaSourcePlayerTest, RequestPlayDeniedDontPlay_AV) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  EXPECT_EQ(demuxer_->num_data_requests(), 0);
  player_.OnDemuxerConfigsAvailable(CreateDemuxerConfigs(true, true));

  manager_.set_allow_play(false);
  player_.Start();
  EXPECT_FALSE(player_.IsPlaying());
}

TEST_F(MediaSourcePlayerTest, RequestPlayGrantedPlays) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  EXPECT_EQ(demuxer_->num_data_requests(), 0);
  player_.OnDemuxerConfigsAvailable(CreateDemuxerConfigs(true, true));

  manager_.set_allow_play(true);
  player_.Start();
  EXPECT_TRUE(player_.IsPlaying());
}

}  // namespace media
