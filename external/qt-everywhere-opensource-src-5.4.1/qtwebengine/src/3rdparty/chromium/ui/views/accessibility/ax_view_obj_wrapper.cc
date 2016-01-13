// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_view_obj_wrapper.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

AXViewObjWrapper::AXViewObjWrapper(View* view)  : view_(view) {
  DCHECK(view->GetWidget());
  if (view->GetWidget())
    AXAuraObjCache::GetInstance()->GetOrCreate(view->GetWidget());
}

AXViewObjWrapper::~AXViewObjWrapper() {}

AXAuraObjWrapper* AXViewObjWrapper::GetParent() {
  AXAuraObjCache* cache = AXAuraObjCache::GetInstance();
  if (view_->parent())
    return cache->GetOrCreate(view_->parent());

  return cache->GetOrCreate(view_->GetWidget());
}

void AXViewObjWrapper::GetChildren(
    std::vector<AXAuraObjWrapper*>* out_children) {
  // TODO(dtseng): Need to handle |Widget| child of |View|.
  for (int i = 0; i < view_->child_count(); ++i) {
    AXAuraObjWrapper* child =
        AXAuraObjCache::GetInstance()->GetOrCreate(view_->child_at(i));
    out_children->push_back(child);
  }
}

void AXViewObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
  ui::AXViewState view_data;
  view_->GetAccessibleState(&view_data);
  out_node_data->id = GetID();
  out_node_data->role = view_data.role;

  out_node_data->state = view_data.state();
  if (view_->HasFocus())
    out_node_data->state |= 1 << ui::AX_STATE_FOCUSED;
  if (view_->IsFocusable())
    out_node_data->state |= 1 << ui::AX_STATE_FOCUSABLE;

  out_node_data->location = view_->GetBoundsInScreen();

  out_node_data->AddStringAttribute(
      ui::AX_ATTR_NAME, base::UTF16ToUTF8(view_data.name));
  out_node_data->AddStringAttribute(
      ui::AX_ATTR_VALUE, base::UTF16ToUTF8(view_data.value));

  out_node_data->AddIntAttribute(ui::AX_ATTR_TEXT_SEL_START,
                                 view_data.selection_start);
  out_node_data->AddIntAttribute(ui::AX_ATTR_TEXT_SEL_END,
                                 view_data.selection_end);
}

int32 AXViewObjWrapper::GetID() {
  return AXAuraObjCache::GetInstance()->GetID(view_);
}

void AXViewObjWrapper::DoDefault() {
  gfx::Rect rect = view_->GetBoundsInScreen();
  gfx::Point center = rect.CenterPoint();
  view_->OnMousePressed(ui::MouseEvent(ui::ET_MOUSE_PRESSED, center, center,
                                       ui::EF_LEFT_MOUSE_BUTTON,
                                       ui::EF_LEFT_MOUSE_BUTTON));
}

void AXViewObjWrapper::Focus() {
  view_->RequestFocus();
}

void AXViewObjWrapper::MakeVisible() {
  // TODO(dtseng): Implement.
}

void AXViewObjWrapper::SetSelection(int32 start, int32 end) {
  // TODO(dtseng): Implement.
}

}  // namespace views
