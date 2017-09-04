// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_RENDERER_CONTROLLER_H_
#define MEDIA_REMOTING_REMOTING_RENDERER_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/base/media_observer.h"
#include "media/remoting/remoting_source_impl.h"

namespace media {

namespace remoting {
class RpcBroker;
}

// This class:
// 1) Implements the RemotingSourceImpl::Client;
// 2) Monitors player events as a MediaObserver;
// 3) May trigger the switch of the media renderer between local playback
// and remoting.
class RemotingRendererController final : public RemotingSourceImpl::Client,
                                         public MediaObserver {
 public:
  explicit RemotingRendererController(
      scoped_refptr<RemotingSourceImpl> remoting_source);
  ~RemotingRendererController() override;

  // RemotingSourceImpl::Client implemenations.
  void OnStarted(bool success) override;
  void OnSessionStateChanged() override;

  // MediaObserver implementations.
  void OnEnteredFullscreen() override;
  void OnExitedFullscreen() override;
  void OnSetCdm(CdmContext* cdm_context) override;
  void OnMetadataChanged(const PipelineMetadata& metadata) override;

  void SetSwitchRendererCallback(const base::Closure& cb);

  base::WeakPtr<RemotingRendererController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Used by RemotingRendererFactory to query whether to create Media Remoting
  // Renderer.
  bool remote_rendering_started() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return remote_rendering_started_;
  }

  void StartDataPipe(
      std::unique_ptr<mojo::DataPipe> audio_data_pipe,
      std::unique_ptr<mojo::DataPipe> video_data_pipe,
      const RemotingSourceImpl::DataPipeStartCallback& done_callback);

  // Used by RemotingRendererImpl to query the session state.
  RemotingSourceImpl* remoting_source() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return remoting_source_.get();
  }

  base::WeakPtr<remoting::RpcBroker> GetRpcBroker() const;

  PipelineMetadata pipeline_metadata() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return pipeline_metadata_;
  }

 private:
  bool has_audio() const {
    return pipeline_metadata_.has_audio &&
           pipeline_metadata_.audio_decoder_config.IsValidConfig();
  }

  bool has_video() const {
    return pipeline_metadata_.has_video &&
           pipeline_metadata_.video_decoder_config.IsValidConfig();
  }

  bool IsVideoCodecSupported();
  bool IsAudioCodecSupported();

  // Helper to decide whether to enter or leave Remoting mode.
  bool ShouldBeRemoting();

  // Determines whether to enter or leave Remoting mode and switches if
  // necessary.
  void UpdateAndMaybeSwitch();

  // Indicates whether this media element or its ancestor is in full screen.
  bool is_fullscreen_ = false;

  // Indicates whether remoting is started.
  bool remote_rendering_started_ = false;

  // Indicates whether audio or video is encrypted.
  bool is_encrypted_ = false;

  // Current audio/video config.
  VideoDecoderConfig video_decoder_config_;
  AudioDecoderConfig audio_decoder_config_;

  // The callback to switch the media renderer.
  base::Closure switch_renderer_cb_;

  // This is initially the RemotingSourceImpl passed to the ctor, and might be
  // replaced with a different instance later if OnSetCdm() is called.
  scoped_refptr<RemotingSourceImpl> remoting_source_;

  // This is used to check all the methods are called on the current thread in
  // debug builds.
  base::ThreadChecker thread_checker_;

  PipelineMetadata pipeline_metadata_;

  base::WeakPtrFactory<RemotingRendererController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemotingRendererController);
};

}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_RENDERER_CONTROLLER_H_
