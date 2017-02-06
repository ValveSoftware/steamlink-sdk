// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/context_menu_params.h"

namespace content {

const int32_t CustomContextMenuContext::kCurrentRenderWidget = INT32_MAX;

CustomContextMenuContext::CustomContextMenuContext()
    : is_pepper_menu(false),
      request_id(0),
      render_widget_id(kCurrentRenderWidget) {
}

ContextMenuParams::ContextMenuParams()
    : media_type(blink::WebContextMenuData::MediaTypeNone),
      x(0),
      y(0),
      has_image_contents(true),
      media_flags(0),
      misspelling_hash(0),
      spellcheck_enabled(false),
      is_editable(false),
      writing_direction_default(
          blink::WebContextMenuData::CheckableMenuItemDisabled),
      writing_direction_left_to_right(
          blink::WebContextMenuData::CheckableMenuItemEnabled),
      writing_direction_right_to_left(
          blink::WebContextMenuData::CheckableMenuItemEnabled),
      edit_flags(0),
      referrer_policy(blink::WebReferrerPolicyDefault),
      source_type(ui::MENU_SOURCE_NONE),
      input_field_type(blink::WebContextMenuData::InputFieldTypeNone) {
}

ContextMenuParams::ContextMenuParams(const ContextMenuParams& other) = default;

ContextMenuParams::~ContextMenuParams() {
}

}  // namespace content
