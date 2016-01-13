// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_video_source_host.h"

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/skia/include/core/SkBitmap.h"

using ppapi::host::HostMessageContext;
using ppapi::host::ReplyMessageContext;

namespace content {

PepperVideoSourceHost::FrameReceiver::FrameReceiver(
    const base::WeakPtr<PepperVideoSourceHost>& host)
    : host_(host),
      main_message_loop_proxy_(base::MessageLoopProxy::current()) {}

PepperVideoSourceHost::FrameReceiver::~FrameReceiver() {}

bool PepperVideoSourceHost::FrameReceiver::GotFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  // It's not safe to access the host from this thread, so post a task to our
  // main thread to transfer the new frame.
  main_message_loop_proxy_->PostTask(
      FROM_HERE, base::Bind(&FrameReceiver::OnGotFrame, this, frame));

  return true;
}

void PepperVideoSourceHost::FrameReceiver::OnGotFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  if (host_.get()) {
    // Hold a reference to the new frame and release the previous.
    host_->last_frame_ = frame;

    if (host_->get_frame_pending_)
      host_->SendGetFrameReply();
  }
}

PepperVideoSourceHost::PepperVideoSourceHost(RendererPpapiHost* host,
                                             PP_Instance instance,
                                             PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      source_handler_(new VideoSourceHandler(NULL)),
      get_frame_pending_(false),
      weak_factory_(this) {
  frame_receiver_ = new FrameReceiver(weak_factory_.GetWeakPtr());
}

PepperVideoSourceHost::~PepperVideoSourceHost() { Close(); }

int32_t PepperVideoSourceHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperVideoSourceHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoSource_Open,
                                      OnHostMsgOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_VideoSource_GetFrame,
                                        OnHostMsgGetFrame)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_VideoSource_Close,
                                        OnHostMsgClose)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperVideoSourceHost::OnHostMsgOpen(HostMessageContext* context,
                                             const std::string& stream_url) {
  GURL gurl(stream_url);
  if (!gurl.is_valid())
    return PP_ERROR_BADARGUMENT;

  if (!source_handler_->Open(gurl.spec(), frame_receiver_.get()))
    return PP_ERROR_BADARGUMENT;

  stream_url_ = gurl.spec();

  ReplyMessageContext reply_context = context->MakeReplyMessageContext();
  reply_context.params.set_result(PP_OK);
  host()->SendReply(reply_context, PpapiPluginMsg_VideoSource_OpenReply());
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoSourceHost::OnHostMsgGetFrame(HostMessageContext* context) {
  if (!source_handler_.get())
    return PP_ERROR_FAILED;
  if (get_frame_pending_)
    return PP_ERROR_INPROGRESS;

  reply_context_ = context->MakeReplyMessageContext();
  get_frame_pending_ = true;

  // If a frame is ready, try to convert it and send the reply.
  if (last_frame_.get())
    SendGetFrameReply();

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoSourceHost::OnHostMsgClose(HostMessageContext* context) {
  Close();
  return PP_OK;
}

void PepperVideoSourceHost::SendGetFrameReply() {
  DCHECK(get_frame_pending_);
  get_frame_pending_ = false;

  DCHECK(last_frame_.get());
  scoped_refptr<media::VideoFrame> frame(last_frame_);
  last_frame_ = NULL;

  const int dst_width = frame->visible_rect().width();
  const int dst_height = frame->visible_rect().height();

  PP_ImageDataDesc image_desc;
  IPC::PlatformFileForTransit image_handle;
  uint32_t byte_count;
  ppapi::ScopedPPResource resource(
      ppapi::ScopedPPResource::PassRef(),
      ppapi::proxy::PPB_ImageData_Proxy::CreateImageData(
          pp_instance(),
          ppapi::PPB_ImageData_Shared::SIMPLE,
          PP_IMAGEDATAFORMAT_BGRA_PREMUL,
          PP_MakeSize(dst_width, dst_height),
          false /* init_to_zero */,
          &image_desc,
          &image_handle,
          &byte_count));
  if (!resource.get()) {
    SendGetFrameErrorReply(PP_ERROR_FAILED);
    return;
  }

  ppapi::thunk::EnterResourceNoLock<ppapi::thunk::PPB_ImageData_API>
      enter_resource(resource, false);
  if (enter_resource.failed()) {
    SendGetFrameErrorReply(PP_ERROR_FAILED);
    return;
  }

  PPB_ImageData_Impl* image_data =
      static_cast<PPB_ImageData_Impl*>(enter_resource.object());
  ImageDataAutoMapper mapper(image_data);
  if (!mapper.is_valid()) {
    SendGetFrameErrorReply(PP_ERROR_FAILED);
    return;
  }

  const SkBitmap* bitmap = image_data->GetMappedBitmap();
  if (!bitmap) {
    SendGetFrameErrorReply(PP_ERROR_FAILED);
    return;
  }
  uint8_t* bitmap_pixels = static_cast<uint8_t*>(bitmap->getPixels());
  if (!bitmap_pixels) {
    SendGetFrameErrorReply(PP_ERROR_FAILED);
    return;
  }

  // Calculate that portion of the |frame| that should be copied into
  // |bitmap|. If |frame| has been cropped,
  // frame->coded_size() != frame->visible_rect().
  const int src_width = frame->coded_size().width();
  const int src_height = frame->coded_size().height();
  DCHECK(src_width >= dst_width && src_height >= dst_height);

  const int horiz_crop = frame->visible_rect().x();
  const int vert_crop = frame->visible_rect().y();

  const uint8* src_y = frame->data(media::VideoFrame::kYPlane) +
                       (src_width * vert_crop + horiz_crop);
  const int center = (src_width + 1) / 2;
  const uint8* src_u = frame->data(media::VideoFrame::kUPlane) +
                       (center * vert_crop + horiz_crop) / 2;
  const uint8* src_v = frame->data(media::VideoFrame::kVPlane) +
                       (center * vert_crop + horiz_crop) / 2;

  libyuv::I420ToBGRA(src_y,
                     frame->stride(media::VideoFrame::kYPlane),
                     src_u,
                     frame->stride(media::VideoFrame::kUPlane),
                     src_v,
                     frame->stride(media::VideoFrame::kVPlane),
                     bitmap_pixels,
                     bitmap->rowBytes(),
                     dst_width,
                     dst_height);

  ppapi::HostResource host_resource;
  host_resource.SetHostResource(pp_instance(), resource.get());

  // Convert a video timestamp to a PP_TimeTicks (a double, in seconds).
  PP_TimeTicks timestamp = frame->timestamp().InSecondsF();

  ppapi::proxy::SerializedHandle serialized_handle;
  serialized_handle.set_shmem(image_handle, byte_count);
  reply_context_.params.AppendHandle(serialized_handle);

  host()->SendReply(reply_context_,
                    PpapiPluginMsg_VideoSource_GetFrameReply(
                        host_resource, image_desc, timestamp));

  reply_context_ = ppapi::host::ReplyMessageContext();

  // Keep a reference once we know this method succeeds.
  resource.Release();
}

void PepperVideoSourceHost::SendGetFrameErrorReply(int32_t error) {
  reply_context_.params.set_result(error);
  host()->SendReply(
      reply_context_,
      PpapiPluginMsg_VideoSource_GetFrameReply(
          ppapi::HostResource(), PP_ImageDataDesc(), 0.0 /* timestamp */));
  reply_context_ = ppapi::host::ReplyMessageContext();
}

void PepperVideoSourceHost::Close() {
  if (source_handler_.get() && !stream_url_.empty())
    source_handler_->Close(frame_receiver_.get());

  source_handler_.reset(NULL);
  stream_url_.clear();
}

}  // namespace content
