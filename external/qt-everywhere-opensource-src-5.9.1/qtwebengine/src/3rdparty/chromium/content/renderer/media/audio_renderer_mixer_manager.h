// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_MIXER_MANAGER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_MIXER_MANAGER_H_

#include <bitset>
#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_mixer_pool.h"
#include "media/base/output_device_info.h"
#include "url/origin.h"

namespace media {
class AudioRendererMixer;
class AudioRendererMixerInput;
class AudioRendererSink;
}

namespace content {
class AudioRendererSinkCache;

// Manages sharing of an AudioRendererMixer among AudioRendererMixerInputs based
// on their AudioParameters configuration.  Inputs with the same AudioParameters
// configuration will share a mixer while a new AudioRendererMixer will be
// lazily created if one with the exact AudioParameters does not exist. When an
// AudioRendererMixer is returned by AudioRendererMixerInput, it will be deleted
// if its only other reference is held by AudioRendererMixerManager.
//
// There should only be one instance of AudioRendererMixerManager per render
// thread.
//
// TODO(dalecurtis): Right now we require AudioParameters to be an exact match
// when we should be able to ignore bits per channel since we're only dealing
// with floats.  However, bits per channel is currently used to interleave the
// audio data by AudioOutputDevice::AudioThreadCallback::Process for consumption
// via the shared memory.  See http://crbug.com/114700.
class CONTENT_EXPORT AudioRendererMixerManager
    : public media::AudioRendererMixerPool {
 public:
  ~AudioRendererMixerManager() final;

  static std::unique_ptr<AudioRendererMixerManager> Create();

  // Creates an AudioRendererMixerInput with the proper callbacks necessary to
  // retrieve an AudioRendererMixer instance from AudioRendererMixerManager.
  // |source_render_frame_id| refers to the RenderFrame containing the entity
  // rendering the audio.  Caller must ensure AudioRendererMixerManager outlives
  // the returned input. |device_id|, |session_id| and |security_origin|
  // identify the output device to use. If |device_id| is empty and |session_id|
  // is nonzero, output device associated with the opened input device
  // designated by |session_id| is used. Otherwise, |session_id| is ignored.
  media::AudioRendererMixerInput* CreateInput(
      int source_render_frame_id,
      int session_id,
      const std::string& device_id,
      const url::Origin& security_origin,
      media::AudioLatency::LatencyType latency);

  // AudioRendererMixerPool implementation.

  media::AudioRendererMixer* GetMixer(
      int source_render_frame_id,
      const media::AudioParameters& input_params,
      media::AudioLatency::LatencyType latency,
      const std::string& device_id,
      const url::Origin& security_origin,
      media::OutputDeviceStatus* device_status) final;

  void ReturnMixer(media::AudioRendererMixer* mixer) final;

  media::OutputDeviceInfo GetOutputDeviceInfo(
      int source_render_frame_id,
      int session_id,
      const std::string& device_id,
      const url::Origin& security_origin) final;

 protected:
  explicit AudioRendererMixerManager(
      std::unique_ptr<AudioRendererSinkCache> sink_cache);

 private:
  friend class AudioRendererMixerManagerTest;

  // Define a key so that only those AudioRendererMixerInputs from the same
  // RenderView, AudioParameters and output device can be mixed together.
  struct MixerKey {
    MixerKey(int source_render_frame_id,
             const media::AudioParameters& params,
             media::AudioLatency::LatencyType latency,
             const std::string& device_id,
             const url::Origin& security_origin);
    MixerKey(const MixerKey& other);
    int source_render_frame_id;
    media::AudioParameters params;
    media::AudioLatency::LatencyType latency;
    std::string device_id;
    url::Origin security_origin;
  };

  // Custom compare operator for the AudioRendererMixerMap.  Allows reuse of
  // mixers where only irrelevant keys mismatch; e.g., effects, bits per sample.
  struct MixerKeyCompare {
    bool operator()(const MixerKey& a, const MixerKey& b) const {
      if (a.source_render_frame_id != b.source_render_frame_id)
        return a.source_render_frame_id < b.source_render_frame_id;
      if (a.params.channels() != b.params.channels())
        return a.params.channels() < b.params.channels();

      if (a.latency != b.latency)
        return a.latency < b.latency;

      // TODO(olka) add buffer duration comparison for LATENCY_EXACT_MS when
      // adding support for it.
      DCHECK_NE(media::AudioLatency::LATENCY_EXACT_MS, a.latency);

      // Ignore effects(), bits_per_sample(), format(), and frames_per_buffer(),
      // these parameters do not affect mixer reuse.  All AudioRendererMixer
      // units disable FIFO, so frames_per_buffer() can be safely ignored.
      if (a.params.channel_layout() != b.params.channel_layout())
        return a.params.channel_layout() < b.params.channel_layout();

      if (media::AudioDeviceDescription::IsDefaultDevice(a.device_id) &&
          media::AudioDeviceDescription::IsDefaultDevice(b.device_id)) {
        // Both device IDs represent the same default device => do not compare
        // them; the default device is always authorized => ignoring security
        // origin.
        return false;
      }

      if (a.device_id != b.device_id)
        return a.device_id < b.device_id;

      return a.security_origin < b.security_origin;
    }
  };

  // Map of MixerKey to <AudioRendererMixer, Count>.  Count allows
  // AudioRendererMixerManager to keep track explicitly (v.s. RefCounted which
  // is implicit) of the number of outstanding AudioRendererMixers.
  struct AudioRendererMixerReference {
    media::AudioRendererMixer* mixer;
    int ref_count;
    // Mixer sink pointer, to remove a sink from cache upon mixer destruction.
    const media::AudioRendererSink* sink_ptr;
  };

  using AudioRendererMixerMap =
      std::map<MixerKey, AudioRendererMixerReference, MixerKeyCompare>;

  // Active mixers.
  AudioRendererMixerMap mixers_;
  base::Lock mixers_lock_;

  // Mixer sink cache.
  const std::unique_ptr<AudioRendererSinkCache> sink_cache_;

  // Map of the output latencies encountered throughout mixer manager lifetime.
  // Used for UMA histogram logging.
  std::bitset<media::AudioLatency::LATENCY_COUNT> latency_map_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_MIXER_MANAGER_H_
