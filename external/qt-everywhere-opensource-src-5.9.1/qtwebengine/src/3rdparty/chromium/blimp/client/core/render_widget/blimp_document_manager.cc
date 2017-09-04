// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/render_widget/blimp_document_manager.h"

#include "base/memory/ptr_util.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/render_widget/blimp_document.h"
#include "cc/output/copy_output_request.h"
#include "cc/proto/compositor_message.pb.h"

namespace blimp {
namespace client {

BlimpDocumentManager::BlimpDocumentManager(
    int blimp_contents_id,
    RenderWidgetFeature* render_widget_feature,
    BlimpCompositorDependencies* compositor_dependencies)
    : blimp_contents_id_(blimp_contents_id),
      render_widget_feature_(render_widget_feature),
      visible_(false),
      layer_(cc::Layer::Create()),
      active_document_(nullptr),
      compositor_dependencies_(compositor_dependencies) {
  DCHECK(render_widget_feature_);
  DCHECK(compositor_dependencies_);

  render_widget_feature_->SetDelegate(blimp_contents_id_, this);
}

BlimpDocumentManager::~BlimpDocumentManager() {
  render_widget_feature_->RemoveDelegate(blimp_contents_id_);
}

void BlimpDocumentManager::SetVisible(bool visible) {
  visible_ = visible;
  if (active_document_)
    active_document_->GetCompositor()->SetVisible(visible_);
}

bool BlimpDocumentManager::OnTouchEvent(const ui::MotionEvent& motion_event) {
  if (active_document_)
    return active_document_->OnTouchEvent(motion_event);
  return false;
}

void BlimpDocumentManager::RequestCopyOfCompositorOutput(
    std::unique_ptr<cc::CopyOutputRequest> copy_request,
    bool flush_pending_update) {
  if (!active_document_)
    return;

  active_document_->GetCompositor()->RequestCopyOfOutput(
      std::move(copy_request), flush_pending_update);
}

std::unique_ptr<BlimpDocument> BlimpDocumentManager::CreateBlimpDocument(
    int document_id,
    BlimpCompositorDependencies* compositor_dependencies) {
  return base::MakeUnique<BlimpDocument>(document_id, compositor_dependencies,
                                         this);
}

void BlimpDocumentManager::OnRenderWidgetCreated(int render_widget_id) {
  CHECK(!GetDocument(render_widget_id));

  documents_[render_widget_id] =
      CreateBlimpDocument(render_widget_id, compositor_dependencies_);
}

void BlimpDocumentManager::OnRenderWidgetInitialized(int render_widget_id) {
  if (active_document_ && active_document_->document_id() == render_widget_id)
    return;

  // Detach the content layer from the old compositor.
  layer_->RemoveAllChildren();

  if (active_document_) {
    VLOG(1) << "Hiding currently active compositor for render widget: "
            << active_document_->document_id();
    active_document_->GetCompositor()->SetVisible(false);
  }

  active_document_ = GetDocument(render_widget_id);
  CHECK(active_document_);

  active_document_->GetCompositor()->SetVisible(visible_);
  layer_->AddChild(active_document_->GetCompositor()->layer());
}

void BlimpDocumentManager::OnRenderWidgetDeleted(int render_widget_id) {
  DocumentMap::const_iterator it = documents_.find(render_widget_id);
  CHECK(it != documents_.end());

  // Reset the |active_document_| if that is what we're destroying right now.
  if (active_document_ == it->second.get()) {
    layer_->RemoveAllChildren();
    active_document_ = nullptr;
  }

  documents_.erase(it);
}

void BlimpDocumentManager::OnCompositorMessageReceived(
    int render_widget_id,
    std::unique_ptr<cc::proto::CompositorMessage> message) {
  BlimpDocument* document = GetDocument(render_widget_id);
  BlimpCompositor* compositor = document->GetCompositor();

  CHECK(compositor);
  compositor->OnCompositorMessageReceived(std::move(message));
}

void BlimpDocumentManager::SendWebGestureEvent(
    int document_id,
    const blink::WebGestureEvent& gesture_event) {
  render_widget_feature_->SendWebGestureEvent(blimp_contents_id_, document_id,
                                              gesture_event);
}

void BlimpDocumentManager::SendCompositorMessage(
    int document_id,
    const cc::proto::CompositorMessage& message) {
  render_widget_feature_->SendCompositorMessage(blimp_contents_id_, document_id,
                                                message);
}

BlimpDocument* BlimpDocumentManager::GetDocument(int document_id) {
  DocumentMap::const_iterator it = documents_.find(document_id);
  if (it == documents_.end())
    return nullptr;
  return it->second.get();
}

}  // namespace client
}  // namespace blimp
