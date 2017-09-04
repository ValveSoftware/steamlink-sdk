// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_RENDERER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_RENDERER_SERVICE_H_

#include <stdint.h>
#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "media/base/buffering_state.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer_client.h"
#include "media/mojo/interfaces/renderer.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace media {

class AudioRendererSink;
class DemuxerStreamProviderShim;
class MediaKeys;
class MojoCdmServiceContext;
class Renderer;
class VideoRendererSink;

// A mojom::Renderer implementation that use a media::Renderer to render
// media streams.
class MEDIA_MOJO_EXPORT MojoRendererService
    : NON_EXPORTED_BASE(public mojom::Renderer),
      public RendererClient {
 public:
  using InitiateSurfaceRequestCB = base::Callback<base::UnguessableToken()>;

  // |mojo_cdm_service_context| can be used to find the CDM to support
  // encrypted media. If null, encrypted media is not supported.
  // NOTE: The MojoRendererService will be uniquely owned by a StrongBinding,
  // which is safely accessible via the returned StrongBindingPtr.
  static mojo::StrongBindingPtr<mojom::Renderer> Create(
      base::WeakPtr<MojoCdmServiceContext> mojo_cdm_service_context,
      scoped_refptr<AudioRendererSink> audio_sink,
      std::unique_ptr<VideoRendererSink> video_sink,
      std::unique_ptr<media::Renderer> renderer,
      InitiateSurfaceRequestCB initiate_surface_request_cb,
      mojo::InterfaceRequest<mojom::Renderer> request);

  ~MojoRendererService() final;

  // mojom::Renderer implementation.
  void Initialize(mojom::RendererClientAssociatedPtrInfo client,
                  mojom::DemuxerStreamPtr audio,
                  mojom::DemuxerStreamPtr video,
                  const base::Optional<GURL>& media_url,
                  const base::Optional<GURL>& first_party_for_cookies,
                  const InitializeCallback& callback) final;
  void Flush(const FlushCallback& callback) final;
  void StartPlayingFrom(base::TimeDelta time_delta) final;
  void SetPlaybackRate(double playback_rate) final;
  void SetVolume(float volume) final;
  void SetCdm(int32_t cdm_id, const SetCdmCallback& callback) final;
  void InitiateScopedSurfaceRequest(
      const InitiateScopedSurfaceRequestCallback& callback) final;

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_INITIALIZING,
    STATE_FLUSHING,
    STATE_PLAYING,
    STATE_ERROR
  };

  MojoRendererService(
      base::WeakPtr<MojoCdmServiceContext> mojo_cdm_service_context,
      scoped_refptr<AudioRendererSink> audio_sink,
      std::unique_ptr<VideoRendererSink> video_sink,
      std::unique_ptr<media::Renderer> renderer,
      InitiateSurfaceRequestCB initiate_surface_request_cb);

  // RendererClient implementation.
  void OnError(PipelineStatus status) final;
  void OnEnded() final;
  void OnStatisticsUpdate(const PipelineStatistics& stats) final;
  void OnBufferingStateChange(BufferingState state) final;
  void OnWaitingForDecryptionKey() final;
  void OnVideoNaturalSizeChange(const gfx::Size& size) final;
  void OnVideoOpacityChange(bool opaque) final;
  void OnDurationChange(base::TimeDelta duration) final;

  // Called when the DemuxerStreamProviderShim is ready to go (has a config,
  // pipe handle, etc) and can be handed off to a renderer for use.
  void OnStreamReady(const base::Callback<void(bool)>& callback);

  // Called when |audio_renderer_| initialization has completed.
  void OnRendererInitializeDone(const base::Callback<void(bool)>& callback,
                                PipelineStatus status);

  // Periodically polls the media time from the renderer and notifies the client
  // if the media time has changed since the last update.
  // If |force| is true, the client is notified even if the time is unchanged.
  // If |range| is true, an interpolation time range is reported.
  void UpdateMediaTime(bool force);
  void CancelPeriodicMediaTimeUpdates();
  void SchedulePeriodicMediaTimeUpdates();

  // Callback executed once Flush() completes.
  void OnFlushCompleted(const FlushCallback& callback);

  // Callback executed once SetCdm() completes.
  void OnCdmAttached(scoped_refptr<MediaKeys> cdm,
                     const base::Callback<void(bool)>& callback,
                     bool success);

  base::WeakPtr<MojoCdmServiceContext> mojo_cdm_service_context_;

  State state_;
  double playback_rate_;

  std::unique_ptr<DemuxerStreamProvider> stream_provider_;

  base::RepeatingTimer time_update_timer_;
  base::TimeDelta last_media_time_;

  mojom::RendererClientAssociatedPtr client_;

  // Hold a reference to the CDM set on the |renderer_| so that the CDM won't be
  // destructed while the |renderer_| is still using it.
  scoped_refptr<MediaKeys> cdm_;

  // Audio and Video sinks.
  // May be null if underlying |renderer_| does not use them.
  scoped_refptr<AudioRendererSink> audio_sink_;
  std::unique_ptr<VideoRendererSink> video_sink_;

  // Note: Destroy |renderer_| first to avoid access violation into other
  // members, e.g. |stream_provider_|, |cdm_|, |audio_sink_|, and
  // |video_sink_|.
  // Must use "media::" because "Renderer" is ambiguous.
  std::unique_ptr<media::Renderer> renderer_;

  // Registers a new request in the ScopedSurfaceRequestManager.
  // Returns the token to be used to fulfill the request.
  InitiateSurfaceRequestCB initiate_surface_request_cb_;

  // WeakPtr to the binding that owns |this|.
  // Used to forcefully close the connection (which also safely destroy |this|).
  mojo::StrongBindingPtr<mojom::Renderer> binding_;

  base::WeakPtr<MojoRendererService> weak_this_;
  base::WeakPtrFactory<MojoRendererService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoRendererService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_RENDERER_SERVICE_H_
