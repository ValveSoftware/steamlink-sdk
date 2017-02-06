// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_RENDERER_OVERLAY_JS_RENDER_FRAME_OBSERVER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_RENDERER_OVERLAY_JS_RENDER_FRAME_OBSERVER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/contextual_search/common/overlay_page_notifier_service.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"

namespace contextual_search {

// OverlayJsRenderFrame observer waits for a page to be loaded and then
// tries to connect to a mojo service hosted in the browser process. The
// service will tell this render process if the current page is presented
// in an overlay panel.
class OverlayJsRenderFrameObserver : public content::RenderFrameObserver {
 public:
  explicit OverlayJsRenderFrameObserver(content::RenderFrame* render_frame);
  ~OverlayJsRenderFrameObserver() override;

  // RenderFrameObserver implementation.
  void DidStartProvisionalLoad() override;
  void DidClearWindowObject() override;
  void DidFinishLoad() override;

  // Flag the current page as a contextual search overlay.
  void SetIsContextualSearchOverlay();

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Add the mojo interface to a RenderFrame's shell::InterfaceRegistry.
  void RegisterMojoInterface();
  void CreateOverlayPageNotifierService(
      mojo::InterfaceRequest<mojom::OverlayPageNotifierService> request);

  // Track if the current page is presented in the contextual search overlay.
  bool is_contextual_search_overlay_;

  base::WeakPtrFactory<OverlayJsRenderFrameObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(OverlayJsRenderFrameObserver);
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_RENDERER_OVERLAY_JS_RENDER_FRAME_OBSERVER_H_
