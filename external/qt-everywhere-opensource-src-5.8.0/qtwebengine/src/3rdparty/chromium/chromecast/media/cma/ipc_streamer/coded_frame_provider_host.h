// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_IPC_STREAMER_CODED_FRAME_PROVIDER_HOST_H_
#define CHROMECAST_MEDIA_CMA_IPC_STREAMER_CODED_FRAME_PROVIDER_HOST_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"

namespace chromecast {
namespace media {
class MediaMessageFifo;

// CodedFrameProviderHost is a frame provider that gets the frames
// from a media message fifo.
class CodedFrameProviderHost : public CodedFrameProvider {
 public:
  // Note: if the media message fifo is located into shared memory,
  // the caller must make sure the shared memory segment is valid
  // during the whole lifetime of this object.
  explicit CodedFrameProviderHost(
      std::unique_ptr<MediaMessageFifo> media_message_fifo);
  ~CodedFrameProviderHost() override;

  // CodedFrameProvider implementation.
  void Read(const ReadCB& read_cb) override;
  void Flush(const base::Closure& flush_cb) override;

  // Invoked when some data has been written into the fifo.
  void OnFifoWriteEvent();
  base::Closure GetFifoWriteEventCb();

 private:
  void ReadMessages();

  base::ThreadChecker thread_checker_;

  // Fifo holding the frames.
  std::unique_ptr<MediaMessageFifo> fifo_;

  ReadCB read_cb_;

  // Audio/video configuration for the next A/V buffer.
  ::media::AudioDecoderConfig audio_config_;
  ::media::VideoDecoderConfig video_config_;

  base::WeakPtr<CodedFrameProviderHost> weak_this_;
  base::WeakPtrFactory<CodedFrameProviderHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CodedFrameProviderHost);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_IPC_STREAMER_CODED_FRAME_PROVIDER_HOST_H_
