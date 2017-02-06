// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/ipc_streamer/av_streamer_proxy.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/ipc/media_memory_chunk.h"
#include "chromecast/media/cma/ipc/media_message.h"
#include "chromecast/media/cma/ipc/media_message_fifo.h"
#include "chromecast/media/cma/ipc/media_message_type.h"
#include "chromecast/media/cma/ipc_streamer/audio_decoder_config_marshaller.h"
#include "chromecast/media/cma/ipc_streamer/decoder_buffer_base_marshaller.h"
#include "chromecast/media/cma/ipc_streamer/video_decoder_config_marshaller.h"

namespace chromecast {
namespace media {

AvStreamerProxy::AvStreamerProxy()
    : is_running_(false),
      pending_read_(false),
      pending_av_data_(false),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

AvStreamerProxy::~AvStreamerProxy() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void AvStreamerProxy::SetCodedFrameProvider(
    std::unique_ptr<CodedFrameProvider> frame_provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!frame_provider_);
  frame_provider_.reset(frame_provider.release());
}

void AvStreamerProxy::SetMediaMessageFifo(
    std::unique_ptr<MediaMessageFifo> fifo) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!fifo_);
  fifo_.reset(fifo.release());
}

void AvStreamerProxy::Start() {
  DCHECK(!is_running_);

  is_running_ = true;
  RequestBufferIfNeeded();
}

void AvStreamerProxy::StopAndFlush(const base::Closure& done_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!done_cb.is_null());

  pending_av_data_ = false;
  pending_audio_config_ = ::media::AudioDecoderConfig();
  pending_video_config_ = ::media::VideoDecoderConfig();
  pending_buffer_ = scoped_refptr<DecoderBufferBase>();

  pending_read_ = false;
  is_running_ = false;

  // If there's another pending Flush, for example, the pipeline is stopped
  // while another seek is pending, then we don't need to call Flush again. Save
  // the callback and fire it later when Flush is done.
  pending_stop_flush_cb_list_.push_back(done_cb);
  if (pending_stop_flush_cb_list_.size() == 1) {
    frame_provider_->Flush(
        base::Bind(&AvStreamerProxy::OnStopAndFlushDone, weak_this_));
  }
}

void AvStreamerProxy::OnStopAndFlushDone() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Flush is done. Fire all the "flush done" callbacks in order. This is
  // necessary to guarantee proper state transition in pipeline.
  for (const auto& cb : pending_stop_flush_cb_list_) {
    cb.Run();
  }
  pending_stop_flush_cb_list_.clear();
}

void AvStreamerProxy::OnFifoReadEvent() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Some enough space might have been released
  // to accommodate the pending data.
  if (pending_av_data_)
    ProcessPendingData();
}

void AvStreamerProxy::RequestBufferIfNeeded() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!is_running_ || pending_read_ || pending_av_data_)
    return;

  // |frame_provider_| is assumed to run on the same message loop.
  // Add a BindToCurrentLoop if that's not the case in the future.
  pending_read_ = true;
  frame_provider_->Read(base::Bind(&AvStreamerProxy::OnNewBuffer, weak_this_));
}

void AvStreamerProxy::OnNewBuffer(
    const scoped_refptr<DecoderBufferBase>& buffer,
    const ::media::AudioDecoderConfig& audio_config,
    const ::media::VideoDecoderConfig& video_config) {
  DCHECK(thread_checker_.CalledOnValidThread());

  pending_read_ = false;

  if (buffer->end_of_stream())
    is_running_ = false;

  DCHECK(!pending_av_data_);
  pending_av_data_ = true;

  pending_buffer_ = buffer;
  pending_audio_config_ = audio_config;
  pending_video_config_ = video_config;

  ProcessPendingData();
}

void AvStreamerProxy::ProcessPendingData() {
  if (pending_audio_config_.IsValidConfig()) {
    if (!SendAudioDecoderConfig(pending_audio_config_))
      return;
    pending_audio_config_ = ::media::AudioDecoderConfig();
  }

  if (pending_video_config_.IsValidConfig()) {
    if (!SendVideoDecoderConfig(pending_video_config_))
      return;
    pending_video_config_ = ::media::VideoDecoderConfig();
  }

  if (pending_buffer_.get()) {
    if (!SendBuffer(pending_buffer_))
      return;
    pending_buffer_ = scoped_refptr<DecoderBufferBase>();
  }

  pending_av_data_ = false;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&AvStreamerProxy::RequestBufferIfNeeded, weak_this_));
}

bool AvStreamerProxy::SendAudioDecoderConfig(
    const ::media::AudioDecoderConfig& config) {
  // Create a dummy message to calculate first the message size.
  std::unique_ptr<MediaMessage> dummy_msg(
      MediaMessage::CreateDummyMessage(AudioConfigMediaMsg));
  AudioDecoderConfigMarshaller::Write(config, dummy_msg.get());

  // Create the real message and write the actual content.
  std::unique_ptr<MediaMessage> msg(MediaMessage::CreateMessage(
      AudioConfigMediaMsg, base::Bind(&MediaMessageFifo::ReserveMemory,
                                      base::Unretained(fifo_.get())),
      dummy_msg->content_size()));
  if (!msg)
    return false;

  AudioDecoderConfigMarshaller::Write(config, msg.get());
  return true;
}

bool AvStreamerProxy::SendVideoDecoderConfig(
    const ::media::VideoDecoderConfig& config) {
  // Create a dummy message to calculate first the message size.
  std::unique_ptr<MediaMessage> dummy_msg(
      MediaMessage::CreateDummyMessage(VideoConfigMediaMsg));
  VideoDecoderConfigMarshaller::Write(config, dummy_msg.get());

  // Create the real message and write the actual content.
  std::unique_ptr<MediaMessage> msg(MediaMessage::CreateMessage(
      VideoConfigMediaMsg, base::Bind(&MediaMessageFifo::ReserveMemory,
                                      base::Unretained(fifo_.get())),
      dummy_msg->content_size()));
  if (!msg)
    return false;

  VideoDecoderConfigMarshaller::Write(config, msg.get());
  return true;
}

bool AvStreamerProxy::SendBuffer(
    const scoped_refptr<DecoderBufferBase>& buffer) {
  // Create a dummy message to calculate first the message size.
  std::unique_ptr<MediaMessage> dummy_msg(
      MediaMessage::CreateDummyMessage(FrameMediaMsg));
  DecoderBufferBaseMarshaller::Write(buffer, dummy_msg.get());

  // Create the real message and write the actual content.
  std::unique_ptr<MediaMessage> msg(MediaMessage::CreateMessage(
      FrameMediaMsg, base::Bind(&MediaMessageFifo::ReserveMemory,
                                base::Unretained(fifo_.get())),
      dummy_msg->content_size()));
  if (!msg)
    return false;

  DecoderBufferBaseMarshaller::Write(buffer, msg.get());
  return true;
}

}  // namespace media
}  // namespace chromecast
