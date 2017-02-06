// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_IPC_STREAMER_AV_STREAMER_PROXY_H_
#define CHROMECAST_MEDIA_CMA_IPC_STREAMER_AV_STREAMER_PROXY_H_

#include <list>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"

namespace chromecast {
namespace media {
class CodedFrameProvider;
class DecoderBufferBase;
class MediaMessageFifo;

// AvStreamerProxy streams audio/video data from a coded frame provider
// to a media message fifo.
class AvStreamerProxy {
 public:
  AvStreamerProxy();
  ~AvStreamerProxy();

  // AvStreamerProxy gets frames and audio/video config
  // from the |frame_provider| and feed them into the |fifo|.
  void SetCodedFrameProvider(
      std::unique_ptr<CodedFrameProvider> frame_provider);
  void SetMediaMessageFifo(std::unique_ptr<MediaMessageFifo> fifo);

  // Starts fetching A/V buffers.
  void Start();

  // Stops fetching A/V buffers and flush the pending buffers,
  // invoking |flush_cb| when done.
  void StopAndFlush(const base::Closure& flush_cb);

  // Event invoked when some data have been read from the fifo.
  // This means some data can now possibly be written into the fifo.
  void OnFifoReadEvent();

 private:
  void RequestBufferIfNeeded();

  void OnNewBuffer(const scoped_refptr<DecoderBufferBase>& buffer,
                   const ::media::AudioDecoderConfig& audio_config,
                   const ::media::VideoDecoderConfig& video_config);

  void ProcessPendingData();

  bool SendAudioDecoderConfig(const ::media::AudioDecoderConfig& config);
  bool SendVideoDecoderConfig(const ::media::VideoDecoderConfig& config);
  bool SendBuffer(const scoped_refptr<DecoderBufferBase>& buffer);

  void OnStopAndFlushDone();

  base::ThreadChecker thread_checker_;

  std::unique_ptr<CodedFrameProvider> frame_provider_;

  // Fifo used to convey A/V configs and buffers.
  std::unique_ptr<MediaMessageFifo> fifo_;

  // State.
  bool is_running_;

  // Indicates if there is a pending request to the coded frame provider.
  bool pending_read_;

  // Pending config & buffer.
  // |pending_av_data_| is set as long as one of the pending audio/video
  // config or buffer is valid.
  bool pending_av_data_;
  ::media::AudioDecoderConfig pending_audio_config_;
  ::media::VideoDecoderConfig pending_video_config_;
  scoped_refptr<DecoderBufferBase> pending_buffer_;

  // List of pending callbacks in StopAndFlush
  std::list<base::Closure> pending_stop_flush_cb_list_;

  base::WeakPtr<AvStreamerProxy> weak_this_;
  base::WeakPtrFactory<AvStreamerProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AvStreamerProxy);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_IPC_STREAMER_AV_STREAMER_PROXY_H_
