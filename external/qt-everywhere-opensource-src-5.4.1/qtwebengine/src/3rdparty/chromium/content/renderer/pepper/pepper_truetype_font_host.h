// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_TRUETYPE_FONT_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_TRUETYPE_FONT_HOST_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/pepper/pepper_truetype_font.h"
#include "ppapi/host/resource_host.h"

namespace content {

class RendererPpapiHost;

class CONTENT_EXPORT PepperTrueTypeFontHost : public ppapi::host::ResourceHost {
 public:
  PepperTrueTypeFontHost(RendererPpapiHost* host,
                         PP_Instance instance,
                         PP_Resource resource,
                         const ppapi::proxy::SerializedTrueTypeFontDesc& desc);

  virtual ~PepperTrueTypeFontHost();

  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  int32_t OnHostMsgDescribe(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgGetTableTags(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgGetTable(ppapi::host::HostMessageContext* context,
                            uint32_t table,
                            int32_t offset,
                            int32_t max_data_length);

  RendererPpapiHost* renderer_ppapi_host_;
  scoped_ptr<PepperTrueTypeFont> font_;

  base::WeakPtrFactory<PepperTrueTypeFontHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperTrueTypeFontHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_TRUETYPE_FONT_HOST_H_
