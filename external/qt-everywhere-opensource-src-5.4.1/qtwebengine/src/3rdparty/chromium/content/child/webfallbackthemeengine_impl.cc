// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webfallbackthemeengine_impl.h"

#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "ui/native_theme/fallback_theme.h"

using blink::WebCanvas;
using blink::WebColor;
using blink::WebRect;
using blink::WebFallbackThemeEngine;

namespace content {

static ui::NativeTheme::Part NativeThemePart(
    WebFallbackThemeEngine::Part part) {
  switch (part) {
    case WebFallbackThemeEngine::PartScrollbarDownArrow:
      return ui::NativeTheme::kScrollbarDownArrow;
    case WebFallbackThemeEngine::PartScrollbarLeftArrow:
      return ui::NativeTheme::kScrollbarLeftArrow;
    case WebFallbackThemeEngine::PartScrollbarRightArrow:
      return ui::NativeTheme::kScrollbarRightArrow;
    case WebFallbackThemeEngine::PartScrollbarUpArrow:
      return ui::NativeTheme::kScrollbarUpArrow;
    case WebFallbackThemeEngine::PartScrollbarHorizontalThumb:
      return ui::NativeTheme::kScrollbarHorizontalThumb;
    case WebFallbackThemeEngine::PartScrollbarVerticalThumb:
      return ui::NativeTheme::kScrollbarVerticalThumb;
    case WebFallbackThemeEngine::PartScrollbarHorizontalTrack:
      return ui::NativeTheme::kScrollbarHorizontalTrack;
    case WebFallbackThemeEngine::PartScrollbarVerticalTrack:
      return ui::NativeTheme::kScrollbarVerticalTrack;
    case WebFallbackThemeEngine::PartScrollbarCorner:
      return ui::NativeTheme::kScrollbarCorner;
    case WebFallbackThemeEngine::PartCheckbox:
      return ui::NativeTheme::kCheckbox;
    case WebFallbackThemeEngine::PartRadio:
      return ui::NativeTheme::kRadio;
    case WebFallbackThemeEngine::PartButton:
      return ui::NativeTheme::kPushButton;
    case WebFallbackThemeEngine::PartTextField:
      return ui::NativeTheme::kTextField;
    case WebFallbackThemeEngine::PartMenuList:
      return ui::NativeTheme::kMenuList;
    case WebFallbackThemeEngine::PartSliderTrack:
      return ui::NativeTheme::kSliderTrack;
    case WebFallbackThemeEngine::PartSliderThumb:
      return ui::NativeTheme::kSliderThumb;
    case WebFallbackThemeEngine::PartInnerSpinButton:
      return ui::NativeTheme::kInnerSpinButton;
    case WebFallbackThemeEngine::PartProgressBar:
      return ui::NativeTheme::kProgressBar;
    default:
      return ui::NativeTheme::kScrollbarDownArrow;
  }
}

static ui::NativeTheme::State NativeThemeState(
    WebFallbackThemeEngine::State state) {
  switch (state) {
    case WebFallbackThemeEngine::StateDisabled:
      return ui::NativeTheme::kDisabled;
    case WebFallbackThemeEngine::StateHover:
      return ui::NativeTheme::kHovered;
    case WebFallbackThemeEngine::StateNormal:
      return ui::NativeTheme::kNormal;
    case WebFallbackThemeEngine::StatePressed:
      return ui::NativeTheme::kPressed;
    default:
      return ui::NativeTheme::kDisabled;
  }
}

static void GetNativeThemeExtraParams(
    WebFallbackThemeEngine::Part part,
    WebFallbackThemeEngine::State state,
    const WebFallbackThemeEngine::ExtraParams* extra_params,
    ui::NativeTheme::ExtraParams* native_theme_extra_params) {
  switch (part) {
    case WebFallbackThemeEngine::PartScrollbarHorizontalTrack:
    case WebFallbackThemeEngine::PartScrollbarVerticalTrack:
      native_theme_extra_params->scrollbar_track.track_x =
          extra_params->scrollbarTrack.trackX;
      native_theme_extra_params->scrollbar_track.track_y =
          extra_params->scrollbarTrack.trackY;
      native_theme_extra_params->scrollbar_track.track_width =
          extra_params->scrollbarTrack.trackWidth;
      native_theme_extra_params->scrollbar_track.track_height =
          extra_params->scrollbarTrack.trackHeight;
      break;
    case WebFallbackThemeEngine::PartCheckbox:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      native_theme_extra_params->button.indeterminate =
          extra_params->button.indeterminate;
      break;
    case WebFallbackThemeEngine::PartRadio:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      break;
    case WebFallbackThemeEngine::PartButton:
      native_theme_extra_params->button.is_default =
          extra_params->button.isDefault;
      native_theme_extra_params->button.has_border =
          extra_params->button.hasBorder;
      // Native buttons have a different focus style.
      native_theme_extra_params->button.is_focused = false;
      native_theme_extra_params->button.background_color =
          extra_params->button.backgroundColor;
      break;
    case WebFallbackThemeEngine::PartTextField:
      native_theme_extra_params->text_field.is_text_area =
          extra_params->textField.isTextArea;
      native_theme_extra_params->text_field.is_listbox =
          extra_params->textField.isListbox;
      native_theme_extra_params->text_field.background_color =
          extra_params->textField.backgroundColor;
      break;
    case WebFallbackThemeEngine::PartMenuList:
      native_theme_extra_params->menu_list.has_border =
          extra_params->menuList.hasBorder;
      native_theme_extra_params->menu_list.has_border_radius =
          extra_params->menuList.hasBorderRadius;
      native_theme_extra_params->menu_list.arrow_x =
          extra_params->menuList.arrowX;
      native_theme_extra_params->menu_list.arrow_y =
          extra_params->menuList.arrowY;
      native_theme_extra_params->menu_list.background_color =
          extra_params->menuList.backgroundColor;
      break;
    case WebFallbackThemeEngine::PartSliderTrack:
    case WebFallbackThemeEngine::PartSliderThumb:
      native_theme_extra_params->slider.vertical =
          extra_params->slider.vertical;
      native_theme_extra_params->slider.in_drag = extra_params->slider.inDrag;
      break;
    case WebFallbackThemeEngine::PartInnerSpinButton:
      native_theme_extra_params->inner_spin.spin_up =
          extra_params->innerSpin.spinUp;
      native_theme_extra_params->inner_spin.read_only =
          extra_params->innerSpin.readOnly;
      break;
    case WebFallbackThemeEngine::PartProgressBar:
      native_theme_extra_params->progress_bar.determinate =
          extra_params->progressBar.determinate;
      native_theme_extra_params->progress_bar.value_rect_x =
          extra_params->progressBar.valueRectX;
      native_theme_extra_params->progress_bar.value_rect_y =
          extra_params->progressBar.valueRectY;
      native_theme_extra_params->progress_bar.value_rect_width =
          extra_params->progressBar.valueRectWidth;
      native_theme_extra_params->progress_bar.value_rect_height =
          extra_params->progressBar.valueRectHeight;
      break;
    default:
      break;  // Parts that have no extra params get here.
  }
}

WebFallbackThemeEngineImpl::WebFallbackThemeEngineImpl()
    : theme_(new ui::FallbackTheme()) {
}

WebFallbackThemeEngineImpl::~WebFallbackThemeEngineImpl() {}

blink::WebSize WebFallbackThemeEngineImpl::getSize(
    WebFallbackThemeEngine::Part part) {
  ui::NativeTheme::ExtraParams extra;
  return theme_->GetPartSize(NativeThemePart(part),
                             ui::NativeTheme::kNormal,
                             extra);
}

void WebFallbackThemeEngineImpl::paint(
    blink::WebCanvas* canvas,
    WebFallbackThemeEngine::Part part,
    WebFallbackThemeEngine::State state,
    const blink::WebRect& rect,
    const WebFallbackThemeEngine::ExtraParams* extra_params) {
  ui::NativeTheme::ExtraParams native_theme_extra_params;
  GetNativeThemeExtraParams(
      part, state, extra_params, &native_theme_extra_params);
  theme_->Paint(canvas,
                NativeThemePart(part),
                NativeThemeState(state),
                gfx::Rect(rect),
                native_theme_extra_params);
}

}  // namespace content
