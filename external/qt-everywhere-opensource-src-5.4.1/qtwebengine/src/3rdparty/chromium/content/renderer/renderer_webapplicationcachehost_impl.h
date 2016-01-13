// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_WEBAPPLICATIONCACHEHOST_IMPL_H_
#define CONTENT_RENDERER_RENDERER_WEBAPPLICATIONCACHEHOST_IMPL_H_

#include "content/child/appcache/web_application_cache_host_impl.h"

namespace content {
class RenderViewImpl;

class RendererWebApplicationCacheHostImpl : public WebApplicationCacheHostImpl {
 public:
  RendererWebApplicationCacheHostImpl(
      RenderViewImpl* render_view,
      blink::WebApplicationCacheHostClient* client,
      appcache::AppCacheBackend* backend);

  // WebApplicationCacheHostImpl:
  virtual void OnLogMessage(appcache::AppCacheLogLevel log_level,
                            const std::string& message) OVERRIDE;
  virtual void OnContentBlocked(const GURL& manifest_url) OVERRIDE;
  virtual void OnCacheSelected(const appcache::AppCacheInfo& info) OVERRIDE;

 private:
  RenderViewImpl* GetRenderView();

  int routing_id_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_WEBAPPLICATIONCACHEHOST_IMPL_H_
