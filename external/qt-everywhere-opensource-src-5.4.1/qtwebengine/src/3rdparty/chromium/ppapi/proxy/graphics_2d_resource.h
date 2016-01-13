// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_GRAPHICS_2D_RESOURCE_H_
#define PPAPI_PROXY_GRAPHICS_2D_RESOURCE_H_

#include "base/compiler_specific.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_graphics_2d_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

class PPAPI_PROXY_EXPORT Graphics2DResource
      : public PluginResource,
        public NON_EXPORTED_BASE(thunk::PPB_Graphics2D_API) {
 public:
  Graphics2DResource(Connection connection,
                     PP_Instance instance,
                     const PP_Size& size,
                     PP_Bool is_always_opaque);

  virtual ~Graphics2DResource();

  // Resource overrides.
  virtual thunk::PPB_Graphics2D_API* AsPPB_Graphics2D_API() OVERRIDE;

  // PPB_Graphics2D_API overrides.
  virtual PP_Bool Describe(PP_Size* size, PP_Bool* is_always_opaque) OVERRIDE;
  virtual void PaintImageData(PP_Resource image_data,
                              const PP_Point* top_left,
                              const PP_Rect* src_rect) OVERRIDE;
  virtual void Scroll(const PP_Rect* clip_rect,
                      const PP_Point* amount) OVERRIDE;
  virtual void ReplaceContents(PP_Resource image_data) OVERRIDE;
  virtual PP_Bool SetScale(float scale) OVERRIDE;
  virtual float GetScale() OVERRIDE;
  virtual int32_t Flush(scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual bool ReadImageData(PP_Resource image,
                             const PP_Point* top_left) OVERRIDE;

 private:
  void OnPluginMsgFlushACK(const ResourceMessageReplyParams& params);

  const PP_Size size_;
  const PP_Bool is_always_opaque_;
  float scale_;

  scoped_refptr<TrackedCallback> current_flush_callback_;

  DISALLOW_COPY_AND_ASSIGN(Graphics2DResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_GRAPHICS_2D_RESOURCE_H_
