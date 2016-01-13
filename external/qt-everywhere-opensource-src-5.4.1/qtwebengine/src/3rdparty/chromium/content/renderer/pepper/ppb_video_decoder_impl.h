// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_VIDEO_DECODER_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_VIDEO_DECODER_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/dev/ppp_video_decoder_dev.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppb_video_decoder_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_video_decoder_dev_api.h"

struct PP_PictureBuffer_Dev;
struct PP_VideoBitstreamBuffer_Dev;

namespace content {
class PPB_Graphics3D_Impl;

class PPB_VideoDecoder_Impl : public ppapi::PPB_VideoDecoder_Shared,
                              public media::VideoDecodeAccelerator::Client {
 public:
  // See PPB_VideoDecoder_Dev::Create.  Returns 0 on failure to create &
  // initialize.
  static PP_Resource Create(PP_Instance instance,
                            PP_Resource graphics_context,
                            PP_VideoDecoder_Profile profile);

  // PPB_VideoDecoder_Dev_API implementation.
  virtual int32_t Decode(const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
                         scoped_refptr<ppapi::TrackedCallback> callback)
      OVERRIDE;
  virtual void AssignPictureBuffers(uint32_t no_of_buffers,
                                    const PP_PictureBuffer_Dev* buffers)
      OVERRIDE;
  virtual void ReusePictureBuffer(int32_t picture_buffer_id) OVERRIDE;
  virtual int32_t Flush(scoped_refptr<ppapi::TrackedCallback> callback)
      OVERRIDE;
  virtual int32_t Reset(scoped_refptr<ppapi::TrackedCallback> callback)
      OVERRIDE;
  virtual void Destroy() OVERRIDE;

  // media::VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(uint32 requested_num_of_buffers,
                                     const gfx::Size& dimensions,
                                     uint32 texture_target) OVERRIDE;
  virtual void DismissPictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void NotifyError(media::VideoDecodeAccelerator::Error error) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 buffer_id) OVERRIDE;
  virtual void NotifyResetDone() OVERRIDE;

 private:
  virtual ~PPB_VideoDecoder_Impl();

  explicit PPB_VideoDecoder_Impl(PP_Instance instance);
  bool Init(PP_Resource graphics_context,
            PP_VideoDecoder_Profile profile);

  // This is NULL before initialization, and after destruction.
  // Holds a GpuVideoDecodeAcceleratorHost.
  scoped_ptr<media::VideoDecodeAccelerator> decoder_;

  // Reference to the plugin requesting this interface.
  const PPP_VideoDecoder_Dev* ppp_videodecoder_;

  DISALLOW_COPY_AND_ASSIGN(PPB_VideoDecoder_Impl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_VIDEO_DECODER_IMPL_H_
