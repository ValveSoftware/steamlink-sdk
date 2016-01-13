// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_DESTINATION_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_DESTINATION_HOST_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc/video_destination_handler.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/host/resource_host.h"

namespace content {

class RendererPpapiHost;

class CONTENT_EXPORT PepperVideoDestinationHost
    : public ppapi::host::ResourceHost {
 public:
  PepperVideoDestinationHost(RendererPpapiHost* host,
                             PP_Instance instance,
                             PP_Resource resource);

  virtual ~PepperVideoDestinationHost();

  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  int32_t OnHostMsgOpen(ppapi::host::HostMessageContext* context,
                        const std::string& stream_url);
  int32_t OnHostMsgPutFrame(ppapi::host::HostMessageContext* context,
                            const ppapi::HostResource& image_data_resource,
                            PP_TimeTicks timestamp);
  int32_t OnHostMsgClose(ppapi::host::HostMessageContext* context);

  RendererPpapiHost* renderer_ppapi_host_;

  scoped_ptr<FrameWriterInterface> frame_writer_;

  base::WeakPtrFactory<PepperVideoDestinationHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperVideoDestinationHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_DESTINATION_HOST_H_
