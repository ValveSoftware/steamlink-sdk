// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/renderer/distiller_page_notifier_service_impl.h"

#include <utility>

#include "components/dom_distiller/content/renderer/distiller_js_render_frame_observer.h"
#include "components/dom_distiller/content/renderer/distiller_native_javascript.h"

namespace dom_distiller {

DistillerPageNotifierServiceImpl::DistillerPageNotifierServiceImpl(
    DistillerJsRenderFrameObserver* observer,
    mojo::InterfaceRequest<mojom::DistillerPageNotifierService> request)
    : binding_(this, std::move(request)), distiller_js_observer_(observer) {}

void DistillerPageNotifierServiceImpl::NotifyIsDistillerPage() {
  // TODO(mdjones): Send some form of unique ID so this call knows
  // which tab it should respond to..
  distiller_js_observer_->SetIsDistillerPage();
}

DistillerPageNotifierServiceImpl::~DistillerPageNotifierServiceImpl() {}

}  // namespace dom_distiller
