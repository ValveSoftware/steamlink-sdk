// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/media_gpu_channel.h"

#include "base/unguessable_token.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "ipc/message_filter.h"
#include "media/gpu/ipc/common/media_messages.h"
#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_encode_accelerator.h"

namespace media {

namespace {

void SendCreateJpegDecoderResult(
    std::unique_ptr<IPC::Message> reply_message,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::WeakPtr<gpu::GpuChannel> channel,
    scoped_refptr<gpu::GpuChannelMessageFilter> filter,
    bool result) {
  GpuChannelMsg_CreateJpegDecoder::WriteReplyParams(reply_message.get(),
                                                    result);
  if (io_task_runner->BelongsToCurrentThread()) {
    filter->Send(reply_message.release());
  } else if (channel) {
    channel->Send(reply_message.release());
  }
}

}  // namespace

class MediaGpuChannelDispatchHelper {
 public:
  MediaGpuChannelDispatchHelper(MediaGpuChannel* channel, int32_t routing_id)
      : channel_(channel), routing_id_(routing_id) {}

  bool Send(IPC::Message* msg) { return channel_->Send(msg); }

  void OnCreateVideoDecoder(const VideoDecodeAccelerator::Config& config,
                            int32_t decoder_route_id,
                            IPC::Message* reply_message) {
    channel_->OnCreateVideoDecoder(routing_id_, config, decoder_route_id,
                                   reply_message);
  }

  void OnCreateVideoEncoder(const CreateVideoEncoderParams& params,
                            IPC::Message* reply_message) {
    channel_->OnCreateVideoEncoder(routing_id_, params, reply_message);
  }

 private:
  MediaGpuChannel* const channel_;
  const int32_t routing_id_;
  DISALLOW_COPY_AND_ASSIGN(MediaGpuChannelDispatchHelper);
};

// Filter to respond to GetChannelToken on the IO thread.
class MediaGpuChannelFilter : public IPC::MessageFilter {
 public:
  explicit MediaGpuChannelFilter(const base::UnguessableToken& channel_token)
      : channel_token_(channel_token) {}

  void OnFilterAdded(IPC::Channel* channel) override { channel_ = channel; }
  bool Send(IPC::Message* msg) { return channel_->Send(msg); }

  bool OnMessageReceived(const IPC::Message& msg) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MediaGpuChannelFilter, msg)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetChannelToken,
                                      OnGetChannelToken)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnGetChannelToken(IPC::Message* reply_message) {
    GpuCommandBufferMsg_GetChannelToken::WriteReplyParams(reply_message,
                                                          channel_token_);
    Send(reply_message);
  }

 private:
  ~MediaGpuChannelFilter() override {}

  IPC::Channel* channel_;
  base::UnguessableToken channel_token_;
};

MediaGpuChannel::MediaGpuChannel(gpu::GpuChannel* channel) : channel_(channel) {
  channel_->AddFilter(
      new MediaGpuChannelFilter(base::UnguessableToken::Create()));
}

MediaGpuChannel::~MediaGpuChannel() {}

bool MediaGpuChannel::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

bool MediaGpuChannel::OnMessageReceived(const IPC::Message& message) {
  MediaGpuChannelDispatchHelper helper(this, message.routing_id());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaGpuChannel, message)
    IPC_MESSAGE_FORWARD_DELAY_REPLY(
        GpuCommandBufferMsg_CreateVideoDecoder, &helper,
        MediaGpuChannelDispatchHelper::OnCreateVideoDecoder)
    IPC_MESSAGE_FORWARD_DELAY_REPLY(
        GpuCommandBufferMsg_CreateVideoEncoder, &helper,
        MediaGpuChannelDispatchHelper::OnCreateVideoEncoder)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuChannelMsg_CreateJpegDecoder,
                                    OnCreateJpegDecoder)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaGpuChannel::OnCreateJpegDecoder(int32_t route_id,
                                          IPC::Message* reply_msg) {
  std::unique_ptr<IPC::Message> msg(reply_msg);
  if (!jpeg_decoder_) {
    jpeg_decoder_.reset(
        new GpuJpegDecodeAccelerator(channel_, channel_->io_task_runner()));
  }
  jpeg_decoder_->AddClient(
      route_id, base::Bind(&SendCreateJpegDecoderResult, base::Passed(&msg),
                           channel_->io_task_runner(), channel_->AsWeakPtr(),
                           make_scoped_refptr(channel_->filter())));
}

void MediaGpuChannel::OnCreateVideoDecoder(
    int32_t command_buffer_route_id,
    const VideoDecodeAccelerator::Config& config,
    int32_t decoder_route_id,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "MediaGpuChannel::OnCreateVideoDecoder");
  gpu::GpuCommandBufferStub* stub =
      channel_->LookupCommandBuffer(command_buffer_route_id);
  if (!stub) {
    reply_message->set_reply_error();
    Send(reply_message);
    return;
  }
  GpuVideoDecodeAccelerator* decoder = new GpuVideoDecodeAccelerator(
      decoder_route_id, stub, stub->channel()->io_task_runner());
  bool succeeded = decoder->Initialize(config);
  GpuCommandBufferMsg_CreateVideoDecoder::WriteReplyParams(reply_message,
                                                           succeeded);
  Send(reply_message);

  // decoder is registered as a DestructionObserver of this stub and will
  // self-delete during destruction of this stub.
}

void MediaGpuChannel::OnCreateVideoEncoder(
    int32_t command_buffer_route_id,
    const CreateVideoEncoderParams& params,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "MediaGpuChannel::OnCreateVideoEncoder");
  gpu::GpuCommandBufferStub* stub =
      channel_->LookupCommandBuffer(command_buffer_route_id);
  if (!stub) {
    reply_message->set_reply_error();
    Send(reply_message);
    return;
  }
  GpuVideoEncodeAccelerator* encoder = new GpuVideoEncodeAccelerator(
      params.encoder_route_id, stub, stub->channel()->io_task_runner());
  bool succeeded =
      encoder->Initialize(params.input_format, params.input_visible_size,
                          params.output_profile, params.initial_bitrate);
  GpuCommandBufferMsg_CreateVideoEncoder::WriteReplyParams(reply_message,
                                                           succeeded);
  Send(reply_message);

  // encoder is registered as a DestructionObserver of this stub and will
  // self-delete during destruction of this stub.
}

}  // namespace media
