// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/ipc_streamer/coded_frame_provider_host.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/ipc/media_message.h"
#include "chromecast/media/cma/ipc/media_message_fifo.h"
#include "chromecast/media/cma/ipc/media_message_type.h"
#include "chromecast/media/cma/ipc_streamer/audio_decoder_config_marshaller.h"
#include "chromecast/media/cma/ipc_streamer/decoder_buffer_base_marshaller.h"
#include "chromecast/media/cma/ipc_streamer/video_decoder_config_marshaller.h"
#include "media/base/decrypt_config.h"

namespace chromecast {
namespace media {

CodedFrameProviderHost::CodedFrameProviderHost(
    std::unique_ptr<MediaMessageFifo> media_message_fifo)
    : fifo_(std::move(media_message_fifo)), weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

CodedFrameProviderHost::~CodedFrameProviderHost() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void CodedFrameProviderHost::Read(const ReadCB& read_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Cannot be called if there is already a pending read.
  DCHECK(read_cb_.is_null());
  read_cb_ = read_cb;

  ReadMessages();
}

void CodedFrameProviderHost::Flush(const base::Closure& flush_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  audio_config_ = ::media::AudioDecoderConfig();
  video_config_ = ::media::VideoDecoderConfig();
  read_cb_.Reset();
  fifo_->Flush();
  flush_cb.Run();
}

void CodedFrameProviderHost::OnFifoWriteEvent() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ReadMessages();
}

base::Closure CodedFrameProviderHost::GetFifoWriteEventCb() {
  return base::Bind(&CodedFrameProviderHost::OnFifoWriteEvent, weak_this_);
}

void CodedFrameProviderHost::ReadMessages() {
  // Read messages until a frame is provided (i.e. not just the audio/video
  // configurations).
  while (!read_cb_.is_null()) {
    std::unique_ptr<MediaMessage> msg(fifo_->Pop());
    if (!msg)
      break;

    if (msg->type() == PaddingMediaMsg) {
      // Ignore the message.
    } else if (msg->type() == AudioConfigMediaMsg) {
      audio_config_ = AudioDecoderConfigMarshaller::Read(msg.get());
    } else if (msg->type() == VideoConfigMediaMsg) {
      video_config_ = VideoDecoderConfigMarshaller::Read(msg.get());
    } else if (msg->type() == FrameMediaMsg) {
      scoped_refptr<DecoderBufferBase> buffer =
          DecoderBufferBaseMarshaller::Read(std::move(msg));
      base::ResetAndReturn(&read_cb_).Run(
          buffer, audio_config_, video_config_);
      audio_config_ = ::media::AudioDecoderConfig();
      video_config_ = ::media::VideoDecoderConfig();
    } else {
      // Receiving an unexpected message.
      // Possible use case (except software bugs): the renderer process has
      // been compromised and an invalid message value has been written to
      // the fifo. Crash the browser process in this case to avoid further
      // security implications (so do not use NOTREACHED which crashes only
      // in debug builds).
      LOG(FATAL) << "Unknown media message";
    }
  }
}

}  // namespace media
}  // namespace chromecast
