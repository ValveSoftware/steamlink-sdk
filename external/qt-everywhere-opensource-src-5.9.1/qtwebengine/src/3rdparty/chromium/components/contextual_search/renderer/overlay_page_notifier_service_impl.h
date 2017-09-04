// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_RENDERER_OVERLAY_PAGE_NOTIFIER_SERVICE_IMPL_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_RENDERER_OVERLAY_PAGE_NOTIFIER_SERVICE_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/contextual_search/common/overlay_page_notifier_service.mojom.h"
#include "components/contextual_search/renderer/overlay_js_render_frame_observer.h"

namespace contextual_search {

class OverlayJsRenderFrameObserver;

// mojom::OverlayPageNotifierService is responsible for listening to the browser
// for messages about whether a page is presented in an overlay panel.
// No message is received if the page is not presented in an overlay panel.
// This service should be removed from the registry once the page is done
// loading.
class OverlayPageNotifierServiceImpl
    : public mojom::OverlayPageNotifierService {
 public:
  explicit OverlayPageNotifierServiceImpl(
      base::WeakPtr<OverlayJsRenderFrameObserver> observer);
  ~OverlayPageNotifierServiceImpl() override;

  // Implementation of mojo interface OverlayPageNotifierService.
  void NotifyIsContextualSearchOverlay() override;

 private:
  base::WeakPtr<OverlayJsRenderFrameObserver> overlay_js_observer_;

  DISALLOW_COPY_AND_ASSIGN(OverlayPageNotifierServiceImpl);
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_RENDERER_OVERLAY_PAGE_NOTIFIER_SERVICE_IMPL_H_
