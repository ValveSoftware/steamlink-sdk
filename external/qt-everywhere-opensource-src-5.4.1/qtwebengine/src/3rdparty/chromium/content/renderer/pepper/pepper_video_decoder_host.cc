// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_video_decoder_host.h"

#include "base/bind.h"
#include "base/memory/shared_memory.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/ppb_graphics_3d_impl.h"
#include "content/renderer/pepper/video_decoder_shim.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/video_decoder_constants.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_graphics_3d_api.h"

using ppapi::proxy::SerializedHandle;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics3D_API;

namespace content {

namespace {

media::VideoCodecProfile PepperToMediaVideoProfile(PP_VideoProfile profile) {
  switch (profile) {
    case PP_VIDEOPROFILE_H264BASELINE:
      return media::H264PROFILE_BASELINE;
    case PP_VIDEOPROFILE_H264MAIN:
      return media::H264PROFILE_MAIN;
    case PP_VIDEOPROFILE_H264EXTENDED:
      return media::H264PROFILE_EXTENDED;
    case PP_VIDEOPROFILE_H264HIGH:
      return media::H264PROFILE_HIGH;
    case PP_VIDEOPROFILE_H264HIGH10PROFILE:
      return media::H264PROFILE_HIGH10PROFILE;
    case PP_VIDEOPROFILE_H264HIGH422PROFILE:
      return media::H264PROFILE_HIGH422PROFILE;
    case PP_VIDEOPROFILE_H264HIGH444PREDICTIVEPROFILE:
      return media::H264PROFILE_HIGH444PREDICTIVEPROFILE;
    case PP_VIDEOPROFILE_H264SCALABLEBASELINE:
      return media::H264PROFILE_SCALABLEBASELINE;
    case PP_VIDEOPROFILE_H264SCALABLEHIGH:
      return media::H264PROFILE_SCALABLEHIGH;
    case PP_VIDEOPROFILE_H264STEREOHIGH:
      return media::H264PROFILE_STEREOHIGH;
    case PP_VIDEOPROFILE_H264MULTIVIEWHIGH:
      return media::H264PROFILE_MULTIVIEWHIGH;
    case PP_VIDEOPROFILE_VP8MAIN:
      return media::VP8PROFILE_MAIN;
    case PP_VIDEOPROFILE_VP9MAIN:
      return media::VP9PROFILE_MAIN;
    // No default case, to catch unhandled PP_VideoProfile values.
  }

  return media::VIDEO_CODEC_PROFILE_UNKNOWN;
}

}  // namespace

PepperVideoDecoderHost::PendingDecode::PendingDecode(
    uint32_t shm_id,
    const ppapi::host::ReplyMessageContext& reply_context)
    : shm_id(shm_id), reply_context(reply_context) {
}

PepperVideoDecoderHost::PendingDecode::~PendingDecode() {
}

PepperVideoDecoderHost::PepperVideoDecoderHost(RendererPpapiHost* host,
                                               PP_Instance instance,
                                               PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      initialized_(false) {
}

PepperVideoDecoderHost::~PepperVideoDecoderHost() {
}

int32_t PepperVideoDecoderHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperVideoDecoderHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoDecoder_Initialize,
                                      OnHostMsgInitialize)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoDecoder_GetShm,
                                      OnHostMsgGetShm)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoDecoder_Decode,
                                      OnHostMsgDecode)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoDecoder_AssignTextures,
                                      OnHostMsgAssignTextures)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoDecoder_RecyclePicture,
                                      OnHostMsgRecyclePicture)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_VideoDecoder_Flush,
                                        OnHostMsgFlush)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_VideoDecoder_Reset,
                                        OnHostMsgReset)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperVideoDecoderHost::OnHostMsgInitialize(
    ppapi::host::HostMessageContext* context,
    const ppapi::HostResource& graphics_context,
    PP_VideoProfile profile,
    bool allow_software_fallback) {
  if (initialized_)
    return PP_ERROR_FAILED;

  EnterResourceNoLock<PPB_Graphics3D_API> enter_graphics(
      graphics_context.host_resource(), true);
  if (enter_graphics.failed())
    return PP_ERROR_FAILED;
  PPB_Graphics3D_Impl* graphics3d =
      static_cast<PPB_Graphics3D_Impl*>(enter_graphics.object());

  int command_buffer_route_id = graphics3d->GetCommandBufferRouteId();
  if (!command_buffer_route_id)
    return PP_ERROR_FAILED;

  media::VideoCodecProfile media_profile = PepperToMediaVideoProfile(profile);

  // This is not synchronous, but subsequent IPC messages will be buffered, so
  // it is okay to immediately send IPC messages through the returned channel.
  GpuChannelHost* channel = graphics3d->channel();
  DCHECK(channel);
  decoder_ = channel->CreateVideoDecoder(command_buffer_route_id);
  if (decoder_ && decoder_->Initialize(media_profile, this)) {
    initialized_ = true;
    return PP_OK;
  }
  decoder_.reset();

  if (!allow_software_fallback)
    return PP_ERROR_NOTSUPPORTED;

  decoder_.reset(new VideoDecoderShim(this));
  initialize_reply_context_ = context->MakeReplyMessageContext();
  decoder_->Initialize(media_profile, this);

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoDecoderHost::OnHostMsgGetShm(
    ppapi::host::HostMessageContext* context,
    uint32_t shm_id,
    uint32_t shm_size) {
  if (!initialized_)
    return PP_ERROR_FAILED;

  // Make the buffers larger since we hope to reuse them.
  shm_size = std::max(
      shm_size,
      static_cast<uint32_t>(ppapi::proxy::kMinimumBitstreamBufferSize));
  if (shm_size > ppapi::proxy::kMaximumBitstreamBufferSize)
    return PP_ERROR_FAILED;

  if (shm_id >= ppapi::proxy::kMaximumPendingDecodes)
    return PP_ERROR_FAILED;
  // The shm_id must be inside or at the end of shm_buffers_.
  if (shm_id > shm_buffers_.size())
    return PP_ERROR_FAILED;
  // Reject an attempt to reallocate a busy shm buffer.
  if (shm_id < shm_buffers_.size() && shm_buffer_busy_[shm_id])
    return PP_ERROR_FAILED;

  content::RenderThread* render_thread = content::RenderThread::Get();
  scoped_ptr<base::SharedMemory> shm(
      render_thread->HostAllocateSharedMemoryBuffer(shm_size).Pass());
  if (!shm || !shm->Map(shm_size))
    return PP_ERROR_FAILED;

  base::SharedMemoryHandle shm_handle = shm->handle();
  if (shm_id == shm_buffers_.size()) {
    shm_buffers_.push_back(shm.release());
    shm_buffer_busy_.push_back(false);
  } else {
    // Remove the old buffer. Delete manually since ScopedVector won't delete
    // the existing element if we just assign over it.
    delete shm_buffers_[shm_id];
    shm_buffers_[shm_id] = shm.release();
  }

#if defined(OS_WIN)
  base::PlatformFile platform_file = shm_handle;
#elif defined(OS_POSIX)
  base::PlatformFile platform_file = shm_handle.fd;
#else
#error Not implemented.
#endif
  SerializedHandle handle(
      renderer_ppapi_host_->ShareHandleWithRemote(platform_file, false),
      shm_size);
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  reply_context.params.AppendHandle(handle);
  host()->SendReply(reply_context,
                    PpapiPluginMsg_VideoDecoder_GetShmReply(shm_size));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoDecoderHost::OnHostMsgDecode(
    ppapi::host::HostMessageContext* context,
    uint32_t shm_id,
    uint32_t size,
    int32_t decode_id) {
  if (!initialized_)
    return PP_ERROR_FAILED;
  DCHECK(decoder_);
  // |shm_id| is just an index into shm_buffers_. Make sure it's in range.
  if (static_cast<size_t>(shm_id) >= shm_buffers_.size())
    return PP_ERROR_FAILED;
  // Reject an attempt to pass a busy buffer to the decoder again.
  if (shm_buffer_busy_[shm_id])
    return PP_ERROR_FAILED;
  // Reject non-unique decode_id values.
  if (pending_decodes_.find(decode_id) != pending_decodes_.end())
    return PP_ERROR_FAILED;

  if (flush_reply_context_.is_valid() || reset_reply_context_.is_valid())
    return PP_ERROR_FAILED;

  pending_decodes_.insert(std::make_pair(
      decode_id, PendingDecode(shm_id, context->MakeReplyMessageContext())));

  shm_buffer_busy_[shm_id] = true;
  decoder_->Decode(
      media::BitstreamBuffer(decode_id, shm_buffers_[shm_id]->handle(), size));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoDecoderHost::OnHostMsgAssignTextures(
    ppapi::host::HostMessageContext* context,
    const PP_Size& size,
    const std::vector<uint32_t>& texture_ids) {
  if (!initialized_)
    return PP_ERROR_FAILED;
  DCHECK(decoder_);

  std::vector<media::PictureBuffer> picture_buffers;
  for (uint32 i = 0; i < texture_ids.size(); i++) {
    media::PictureBuffer buffer(
        texture_ids[i],  // Use the texture_id to identify the buffer.
        gfx::Size(size.width, size.height),
        texture_ids[i]);
    picture_buffers.push_back(buffer);
  }
  decoder_->AssignPictureBuffers(picture_buffers);
  return PP_OK;
}

int32_t PepperVideoDecoderHost::OnHostMsgRecyclePicture(
    ppapi::host::HostMessageContext* context,
    uint32_t texture_id) {
  if (!initialized_)
    return PP_ERROR_FAILED;
  DCHECK(decoder_);

  decoder_->ReusePictureBuffer(texture_id);
  return PP_OK;
}

int32_t PepperVideoDecoderHost::OnHostMsgFlush(
    ppapi::host::HostMessageContext* context) {
  if (!initialized_)
    return PP_ERROR_FAILED;
  DCHECK(decoder_);
  if (flush_reply_context_.is_valid() || reset_reply_context_.is_valid())
    return PP_ERROR_FAILED;

  flush_reply_context_ = context->MakeReplyMessageContext();
  decoder_->Flush();

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoDecoderHost::OnHostMsgReset(
    ppapi::host::HostMessageContext* context) {
  if (!initialized_)
    return PP_ERROR_FAILED;
  DCHECK(decoder_);
  if (flush_reply_context_.is_valid() || reset_reply_context_.is_valid())
    return PP_ERROR_FAILED;

  reset_reply_context_ = context->MakeReplyMessageContext();
  decoder_->Reset();

  return PP_OK_COMPLETIONPENDING;
}

void PepperVideoDecoderHost::ProvidePictureBuffers(
    uint32 requested_num_of_buffers,
    const gfx::Size& dimensions,
    uint32 texture_target) {
  RequestTextures(requested_num_of_buffers,
                  dimensions,
                  texture_target,
                  std::vector<gpu::Mailbox>());
}

void PepperVideoDecoderHost::PictureReady(const media::Picture& picture) {
  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_VideoDecoder_PictureReady(picture.bitstream_buffer_id(),
                                               picture.picture_buffer_id()));
}

void PepperVideoDecoderHost::DismissPictureBuffer(int32 picture_buffer_id) {
  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_VideoDecoder_DismissPicture(picture_buffer_id));
}

void PepperVideoDecoderHost::NotifyEndOfBitstreamBuffer(
    int32 bitstream_buffer_id) {
  PendingDecodeMap::iterator it = pending_decodes_.find(bitstream_buffer_id);
  if (it == pending_decodes_.end()) {
    NOTREACHED();
    return;
  }
  const PendingDecode& pending_decode = it->second;
  host()->SendReply(
      pending_decode.reply_context,
      PpapiPluginMsg_VideoDecoder_DecodeReply(pending_decode.shm_id));
  shm_buffer_busy_[pending_decode.shm_id] = false;
  pending_decodes_.erase(it);
}

void PepperVideoDecoderHost::NotifyFlushDone() {
  DCHECK(pending_decodes_.empty());
  host()->SendReply(flush_reply_context_,
                    PpapiPluginMsg_VideoDecoder_FlushReply());
  flush_reply_context_ = ppapi::host::ReplyMessageContext();
}

void PepperVideoDecoderHost::NotifyResetDone() {
  DCHECK(pending_decodes_.empty());
  host()->SendReply(reset_reply_context_,
                    PpapiPluginMsg_VideoDecoder_ResetReply());
  reset_reply_context_ = ppapi::host::ReplyMessageContext();
}

void PepperVideoDecoderHost::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  int32_t pp_error = PP_ERROR_FAILED;
  switch (error) {
    case media::VideoDecodeAccelerator::UNREADABLE_INPUT:
      pp_error = PP_ERROR_MALFORMED_INPUT;
      break;
    case media::VideoDecodeAccelerator::ILLEGAL_STATE:
    case media::VideoDecodeAccelerator::INVALID_ARGUMENT:
    case media::VideoDecodeAccelerator::PLATFORM_FAILURE:
    case media::VideoDecodeAccelerator::LARGEST_ERROR_ENUM:
      pp_error = PP_ERROR_RESOURCE_FAILED;
      break;
    // No default case, to catch unhandled enum values.
  }
  host()->SendUnsolicitedReply(
      pp_resource(), PpapiPluginMsg_VideoDecoder_NotifyError(pp_error));
}

void PepperVideoDecoderHost::OnInitializeComplete(int32_t result) {
  if (!initialized_) {
    if (result == PP_OK)
      initialized_ = true;
    initialize_reply_context_.params.set_result(result);
    host()->SendReply(initialize_reply_context_,
                      PpapiPluginMsg_VideoDecoder_InitializeReply());
  }
}

const uint8_t* PepperVideoDecoderHost::DecodeIdToAddress(uint32_t decode_id) {
  PendingDecodeMap::const_iterator it = pending_decodes_.find(decode_id);
  DCHECK(it != pending_decodes_.end());
  uint32_t shm_id = it->second.shm_id;
  return static_cast<uint8_t*>(shm_buffers_[shm_id]->memory());
}

void PepperVideoDecoderHost::RequestTextures(
    uint32 requested_num_of_buffers,
    const gfx::Size& dimensions,
    uint32 texture_target,
    const std::vector<gpu::Mailbox>& mailboxes) {
  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_VideoDecoder_RequestTextures(
          requested_num_of_buffers,
          PP_MakeSize(dimensions.width(), dimensions.height()),
          texture_target,
          mailboxes));
}

}  // namespace content
