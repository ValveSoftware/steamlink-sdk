// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_TEST_DATA_FACTORY_H_
#define MEDIA_BASE_ANDROID_TEST_DATA_FACTORY_H_

#include <stddef.h>
#include <stdint.h>
#include <set>
#include <vector>
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/android/demuxer_stream_player_params.h"

namespace media {

// TestDataFactory is used by MediaCodecDecoder unit test and MediaCodecPlayer
// unit test to simulate the audio or video access unit stream.
class TestDataFactory {
 public:
  // These methods return corresponding demuxer configs.
  static DemuxerConfigs CreateAudioConfigs(AudioCodec audio_codec,
                                           base::TimeDelta duration);
  static DemuxerConfigs CreateVideoConfigs(VideoCodec video_codec,
                                           base::TimeDelta duration,
                                           const gfx::Size& video_size);

  // Constructor calls |LoadPackets| to load packets from files.
  // Parameters:
  //   file_name_template: the sprintf format string used to generate a file
  //                       name for the packet in the form e.g. "h264-AxB-%d"
  //                       The |%d| will be replaced by 0, 1, 2, 3.
  //   duration: after the last AU exceeds duration the factory generates EOS
  //             unit and stops.
  //   frame_period: PTS increment between units.
  TestDataFactory(const char* file_name_template,
                  const base::TimeDelta duration,
                  const base::TimeDelta frame_period);
  virtual ~TestDataFactory();

  // Returns demuxer configuration for this factory.
  virtual DemuxerConfigs GetConfigs() const = 0;

  // Populates next chunk and the corresponding delay and returns true if
  // duration is not exceeded, otherwise returns false.
  // Default implementation repeatedly uses |packet_| array in order 0-1-2-3
  // and monotonically increases timestamps from 0 to |duration_|.
  // The first unit to exceed |duration_| becomes EOS. The delay is set to 0.
  virtual bool CreateChunk(DemuxerData* chunk, base::TimeDelta* delay);

  // In starvation mode we do not add EOS at the end.
  void SetStarvationMode(bool value) { starvation_mode_ = value; }

  // Resets the timestamp for the next access unit.
  void SeekTo(const base::TimeDelta& seek_time);

  // Request that a chunk containing sole |kConfigChanged| unit is generated
  // before the first true data chunk.
  void RequestInitialConfigs();

  void RequestConfigChange(base::TimeDelta config_position);

  // Returns the maximum PTS, taking into account possible modifications
  // by subclasses. The SeekTo() resets this value.
  base::TimeDelta last_pts() const { return last_pts_; }

 protected:
  // Called by constructor to load packets from files referred by
  // |file_name_template|.
  virtual void LoadPackets(const char* file_name_template);

  // Used to modify the generated chunk by a subclass.
  virtual void ModifyChunk(DemuxerData* chunk) {}

  base::TimeDelta duration_;
  base::TimeDelta frame_period_;

 private:
  typedef std::set<base::TimeDelta> PTSSet;

  // |left| is included in the interval, |right| is excluded.
  // If |left| == |right|, the interval is empty and the method returns false.
  bool HasReconfigForInterval(base::TimeDelta left,
                              base::TimeDelta right) const;

  void AddConfiguration(DemuxerData* chunk);

  std::vector<uint8_t> packet_[4];
  base::TimeDelta regular_pts_;  // monotonically increasing PTS
  base::TimeDelta chunk_begin_pts_;  // beginning of chunk time interval
  base::TimeDelta last_pts_;     // subclass can modify PTS, maintain the last
  PTSSet reconfigs_;             // ConfigChange requests
  size_t total_chunks_;          // total number of chunks returned
  bool starvation_mode_;         // true means no EOS at the end
  bool eos_reached_;             // true if CreateChunk() returned EOS frame
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_TEST_DATA_FACTORY_H_
