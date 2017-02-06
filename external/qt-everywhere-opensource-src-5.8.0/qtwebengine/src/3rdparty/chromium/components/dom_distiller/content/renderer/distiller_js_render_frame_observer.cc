// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/renderer/distiller_js_render_frame_observer.h"

#include <utility>

#include "base/bind.h"
#include "components/dom_distiller/content/common/distiller_page_notifier_service.mojom.h"
#include "components/dom_distiller/content/renderer/distiller_page_notifier_service_impl.h"
#include "content/public/renderer/render_frame.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "v8/include/v8.h"

namespace dom_distiller {

DistillerJsRenderFrameObserver::DistillerJsRenderFrameObserver(
    content::RenderFrame* render_frame,
    const int distiller_isolated_world_id)
    : RenderFrameObserver(render_frame),
      distiller_isolated_world_id_(distiller_isolated_world_id),
      is_distiller_page_(false),
      weak_factory_(this) {}

DistillerJsRenderFrameObserver::~DistillerJsRenderFrameObserver() {}

void DistillerJsRenderFrameObserver::DidStartProvisionalLoad() {
  RegisterMojoInterface();
}

void DistillerJsRenderFrameObserver::DidFinishLoad() {
  // If no message about the distilled page was received at this point, there
  // will not be one; remove the mojom::DistillerPageNotifierService from the
  // registry.
  render_frame()
      ->GetInterfaceRegistry()
      ->RemoveInterface<mojom::DistillerPageNotifierService>();
}

void DistillerJsRenderFrameObserver::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int extension_group,
    int world_id) {
  if (world_id != distiller_isolated_world_id_ || !is_distiller_page_) {
    return;
  }

  native_javascript_handle_.reset(
      new DistillerNativeJavaScript(render_frame()));
  native_javascript_handle_->AddJavaScriptObjectToFrame(context);
}

void DistillerJsRenderFrameObserver::RegisterMojoInterface() {
  render_frame()->GetInterfaceRegistry()->AddInterface(base::Bind(
      &DistillerJsRenderFrameObserver::CreateDistillerPageNotifierService,
      weak_factory_.GetWeakPtr()));
}

void DistillerJsRenderFrameObserver::CreateDistillerPageNotifierService(
    mojo::InterfaceRequest<mojom::DistillerPageNotifierService> request) {
  // This is strongly bound to and owned by the pipe.
  new DistillerPageNotifierServiceImpl(this, std::move(request));
}

void DistillerJsRenderFrameObserver::SetIsDistillerPage() {
  is_distiller_page_ = true;
}

void DistillerJsRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace dom_distiller
