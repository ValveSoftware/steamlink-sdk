// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_DECODER_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_DECODER_HOST_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/pp_codecs.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/resource_message_params.h"

namespace base {
class SharedMemory;
}

namespace content {

class PPB_Graphics3D_Impl;
class RendererPpapiHost;
class RenderViewImpl;
class VideoDecoderShim;

class CONTENT_EXPORT PepperVideoDecoderHost
    : public ppapi::host::ResourceHost,
      public media::VideoDecodeAccelerator::Client {
 public:
  PepperVideoDecoderHost(RendererPpapiHost* host,
                         PP_Instance instance,
                         PP_Resource resource);
  virtual ~PepperVideoDecoderHost();

 private:
  struct PendingDecode {
    PendingDecode(uint32_t shm_id,
                  const ppapi::host::ReplyMessageContext& reply_context);
    ~PendingDecode();

    const uint32_t shm_id;
    const ppapi::host::ReplyMessageContext reply_context;
  };

  friend class VideoDecoderShim;

  // ResourceHost implementation.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  // media::VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(uint32 requested_num_of_buffers,
                                     const gfx::Size& dimensions,
                                     uint32 texture_target) OVERRIDE;
  virtual void DismissPictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 bitstream_buffer_id) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyResetDone() OVERRIDE;
  virtual void NotifyError(media::VideoDecodeAccelerator::Error error) OVERRIDE;

  int32_t OnHostMsgInitialize(ppapi::host::HostMessageContext* context,
                              const ppapi::HostResource& graphics_context,
                              PP_VideoProfile profile,
                              bool allow_software_fallback);
  int32_t OnHostMsgGetShm(ppapi::host::HostMessageContext* context,
                          uint32_t shm_id,
                          uint32_t shm_size);
  int32_t OnHostMsgDecode(ppapi::host::HostMessageContext* context,
                          uint32_t shm_id,
                          uint32_t size,
                          int32_t decode_id);
  int32_t OnHostMsgAssignTextures(ppapi::host::HostMessageContext* context,
                                  const PP_Size& size,
                                  const std::vector<uint32_t>& texture_ids);
  int32_t OnHostMsgRecyclePicture(ppapi::host::HostMessageContext* context,
                                  uint32_t picture_id);
  int32_t OnHostMsgFlush(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgReset(ppapi::host::HostMessageContext* context);

  // These methods are needed by VideoDecodeShim, to look like a
  // VideoDecodeAccelerator.
  void OnInitializeComplete(int32_t result);
  const uint8_t* DecodeIdToAddress(uint32_t decode_id);
  void RequestTextures(uint32 requested_num_of_buffers,
                       const gfx::Size& dimensions,
                       uint32 texture_target,
                       const std::vector<gpu::Mailbox>& mailboxes);

  // Non-owning pointer.
  RendererPpapiHost* renderer_ppapi_host_;

  scoped_ptr<media::VideoDecodeAccelerator> decoder_;

  // A vector holding our shm buffers, in sync with a similar vector in the
  // resource. We use a buffer's index in these vectors as its id on both sides
  // of the proxy. Only add buffers or update them in place so as not to
  // invalidate these ids.
  ScopedVector<base::SharedMemory> shm_buffers_;
  // A vector of true/false indicating if a shm buffer is in use by the decoder.
  // This is parallel to |shm_buffers_|.
  std::vector<uint8_t> shm_buffer_busy_;

  // Maps decode uid to PendingDecode info.
  typedef base::hash_map<int32_t, PendingDecode> PendingDecodeMap;
  PendingDecodeMap pending_decodes_;

  ppapi::host::ReplyMessageContext flush_reply_context_;
  ppapi::host::ReplyMessageContext reset_reply_context_;
  // Only used when in software fallback mode.
  ppapi::host::ReplyMessageContext initialize_reply_context_;

  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(PepperVideoDecoderHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_DECODER_HOST_H_
