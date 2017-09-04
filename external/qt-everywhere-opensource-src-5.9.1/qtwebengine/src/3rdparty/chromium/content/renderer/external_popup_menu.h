// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_EXTERNAL_POPUP_MENU_H_
#define CONTENT_RENDERER_EXTERNAL_POPUP_MENU_H_

#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/WebKit/public/web/WebExternalPopupMenu.h"
#include "third_party/WebKit/public/web/WebPopupMenuInfo.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {
class WebExternalPopupMenuClient;
}

namespace content {
class RenderFrameImpl;

class ExternalPopupMenu : public blink::WebExternalPopupMenu {
 public:
  ExternalPopupMenu(RenderFrameImpl* render_frame,
                    const blink::WebPopupMenuInfo& popup_menu_info,
                    blink::WebExternalPopupMenuClient* popup_menu_client);

  virtual ~ExternalPopupMenu() {}

  void SetOriginScaleAndOffsetForEmulation(
      float scale, const gfx::PointF& offset);

#if defined(USE_EXTERNAL_POPUP_MENU)
#if defined(OS_MACOSX)
  // Called when the user has selected an item. |selected_item| is -1 if the
  // user canceled the popup.
  void DidSelectItem(int selected_index);
#else
  // Called when the user has selected items or canceled the popup.
  void DidSelectItems(bool canceled, const std::vector<int>& selected_indices);
#endif
#endif

  // blink::WebExternalPopupMenu implementation:
  void show(const blink::WebRect& bounds) override;
  void close() override;

 private:
  RenderFrameImpl* render_frame_;
  blink::WebPopupMenuInfo popup_menu_info_;
  blink::WebExternalPopupMenuClient* popup_menu_client_;

  // Popups may be displaced when screen metrics emulation is enabled.
  // These scale and offset are used to properly adjust popup position.
  float origin_scale_for_emulation_;
  gfx::PointF origin_offset_for_emulation_;

  DISALLOW_COPY_AND_ASSIGN(ExternalPopupMenu);
};

}  // namespace content

#endif  // CONTENT_RENDERER_EXTERNAL_POPUP_MENU_H_
