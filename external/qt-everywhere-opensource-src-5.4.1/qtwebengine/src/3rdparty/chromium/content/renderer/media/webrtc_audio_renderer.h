// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_RENDERER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_RENDERER_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/media/media_stream_audio_renderer.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_pull_fifo.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/channel_layout.h"

namespace media {
class AudioOutputDevice;
}  // namespace media

namespace webrtc {
class AudioSourceInterface;
class MediaStreamInterface;
}  // namespace webrtc

namespace content {

class WebRtcAudioRendererSource;

// This renderer handles calls from the pipeline and WebRtc ADM. It is used
// for connecting WebRtc MediaStream with the audio pipeline.
class CONTENT_EXPORT WebRtcAudioRenderer
    : NON_EXPORTED_BASE(public media::AudioRendererSink::RenderCallback),
      NON_EXPORTED_BASE(public MediaStreamAudioRenderer) {
 public:
  // This is a little utility class that holds the configured state of an audio
  // stream.
  // It is used by both WebRtcAudioRenderer and SharedAudioRenderer (see cc
  // file) so a part of why it exists is to avoid code duplication and track
  // the state in the same way in WebRtcAudioRenderer and SharedAudioRenderer.
  class PlayingState : public base::NonThreadSafe {
   public:
    PlayingState() : playing_(false), volume_(1.0f) {}

    bool playing() const {
      DCHECK(CalledOnValidThread());
      return playing_;
    }

    void set_playing(bool playing) {
      DCHECK(CalledOnValidThread());
      playing_ = playing;
    }

    float volume() const {
      DCHECK(CalledOnValidThread());
      return volume_;
    }

    void set_volume(float volume) {
      DCHECK(CalledOnValidThread());
      volume_ = volume;
    }

   private:
    bool playing_;
    float volume_;
  };

  WebRtcAudioRenderer(
      const scoped_refptr<webrtc::MediaStreamInterface>& media_stream,
      int source_render_view_id,
      int source_render_frame_id,
      int session_id,
      int sample_rate,
      int frames_per_buffer);

  // Initialize function called by clients like WebRtcAudioDeviceImpl.
  // Stop() has to be called before |source| is deleted.
  bool Initialize(WebRtcAudioRendererSource* source);

  // When sharing a single instance of WebRtcAudioRenderer between multiple
  // users (e.g. WebMediaPlayerMS), call this method to create a proxy object
  // that maintains the Play and Stop states per caller.
  // The wrapper ensures that Play() won't be called when the caller's state
  // is "playing", Pause() won't be called when the state already is "paused"
  // etc and similarly maintains the same state for Stop().
  // When Stop() is called or when the proxy goes out of scope, the proxy
  // will ensure that Pause() is called followed by a call to Stop(), which
  // is the usage pattern that WebRtcAudioRenderer requires.
  scoped_refptr<MediaStreamAudioRenderer> CreateSharedAudioRendererProxy(
      const scoped_refptr<webrtc::MediaStreamInterface>& media_stream);

  // Used to DCHECK on the expected state.
  bool IsStarted() const;

  // Accessors to the sink audio parameters.
  int channels() const { return sink_params_.channels(); }
  int sample_rate() const { return sink_params_.sample_rate(); }

 private:
  // MediaStreamAudioRenderer implementation.  This is private since we want
  // callers to use proxy objects.
  // TODO(tommi): Make the MediaStreamAudioRenderer implementation a pimpl?
  virtual void Start() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(float volume) OVERRIDE;
  virtual base::TimeDelta GetCurrentRenderTime() const OVERRIDE;
  virtual bool IsLocalRenderer() const OVERRIDE;

  // Called when an audio renderer, either the main or a proxy, starts playing.
  // Here we maintain a reference count of how many renderers are currently
  // playing so that the shared play state of all the streams can be reflected
  // correctly.
  void EnterPlayState();

  // Called when an audio renderer, either the main or a proxy, is paused.
  // See EnterPlayState for more details.
  void EnterPauseState();

 protected:
  virtual ~WebRtcAudioRenderer();

 private:
  enum State {
    UNINITIALIZED,
    PLAYING,
    PAUSED,
  };

  // Holds raw pointers to PlaingState objects.  Ownership is managed outside
  // of this type.
  typedef std::vector<PlayingState*> PlayingStates;
  // Maps an audio source to a list of playing states that collectively hold
  // volume information for that source.
  typedef std::map<webrtc::AudioSourceInterface*, PlayingStates>
      SourcePlayingStates;

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // Flag to keep track the state of the renderer.
  State state_;

  // media::AudioRendererSink::RenderCallback implementation.
  // These two methods are called on the AudioOutputDevice worker thread.
  virtual int Render(media::AudioBus* audio_bus,
                     int audio_delay_milliseconds) OVERRIDE;
  virtual void OnRenderError() OVERRIDE;

  // Called by AudioPullFifo when more data is necessary.
  // This method is called on the AudioOutputDevice worker thread.
  void SourceCallback(int fifo_frame_delay, media::AudioBus* audio_bus);

  // Goes through all renderers for the |source| and applies the proper
  // volume scaling for the source based on the volume(s) of the renderer(s).
  void UpdateSourceVolume(webrtc::AudioSourceInterface* source);

  // Tracks a playing state.  The state must be playing when this method
  // is called.
  // Returns true if the state was added, false if it was already being tracked.
  bool AddPlayingState(webrtc::AudioSourceInterface* source,
                       PlayingState* state);
  // Removes a playing state for an audio source.
  // Returns true if the state was removed from the internal map, false if
  // it had already been removed or if the source isn't being rendered.
  bool RemovePlayingState(webrtc::AudioSourceInterface* source,
                          PlayingState* state);

  // Called whenever the Play/Pause state changes of any of the renderers
  // or if the volume of any of them is changed.
  // Here we update the shared Play state and apply volume scaling to all audio
  // sources associated with the |media_stream| based on the collective volume
  // of playing renderers.
  void OnPlayStateChanged(
      const scoped_refptr<webrtc::MediaStreamInterface>& media_stream,
      PlayingState* state);

  // The render view and frame in which the audio is rendered into |sink_|.
  const int source_render_view_id_;
  const int source_render_frame_id_;
  const int session_id_;

  // The sink (destination) for rendered audio.
  scoped_refptr<media::AudioOutputDevice> sink_;

  // The media stream that holds the audio tracks that this renderer renders.
  const scoped_refptr<webrtc::MediaStreamInterface> media_stream_;

  // Audio data source from the browser process.
  WebRtcAudioRendererSource* source_;

  // Protects access to |state_|, |source_|, |sink_| and |current_time_|.
  mutable base::Lock lock_;

  // Ref count for the MediaPlayers which are playing audio.
  int play_ref_count_;

  // Ref count for the MediaPlayers which have called Start() but not Stop().
  int start_ref_count_;

  // Used to buffer data between the client and the output device in cases where
  // the client buffer size is not the same as the output device buffer size.
  scoped_ptr<media::AudioPullFifo> audio_fifo_;

  // Contains the accumulated delay estimate which is provided to the WebRTC
  // AEC.
  int audio_delay_milliseconds_;

  // Delay due to the FIFO in milliseconds.
  int fifo_delay_milliseconds_;

  base::TimeDelta current_time_;

  // Saved volume and playing state of the root renderer.
  PlayingState playing_state_;

  // Audio params used by the sink of the renderer.
  media::AudioParameters sink_params_;

  // Maps audio sources to a list of active audio renderers.
  // Pointers to PlayingState objects are only kept in this map while the
  // associated renderer is actually playing the stream.  Ownership of the
  // state objects lies with the renderers and they must leave the playing state
  // before being destructed (PlayingState object goes out of scope).
  SourcePlayingStates source_playing_states_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebRtcAudioRenderer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_RENDERER_H_
