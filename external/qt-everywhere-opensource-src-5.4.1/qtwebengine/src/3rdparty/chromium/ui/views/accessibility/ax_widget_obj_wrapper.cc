// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_widget_obj_wrapper.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/widget/widget.h"

namespace views {

AXWidgetObjWrapper::AXWidgetObjWrapper(Widget* widget) : widget_(widget) {
  widget->AddObserver(this);
  widget->AddRemovalsObserver(this);
}

AXWidgetObjWrapper::~AXWidgetObjWrapper() {
  widget_->RemoveObserver(this);
  widget_->RemoveRemovalsObserver(this);
  widget_ = NULL;
}

AXAuraObjWrapper* AXWidgetObjWrapper::GetParent() {
  return AXAuraObjCache::GetInstance()->GetOrCreate(widget_->GetNativeView());
}

void AXWidgetObjWrapper::GetChildren(
    std::vector<AXAuraObjWrapper*>* out_children) {
  out_children->push_back(
      AXAuraObjCache::GetInstance()->GetOrCreate(widget_->GetRootView()));
}

void AXWidgetObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
  out_node_data->id = GetID();
  out_node_data->role = ui::AX_ROLE_CLIENT;
  out_node_data->location = widget_->GetWindowBoundsInScreen();
  // TODO(dtseng): Set better states.
  out_node_data->state = 0;
}

int32 AXWidgetObjWrapper::GetID() {
  return AXAuraObjCache::GetInstance()->GetID(widget_);
}

void AXWidgetObjWrapper::OnWidgetDestroying(Widget* widget) {
  AXAuraObjCache::GetInstance()->Remove(widget);
}

void AXWidgetObjWrapper::OnWillRemoveView(Widget* widget, View* view) {
  AXAuraObjCache::GetInstance()->Remove(view);
}

}  // namespace views
