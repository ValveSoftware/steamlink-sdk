// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_video_decoder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"

namespace media {

MojoVideoDecoder::MojoVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    mojom::VideoDecoderPtr remote_decoder)
    : task_runner_(task_runner),
      gpu_factories_(gpu_factories),
      remote_decoder_info_(remote_decoder.PassInterface()),
      client_binding_(this) {
  (void)gpu_factories_;
  DVLOG(1) << __FUNCTION__;
}

MojoVideoDecoder::~MojoVideoDecoder() {
  DVLOG(1) << __FUNCTION__;
  Stop();
}

std::string MojoVideoDecoder::GetDisplayName() const {
  // TODO(sandersd): Build the name including information from the remote end.
  return "MojoVideoDecoder";
}

void MojoVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                  bool low_delay,
                                  CdmContext* cdm_context,
                                  const InitCB& init_cb,
                                  const OutputCB& output_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!cdm_context);

  if (!remote_decoder_bound_)
    BindRemoteDecoder();

  if (has_connection_error_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, false));
    return;
  }

  initialized_ = false;
  init_cb_ = init_cb;
  output_cb_ = output_cb;
  remote_decoder_->Initialize(
      mojom::VideoDecoderConfig::From(config), low_delay,
      base::Bind(&MojoVideoDecoder::OnInitializeDone, base::Unretained(this)));
}

void MojoVideoDecoder::OnInitializeDone(bool status,
                                        bool needs_bitstream_conversion,
                                        int32_t max_decode_requests) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  initialized_ = status;
  needs_bitstream_conversion_ = needs_bitstream_conversion;
  max_decode_requests_ = max_decode_requests;
  base::ResetAndReturn(&init_cb_).Run(status);
}

void MojoVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                              const DecodeCB& decode_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (has_connection_error_) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(decode_cb, DecodeStatus::DECODE_ERROR));
    return;
  }

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(buffer);
  if (!mojo_buffer) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(decode_cb, DecodeStatus::DECODE_ERROR));
    return;
  }

  uint64_t decode_id = decode_counter_++;
  pending_decodes_[decode_id] = decode_cb;
  remote_decoder_->Decode(std::move(mojo_buffer),
                          base::Bind(&MojoVideoDecoder::OnDecodeDone,
                                     base::Unretained(this), decode_id));
}

void MojoVideoDecoder::OnVideoFrameDecoded(mojom::VideoFramePtr frame) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  output_cb_.Run(frame.To<scoped_refptr<VideoFrame>>());
}

void MojoVideoDecoder::OnDecodeDone(uint64_t decode_id, DecodeStatus status) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto it = pending_decodes_.find(decode_id);
  if (it == pending_decodes_.end()) {
    DLOG(ERROR) << "Decode request " << decode_id << " not found";
    Stop();
    return;
  }
  DecodeCB decode_cb = it->second;
  pending_decodes_.erase(it);
  decode_cb.Run(status);
}

void MojoVideoDecoder::Reset(const base::Closure& reset_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (has_connection_error_) {
    task_runner_->PostTask(FROM_HERE, reset_cb);
    return;
  }

  reset_cb_ = reset_cb;
  remote_decoder_->Reset(
      base::Bind(&MojoVideoDecoder::OnResetDone, base::Unretained(this)));
}

void MojoVideoDecoder::OnResetDone() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::ResetAndReturn(&reset_cb_).Run();
}

bool MojoVideoDecoder::NeedsBitstreamConversion() const {
  DVLOG(3) << __FUNCTION__;
  DCHECK(initialized_);
  return needs_bitstream_conversion_;
}

bool MojoVideoDecoder::CanReadWithoutStalling() const {
  DVLOG(3) << __FUNCTION__;
  return true;
}

int MojoVideoDecoder::GetMaxDecodeRequests() const {
  DVLOG(3) << __FUNCTION__;
  DCHECK(initialized_);
  return max_decode_requests_;
}

void MojoVideoDecoder::BindRemoteDecoder() {
  DVLOG(3) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!remote_decoder_bound_);

  remote_decoder_.Bind(std::move(remote_decoder_info_));
  remote_decoder_bound_ = true;

  remote_decoder_.set_connection_error_handler(
      base::Bind(&MojoVideoDecoder::Stop, base::Unretained(this)));

  // TODO(sandersd): Does this need its own error handler?
  mojom::VideoDecoderClientAssociatedPtrInfo client_ptr_info;
  client_binding_.Bind(&client_ptr_info, remote_decoder_.associated_group());

  // TODO(sandersd): Better buffer sizing.
  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  mojo_decoder_buffer_writer_ = MojoDecoderBufferWriter::Create(
      DemuxerStream::VIDEO, &remote_consumer_handle);

  remote_decoder_->Construct(std::move(client_ptr_info),
                             std::move(remote_consumer_handle));
}

void MojoVideoDecoder::Stop() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  has_connection_error_ = true;

  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(false);

  for (const auto& pending_decode : pending_decodes_)
    pending_decode.second.Run(DecodeStatus::DECODE_ERROR);
  pending_decodes_.clear();

  if (!reset_cb_.is_null())
    base::ResetAndReturn(&reset_cb_).Run();
}

}  // namespace media
