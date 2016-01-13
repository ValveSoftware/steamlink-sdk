// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_RESAMPLER_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_RESAMPLER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/audio_parameters.h"

namespace media {

class OnMoreDataConverter;

// AudioOutputResampler is a browser-side resampling and buffering solution
// which ensures audio data is always output at given parameters.  See the
// AudioConverter class for details on the conversion process.
//
// AOR works by intercepting the AudioSourceCallback provided to StartStream()
// and redirecting it through an AudioConverter instance.  AudioBuffersState is
// adjusted for buffer delay caused by the conversion process.
//
// AOR will automatically fall back from AUDIO_PCM_LOW_LATENCY to
// AUDIO_PCM_LINEAR if the output device fails to open at the requested output
// parameters.
//
// TODO(dalecurtis): Ideally the low latency path will be as reliable as the
// high latency path once we have channel mixing and support querying for the
// hardware's configured bit depth.  Monitor the UMA stats for fallback and
// remove fallback support once it's stable.  http://crbug.com/148418
class MEDIA_EXPORT AudioOutputResampler : public AudioOutputDispatcher {
 public:
  AudioOutputResampler(AudioManager* audio_manager,
                       const AudioParameters& input_params,
                       const AudioParameters& output_params,
                       const std::string& output_device_id,
                       const base::TimeDelta& close_delay);

  // AudioOutputDispatcher interface.
  virtual bool OpenStream() OVERRIDE;
  virtual bool StartStream(AudioOutputStream::AudioSourceCallback* callback,
                           AudioOutputProxy* stream_proxy) OVERRIDE;
  virtual void StopStream(AudioOutputProxy* stream_proxy) OVERRIDE;
  virtual void StreamVolumeSet(AudioOutputProxy* stream_proxy,
                               double volume) OVERRIDE;
  virtual void CloseStream(AudioOutputProxy* stream_proxy) OVERRIDE;
  virtual void Shutdown() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<AudioOutputResampler>;
  virtual ~AudioOutputResampler();

  // Converts low latency based output parameters into high latency
  // appropriate output parameters in error situations.
  void SetupFallbackParams();

  // Used to initialize and reinitialize |dispatcher_|.
  void Initialize();

  // Dispatcher to proxy all AudioOutputDispatcher calls too.
  scoped_refptr<AudioOutputDispatcher> dispatcher_;

  // Map of outstanding OnMoreDataConverter objects.  A new object is created
  // on every StartStream() call and destroyed on CloseStream().
  typedef std::map<AudioOutputProxy*, OnMoreDataConverter*> CallbackMap;
  CallbackMap callbacks_;

  // Used by AudioOutputDispatcherImpl; kept so we can reinitialize on the fly.
  base::TimeDelta close_delay_;

  // AudioParameters used to setup the output stream.
  AudioParameters output_params_;

  // Whether any streams have been opened through |dispatcher_|, if so we can't
  // fallback on future OpenStream() failures.
  bool streams_opened_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputResampler);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_RESAMPLER_H_
