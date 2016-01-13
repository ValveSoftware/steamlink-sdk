// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_IMPL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "content/renderer/media/buffered_data_source_host_impl.h"
#include "content/renderer/media/crypto/proxy_decryptor.h"
#include "content/renderer/media/video_frame_compositor.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/decryptor.h"
// TODO(xhwang): Remove when we remove prefixed EME implementation.
#include "media/base/media_keys.h"
#include "media/base/pipeline.h"
#include "media/base/text_track.h"
#include "media/filters/skcanvas_video_renderer.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebAudioSourceProvider.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "url/gurl.h"

class RenderAudioSourceProvider;

namespace blink {
class WebContentDecryptionModule;
class WebLocalFrame;
}

namespace base {
class MessageLoopProxy;
}

namespace media {
class ChunkDemuxer;
class GpuVideoAcceleratorFactories;
class MediaLog;
}


namespace content {
class BufferedDataSource;
class VideoFrameCompositor;
class WebAudioSourceProviderImpl;
class WebContentDecryptionModuleImpl;
class WebLayerImpl;
class WebMediaPlayerDelegate;
class WebMediaPlayerParams;
class WebTextTrackImpl;

// The canonical implementation of blink::WebMediaPlayer that's backed by
// media::Pipeline. Handles normal resource loading, Media Source, and
// Encrypted Media.
class WebMediaPlayerImpl
    : public blink::WebMediaPlayer,
      public base::SupportsWeakPtr<WebMediaPlayerImpl> {
 public:
  // Constructs a WebMediaPlayer implementation using Chromium's media stack.
  // |delegate| may be null.
  WebMediaPlayerImpl(blink::WebLocalFrame* frame,
                     blink::WebMediaPlayerClient* client,
                     base::WeakPtr<WebMediaPlayerDelegate> delegate,
                     const WebMediaPlayerParams& params);
  virtual ~WebMediaPlayerImpl();

  virtual void load(LoadType load_type,
                    const blink::WebURL& url,
                    CORSMode cors_mode);

  // Playback controls.
  virtual void play();
  virtual void pause();
  virtual bool supportsSave() const;
  virtual void seek(double seconds);
  virtual void setRate(double rate);
  virtual void setVolume(double volume);
  virtual void setPreload(blink::WebMediaPlayer::Preload preload);
  virtual blink::WebTimeRanges buffered() const;
  virtual double maxTimeSeekable() const;

  // Methods for painting.
  virtual void paint(blink::WebCanvas* canvas,
                     const blink::WebRect& rect,
                     unsigned char alpha);

  // True if the loaded media has a playable video/audio track.
  virtual bool hasVideo() const;
  virtual bool hasAudio() const;

  // Dimensions of the video.
  virtual blink::WebSize naturalSize() const;

  // Getters of playback state.
  virtual bool paused() const;
  virtual bool seeking() const;
  virtual double duration() const;
  virtual double timelineOffset() const;
  virtual double currentTime() const;

  // Internal states of loading and network.
  // TODO(hclam): Ask the pipeline about the state rather than having reading
  // them from members which would cause race conditions.
  virtual blink::WebMediaPlayer::NetworkState networkState() const;
  virtual blink::WebMediaPlayer::ReadyState readyState() const;

  virtual bool didLoadingProgress();

  virtual bool hasSingleSecurityOrigin() const;
  virtual bool didPassCORSAccessCheck() const;

  virtual double mediaTimeForTimeValue(double timeValue) const;

  virtual unsigned decodedFrameCount() const;
  virtual unsigned droppedFrameCount() const;
  virtual unsigned audioDecodedByteCount() const;
  virtual unsigned videoDecodedByteCount() const;

  virtual bool copyVideoTextureToPlatformTexture(
      blink::WebGraphicsContext3D* web_graphics_context,
      unsigned int texture,
      unsigned int level,
      unsigned int internal_format,
      unsigned int type,
      bool premultiply_alpha,
      bool flip_y);

  virtual blink::WebAudioSourceProvider* audioSourceProvider();

  virtual MediaKeyException generateKeyRequest(
      const blink::WebString& key_system,
      const unsigned char* init_data,
      unsigned init_data_length);

  virtual MediaKeyException addKey(const blink::WebString& key_system,
                                   const unsigned char* key,
                                   unsigned key_length,
                                   const unsigned char* init_data,
                                   unsigned init_data_length,
                                   const blink::WebString& session_id);

  virtual MediaKeyException cancelKeyRequest(
      const blink::WebString& key_system,
      const blink::WebString& session_id);

  virtual void setContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm);

  // Notifies blink that the entire media element region has been invalidated.
  // This path is slower than notifying the compositor directly as it performs
  // more work and can trigger layouts. It should only be used in two cases:
  //   1) Major state changes (e.g., first frame available, run time error
  //      occured)
  //   2) Compositing not available
  void InvalidateOnMainThread();

  void OnPipelineSeek(media::PipelineStatus status);
  void OnPipelineEnded();
  void OnPipelineError(media::PipelineStatus error);
  void OnPipelineMetadata(media::PipelineMetadata metadata);
  void OnPipelinePrerollCompleted();
  void OnDemuxerOpened();
  void OnKeyAdded(const std::string& session_id);
  void OnKeyError(const std::string& session_id,
                  media::MediaKeys::KeyError error_code,
                  uint32 system_code);
  void OnKeyMessage(const std::string& session_id,
                    const std::vector<uint8>& message,
                    const GURL& destination_url);
  void OnNeedKey(const std::string& type,
                 const std::vector<uint8>& init_data);
  void OnAddTextTrack(const media::TextTrackConfig& config,
                      const media::AddTextTrackDoneCB& done_cb);

 private:
  // Called after |defer_load_cb_| has decided to allow the load. If
  // |defer_load_cb_| is null this is called immediately.
  void DoLoad(LoadType load_type,
              const blink::WebURL& url,
              CORSMode cors_mode);

  // Called after asynchronous initialization of a data source completed.
  void DataSourceInitialized(bool success);

  // Called when the data source is downloading or paused.
  void NotifyDownloading(bool is_downloading);

  // Finishes starting the pipeline due to a call to load().
  void StartPipeline();

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(blink::WebMediaPlayer::NetworkState state);
  void SetReadyState(blink::WebMediaPlayer::ReadyState state);

  // Lets V8 know that player uses extra resources not managed by V8.
  void IncrementExternallyAllocatedMemory();

  // Actually do the work for generateKeyRequest/addKey so they can easily
  // report results to UMA.
  MediaKeyException GenerateKeyRequestInternal(const std::string& key_system,
                                               const unsigned char* init_data,
                                               unsigned init_data_length);
  MediaKeyException AddKeyInternal(const std::string& key_system,
                                   const unsigned char* key,
                                   unsigned key_length,
                                   const unsigned char* init_data,
                                   unsigned init_data_length,
                                   const std::string& session_id);
  MediaKeyException CancelKeyRequestInternal(const std::string& key_system,
                                             const std::string& session_id);

  // Gets the duration value reported by the pipeline.
  double GetPipelineDuration() const;

  // Callbacks from |pipeline_| that are forwarded to |client_|.
  void OnDurationChanged();
  void OnNaturalSizeChanged(gfx::Size size);
  void OnOpacityChanged(bool opaque);

  // Called by VideoRendererImpl on its internal thread with the new frame to be
  // painted.
  void FrameReady(const scoped_refptr<media::VideoFrame>& frame);

  // Requests that this object notifies when a decryptor is ready through the
  // |decryptor_ready_cb| provided.
  // If |decryptor_ready_cb| is null, the existing callback will be fired with
  // NULL immediately and reset.
  void SetDecryptorReadyCB(const media::DecryptorReadyCB& decryptor_ready_cb);

  // Returns the current video frame from |compositor_|. Blocks until the
  // compositor can return the frame.
  scoped_refptr<media::VideoFrame> GetCurrentFrameFromCompositor();

  blink::WebLocalFrame* frame_;

  // TODO(hclam): get rid of these members and read from the pipeline directly.
  blink::WebMediaPlayer::NetworkState network_state_;
  blink::WebMediaPlayer::ReadyState ready_state_;

  // Message loops for posting tasks on Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  const scoped_refptr<base::MessageLoopProxy> main_loop_;

  scoped_refptr<base::MessageLoopProxy> media_loop_;
  scoped_refptr<media::MediaLog> media_log_;
  media::Pipeline pipeline_;

  // The currently selected key system. Empty string means that no key system
  // has been selected.
  std::string current_key_system_;

  // The LoadType passed in the |load_type| parameter of the load() call.
  LoadType load_type_;

  // Cache of metadata for answering hasAudio(), hasVideo(), and naturalSize().
  media::PipelineMetadata pipeline_metadata_;

  // Whether the video is known to be opaque or not.
  bool opaque_;

  // Playback state.
  //
  // TODO(scherkus): we have these because Pipeline favours the simplicity of a
  // single "playback rate" over worrying about paused/stopped etc...  It forces
  // all clients to manage the pause+playback rate externally, but is that
  // really a bad thing?
  //
  // TODO(scherkus): since SetPlaybackRate(0) is asynchronous and we don't want
  // to hang the render thread during pause(), we record the time at the same
  // time we pause and then return that value in currentTime().  Otherwise our
  // clock can creep forward a little bit while the asynchronous
  // SetPlaybackRate(0) is being executed.
  bool paused_;
  bool seeking_;
  double playback_rate_;
  base::TimeDelta paused_time_;

  // Seek gets pending if another seek is in progress. Only last pending seek
  // will have effect.
  bool pending_seek_;
  double pending_seek_seconds_;

  blink::WebMediaPlayerClient* client_;

  base::WeakPtr<WebMediaPlayerDelegate> delegate_;

  base::Callback<void(const base::Closure&)> defer_load_cb_;

  // Since accelerated compositing status is only known after the first layout,
  // we delay reporting it to UMA until that time.
  bool accelerated_compositing_reported_;

  bool incremented_externally_allocated_memory_;

  // Factories for supporting video accelerators. May be null.
  scoped_refptr<media::GpuVideoAcceleratorFactories> gpu_factories_;

  // Routes audio playback to either AudioRendererSink or WebAudio.
  scoped_refptr<WebAudioSourceProviderImpl> audio_source_provider_;

  bool supports_save_;

  // These two are mutually exclusive:
  //   |data_source_| is used for regular resource loads.
  //   |chunk_demuxer_| is used for Media Source resource loads.
  //
  // |demuxer_| will contain the appropriate demuxer based on which resource
  // load strategy we're using.
  scoped_ptr<BufferedDataSource> data_source_;
  scoped_ptr<media::Demuxer> demuxer_;
  media::ChunkDemuxer* chunk_demuxer_;

  BufferedDataSourceHostImpl buffered_data_source_host_;

  // Temporary for EME v0.1. In the future the init data type should be passed
  // through GenerateKeyRequest() directly from WebKit.
  std::string init_data_type_;

  // Video rendering members.
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  VideoFrameCompositor* compositor_;  // Deleted on |compositor_task_runner_|.
  media::SkCanvasVideoRenderer skcanvas_video_renderer_;

  // The compositor layer for displaying the video content when using composited
  // playback.
  scoped_ptr<WebLayerImpl> video_weblayer_;

  // Text track objects get a unique index value when they're created.
  int text_track_index_;

  // Manages decryption keys and decrypts encrypted frames.
  scoped_ptr<ProxyDecryptor> proxy_decryptor_;

  // Non-owned pointer to the CDM. Updated via calls to
  // setContentDecryptionModule().
  WebContentDecryptionModuleImpl* web_cdm_;

  media::DecryptorReadyCB decryptor_ready_cb_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_IMPL_H_
