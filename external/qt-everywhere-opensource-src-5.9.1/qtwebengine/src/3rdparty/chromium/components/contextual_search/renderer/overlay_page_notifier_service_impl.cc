// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/renderer/overlay_page_notifier_service_impl.h"

#include <utility>

#include "components/contextual_search/renderer/overlay_js_render_frame_observer.h"

namespace contextual_search {

OverlayPageNotifierServiceImpl::OverlayPageNotifierServiceImpl(
    base::WeakPtr<OverlayJsRenderFrameObserver> observer)
    : overlay_js_observer_(observer) {}

OverlayPageNotifierServiceImpl::~OverlayPageNotifierServiceImpl() {}

void OverlayPageNotifierServiceImpl::NotifyIsContextualSearchOverlay() {
  if (overlay_js_observer_ != nullptr)
    overlay_js_observer_->SetIsContextualSearchOverlay();
}

}  // namespace contextual_search
