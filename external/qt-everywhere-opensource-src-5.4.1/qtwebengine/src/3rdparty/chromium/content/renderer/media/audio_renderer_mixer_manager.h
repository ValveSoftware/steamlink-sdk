// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_MIXER_MANAGER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_MIXER_MANAGER_H_

#include <map>
#include <utility>

#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "media/audio/audio_parameters.h"

namespace media {
class AudioHardwareConfig;
class AudioRendererMixer;
class AudioRendererMixerInput;
class AudioRendererSink;
}

namespace content {

// Manages sharing of an AudioRendererMixer among AudioRendererMixerInputs based
// on their AudioParameters configuration.  Inputs with the same AudioParameters
// configuration will share a mixer while a new AudioRendererMixer will be
// lazily created if one with the exact AudioParameters does not exist.
//
// There should only be one instance of AudioRendererMixerManager per render
// thread.
//
// TODO(dalecurtis): Right now we require AudioParameters to be an exact match
// when we should be able to ignore bits per channel since we're only dealing
// with floats.  However, bits per channel is currently used to interleave the
// audio data by AudioOutputDevice::AudioThreadCallback::Process for consumption
// via the shared memory.  See http://crbug.com/114700.
class CONTENT_EXPORT AudioRendererMixerManager {
 public:
  // Construct an instance using the given audio hardware configuration.  The
  // provided |hardware_config| is not owned by AudioRendererMixerManager and
  // must outlive it.
  explicit AudioRendererMixerManager(
      media::AudioHardwareConfig* hardware_config);
  ~AudioRendererMixerManager();

  // Creates an AudioRendererMixerInput with the proper callbacks necessary to
  // retrieve an AudioRendererMixer instance from AudioRendererMixerManager.
  // |source_render_view_id| refers to the RenderView containing the entity
  // rendering the audio.  |source_render_frame_id| refers to the RenderFrame
  // containing the entity rendering the audio.  Caller must ensure
  // AudioRendererMixerManager outlives the returned input.
  media::AudioRendererMixerInput* CreateInput(int source_render_view_id,
                                              int source_render_frame_id);

  // Returns a mixer instance based on AudioParameters; an existing one if one
  // with the provided AudioParameters exists or a new one if not.
  media::AudioRendererMixer* GetMixer(int source_render_view_id,
                                      int source_render_frame_id,
                                      const media::AudioParameters& params);

  // Remove a mixer instance given a mixer if the only other reference is held
  // by AudioRendererMixerManager.  Every AudioRendererMixer owner must call
  // this method when it's done with a mixer.
  void RemoveMixer(int source_render_view_id,
                   const media::AudioParameters& params);

 private:
  friend class AudioRendererMixerManagerTest;

  // Define a key so that only those AudioRendererMixerInputs from the same
  // RenderView and with the same AudioParameters can be mixed together.
  typedef std::pair<int, media::AudioParameters> MixerKey;

  // Map of MixerKey to <AudioRendererMixer, Count>.  Count allows
  // AudioRendererMixerManager to keep track explicitly (v.s. RefCounted which
  // is implicit) of the number of outstanding AudioRendererMixers.
  struct AudioRendererMixerReference {
    media::AudioRendererMixer* mixer;
    int ref_count;
  };
  typedef std::map<MixerKey, AudioRendererMixerReference> AudioRendererMixerMap;

  // Overrides the AudioRendererSink implementation for unit testing.
  void SetAudioRendererSinkForTesting(media::AudioRendererSink* sink);

  // Active mixers.
  AudioRendererMixerMap mixers_;
  base::Lock mixers_lock_;

  // Audio hardware configuration.  Used to construct output AudioParameters for
  // each AudioRendererMixer instance.
  media::AudioHardwareConfig* const hardware_config_;

  media::AudioRendererSink* sink_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_MIXER_MANAGER_H_
