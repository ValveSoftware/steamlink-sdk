// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/external_popup_menu.h"

#include "content/common/view_messages.h"
#include "content/renderer/menu_item_builder.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebExternalPopupMenuClient.h"

namespace content {

ExternalPopupMenu::ExternalPopupMenu(
    RenderViewImpl* render_view,
    const blink::WebPopupMenuInfo& popup_menu_info,
    blink::WebExternalPopupMenuClient* popup_menu_client)
    : render_view_(render_view),
      popup_menu_info_(popup_menu_info),
      popup_menu_client_(popup_menu_client),
      origin_scale_for_emulation_(0) {
}

void ExternalPopupMenu::SetOriginScaleAndOffsetForEmulation(
    float scale, const gfx::Point& offset) {
  origin_scale_for_emulation_ = scale;
  origin_offset_for_emulation_ = offset;
}

void ExternalPopupMenu::show(const blink::WebRect& bounds) {
  blink::WebRect rect = bounds;
  if (origin_scale_for_emulation_) {
    rect.x *= origin_scale_for_emulation_;
    rect.y *= origin_scale_for_emulation_;
  }
  rect.x += origin_offset_for_emulation_.x();
  rect.y += origin_offset_for_emulation_.y();

  ViewHostMsg_ShowPopup_Params popup_params;
  popup_params.bounds = rect;
  popup_params.item_height = popup_menu_info_.itemHeight;
  popup_params.item_font_size = popup_menu_info_.itemFontSize;
  popup_params.selected_item = popup_menu_info_.selectedIndex;
  for (size_t i = 0; i < popup_menu_info_.items.size(); ++i) {
    popup_params.popup_items.push_back(
        MenuItemBuilder::Build(popup_menu_info_.items[i]));
  }
  popup_params.right_aligned = popup_menu_info_.rightAligned;
  popup_params.allow_multiple_selection =
      popup_menu_info_.allowMultipleSelection;
  render_view_->Send(
      new ViewHostMsg_ShowPopup(render_view_->routing_id(), popup_params));
}

void ExternalPopupMenu::close()  {
  render_view_->Send(new ViewHostMsg_HidePopup(render_view_->routing_id()));
  render_view_->DidHideExternalPopupMenu();
  // |this| was deleted.
}

#if defined(OS_MACOSX)
void ExternalPopupMenu::DidSelectItem(int index) {
  if (!popup_menu_client_)
    return;
  if (index == -1)
    popup_menu_client_->didCancel();
  else
    popup_menu_client_->didAcceptIndex(index);
}
#endif

#if defined(OS_ANDROID)
void ExternalPopupMenu::DidSelectItems(bool canceled,
                                       const std::vector<int>& indices) {
  if (!popup_menu_client_)
    return;
  if (canceled)
    popup_menu_client_->didCancel();
  else
    popup_menu_client_->didAcceptIndices(indices);
}
#endif

}  // namespace content
