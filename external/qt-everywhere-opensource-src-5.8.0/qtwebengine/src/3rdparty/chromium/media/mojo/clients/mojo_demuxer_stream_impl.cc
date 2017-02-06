// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_demuxer_stream_impl.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"

namespace media {

MojoDemuxerStreamImpl::MojoDemuxerStreamImpl(
    media::DemuxerStream* stream,
    mojo::InterfaceRequest<mojom::DemuxerStream> request)
    : binding_(this, std::move(request)),
      stream_(stream),
      weak_factory_(this) {}

MojoDemuxerStreamImpl::~MojoDemuxerStreamImpl() {}

// This is called when our DemuxerStreamClient has connected itself and is
// ready to receive messages.  Send an initial config and notify it that
// we are now ready for business.
void MojoDemuxerStreamImpl::Initialize(const InitializeCallback& callback) {
  DVLOG(2) << __FUNCTION__;

  // Prepare the initial config.
  mojom::AudioDecoderConfigPtr audio_config;
  mojom::VideoDecoderConfigPtr video_config;
  if (stream_->type() == media::DemuxerStream::AUDIO) {
    audio_config =
        mojom::AudioDecoderConfig::From(stream_->audio_decoder_config());
  } else if (stream_->type() == media::DemuxerStream::VIDEO) {
    video_config =
        mojom::VideoDecoderConfig::From(stream_->video_decoder_config());
  } else {
    NOTREACHED() << "Unsupported stream type: " << stream_->type();
    return;
  }

  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  mojo_decoder_buffer_writer_ =
      MojoDecoderBufferWriter::Create(stream_->type(), &remote_consumer_handle);

  callback.Run(static_cast<mojom::DemuxerStream::Type>(stream_->type()),
               std::move(remote_consumer_handle), std::move(audio_config),
               std::move(video_config));
}

void MojoDemuxerStreamImpl::Read(const ReadCallback& callback) {
  stream_->Read(base::Bind(&MojoDemuxerStreamImpl::OnBufferReady,
                           weak_factory_.GetWeakPtr(), callback));
}

void MojoDemuxerStreamImpl::EnableBitstreamConverter() {
  stream_->EnableBitstreamConverter();
}

void MojoDemuxerStreamImpl::OnBufferReady(
    const ReadCallback& callback,
    media::DemuxerStream::Status status,
    const scoped_refptr<media::DecoderBuffer>& buffer) {
  mojom::AudioDecoderConfigPtr audio_config;
  mojom::VideoDecoderConfigPtr video_config;

  if (status == media::DemuxerStream::kConfigChanged) {
    DVLOG(2) << __FUNCTION__ << ": ConfigChange!";
    // Send the config change so our client can read it once it parses the
    // Status obtained via Run() below.
    if (stream_->type() == media::DemuxerStream::AUDIO) {
      audio_config =
          mojom::AudioDecoderConfig::From(stream_->audio_decoder_config());
    } else if (stream_->type() == media::DemuxerStream::VIDEO) {
      video_config =
          mojom::VideoDecoderConfig::From(stream_->video_decoder_config());
    } else {
      NOTREACHED() << "Unsupported config change encountered for type: "
                   << stream_->type();
    }

    callback.Run(mojom::DemuxerStream::Status::CONFIG_CHANGED,
                 mojom::DecoderBufferPtr(), std::move(audio_config),
                 std::move(video_config));
    return;
  }

  if (status == media::DemuxerStream::kAborted) {
    callback.Run(mojom::DemuxerStream::Status::ABORTED,
                 mojom::DecoderBufferPtr(), std::move(audio_config),
                 std::move(video_config));
    return;
  }

  DCHECK_EQ(status, media::DemuxerStream::kOk);

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(buffer);
  if (!mojo_buffer) {
    callback.Run(mojom::DemuxerStream::Status::ABORTED,
                 mojom::DecoderBufferPtr(), std::move(audio_config),
                 std::move(video_config));
    return;
  }

  // TODO(dalecurtis): Once we can write framed data to the DataPipe, fill via
  // the producer handle and then read more to keep the pipe full.  Waiting for
  // space can be accomplished using an AsyncWaiter.
  callback.Run(static_cast<mojom::DemuxerStream::Status>(status),
               std::move(mojo_buffer), std::move(audio_config),
               std::move(video_config));
}

}  // namespace media
