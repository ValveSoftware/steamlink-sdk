// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_RESTRICTION_WEB_RESTRICTION_GIN_WRAPPER_H_
#define COMPONENTS_WEB_RESTRICTION_WEB_RESTRICTION_GIN_WRAPPER_H_

#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "gin/wrappable.h"

namespace content {
class RenderFrame;
}

namespace web_restrictions {

class WebRestrictionsGinWrapper
    : public gin::Wrappable<WebRestrictionsGinWrapper>,
      public content::RenderFrameObserver {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(content::RenderFrame* render_frame);

 private:
  explicit WebRestrictionsGinWrapper(content::RenderFrame* render_frame);
  ~WebRestrictionsGinWrapper() override;

  // Override OnDestruct to prevent the wrapper going away when the
  // RenderFrame does
  void OnDestruct() override;

  // Request permission to allow visiting the currently blocked site.
  bool RequestPermission();

  // gin::WrappableBase
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  DISALLOW_COPY_AND_ASSIGN(WebRestrictionsGinWrapper);
};

}  // namespace web_restrictions.

#endif  // COMPONENTS_WEB_RESTRICTION_WEB_RESTRICTION_GIN_WRAPPER_H_
