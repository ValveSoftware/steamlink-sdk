// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webthemeengine_impl_android.h"

#include "base/logging.h"
#include "base/sys_info.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "ui/native_theme/native_theme.h"

using blink::WebCanvas;
using blink::WebColor;
using blink::WebRect;
using blink::WebThemeEngine;

namespace content {

namespace {
  const int kVersionLollipop = 5;

  int getMajorVersion() {
    int major, minor, bugfix;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    return major;
  }
}

static ui::NativeTheme::Part NativeThemePart(WebThemeEngine::Part part) {
  switch (part) {
    case WebThemeEngine::PartScrollbarDownArrow:
      return ui::NativeTheme::kScrollbarDownArrow;
    case WebThemeEngine::PartScrollbarLeftArrow:
      return ui::NativeTheme::kScrollbarLeftArrow;
    case WebThemeEngine::PartScrollbarRightArrow:
      return ui::NativeTheme::kScrollbarRightArrow;
    case WebThemeEngine::PartScrollbarUpArrow:
      return ui::NativeTheme::kScrollbarUpArrow;
    case WebThemeEngine::PartScrollbarHorizontalThumb:
    case WebThemeEngine::PartScrollbarVerticalThumb:
    case WebThemeEngine::PartScrollbarHorizontalTrack:
    case WebThemeEngine::PartScrollbarVerticalTrack:
    case WebThemeEngine::PartScrollbarCorner:
      // Android doesn't draw scrollbars.
      NOTREACHED();
      return static_cast<ui::NativeTheme::Part>(0);
    case WebThemeEngine::PartCheckbox:
      return ui::NativeTheme::kCheckbox;
    case WebThemeEngine::PartRadio:
      return ui::NativeTheme::kRadio;
    case WebThemeEngine::PartButton:
      return ui::NativeTheme::kPushButton;
    case WebThemeEngine::PartTextField:
      return ui::NativeTheme::kTextField;
    case WebThemeEngine::PartMenuList:
      return ui::NativeTheme::kMenuList;
    case WebThemeEngine::PartSliderTrack:
      return ui::NativeTheme::kSliderTrack;
    case WebThemeEngine::PartSliderThumb:
      return ui::NativeTheme::kSliderThumb;
    case WebThemeEngine::PartInnerSpinButton:
      return ui::NativeTheme::kInnerSpinButton;
    case WebThemeEngine::PartProgressBar:
      return ui::NativeTheme::kProgressBar;
    default:
      return ui::NativeTheme::kScrollbarDownArrow;
  }
}

static ui::NativeTheme::State NativeThemeState(WebThemeEngine::State state) {
  switch (state) {
    case WebThemeEngine::StateDisabled:
      return ui::NativeTheme::kDisabled;
    case WebThemeEngine::StateHover:
      return ui::NativeTheme::kHovered;
    case WebThemeEngine::StateNormal:
      return ui::NativeTheme::kNormal;
    case WebThemeEngine::StatePressed:
      return ui::NativeTheme::kPressed;
    default:
      return ui::NativeTheme::kDisabled;
  }
}

static void GetNativeThemeExtraParams(
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const WebThemeEngine::ExtraParams* extra_params,
    ui::NativeTheme::ExtraParams* native_theme_extra_params) {
  switch (part) {
    case WebThemeEngine::PartScrollbarHorizontalTrack:
    case WebThemeEngine::PartScrollbarVerticalTrack:
      // Android doesn't draw scrollbars.
      NOTREACHED();
      break;
    case WebThemeEngine::PartCheckbox:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      native_theme_extra_params->button.indeterminate =
          extra_params->button.indeterminate;
      break;
    case WebThemeEngine::PartRadio:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      break;
    case WebThemeEngine::PartButton:
      native_theme_extra_params->button.is_default =
          extra_params->button.isDefault;
      native_theme_extra_params->button.has_border =
          extra_params->button.hasBorder;
      // Native buttons have a different focus style.
      native_theme_extra_params->button.is_focused = false;
      native_theme_extra_params->button.background_color =
          extra_params->button.backgroundColor;
      break;
    case WebThemeEngine::PartTextField:
      native_theme_extra_params->text_field.is_text_area =
          extra_params->textField.isTextArea;
      native_theme_extra_params->text_field.is_listbox =
          extra_params->textField.isListbox;
      native_theme_extra_params->text_field.background_color =
          extra_params->textField.backgroundColor;
      break;
    case WebThemeEngine::PartMenuList:
      native_theme_extra_params->menu_list.has_border =
          extra_params->menuList.hasBorder;
      native_theme_extra_params->menu_list.has_border_radius =
          extra_params->menuList.hasBorderRadius;
      native_theme_extra_params->menu_list.arrow_x =
          extra_params->menuList.arrowX;
      native_theme_extra_params->menu_list.arrow_y =
          extra_params->menuList.arrowY;
      native_theme_extra_params->menu_list.arrow_size =
          extra_params->menuList.arrowSize;
      native_theme_extra_params->menu_list.arrow_color =
          extra_params->menuList.arrowColor;
      native_theme_extra_params->menu_list.background_color =
          extra_params->menuList.backgroundColor;
      break;
    case WebThemeEngine::PartSliderTrack:
    case WebThemeEngine::PartSliderThumb:
      native_theme_extra_params->slider.vertical =
          extra_params->slider.vertical;
      native_theme_extra_params->slider.in_drag = extra_params->slider.inDrag;
      break;
    case WebThemeEngine::PartInnerSpinButton:
      native_theme_extra_params->inner_spin.spin_up =
          extra_params->innerSpin.spinUp;
      native_theme_extra_params->inner_spin.read_only =
          extra_params->innerSpin.readOnly;
      break;
    case WebThemeEngine::PartProgressBar:
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

blink::WebSize WebThemeEngineImpl::getSize(WebThemeEngine::Part part) {
  ui::NativeTheme::ExtraParams extra;
  return ui::NativeTheme::GetInstanceForWeb()->GetPartSize(
      NativeThemePart(part), ui::NativeTheme::kNormal, extra);
}

void WebThemeEngineImpl::getOverlayScrollbarStyle(ScrollbarStyle* style) {
  // TODO(bokan): Android scrollbars on non-composited scrollers don't
  // currently fade out so the fadeOutDuration and Delay  Now that this has
  // been added into Blink for other platforms we should plumb that through for
  // Android as well.
  style->fadeOutDelaySeconds = 0;
  style->fadeOutDurationSeconds = 0;
  if (getMajorVersion() >= kVersionLollipop) {
    style->thumbThickness = 4;
    style->scrollbarMargin = 0;
    style->color = SkColorSetARGB(128, 64, 64, 64);
  } else {
    style->thumbThickness = 3;
    style->scrollbarMargin = 3;
    style->color = SkColorSetARGB(128, 128, 128, 128);
  }
}

void WebThemeEngineImpl::paint(
    blink::WebCanvas* canvas,
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const blink::WebRect& rect,
    const WebThemeEngine::ExtraParams* extra_params) {
  ui::NativeTheme::ExtraParams native_theme_extra_params;
  GetNativeThemeExtraParams(
      part, state, extra_params, &native_theme_extra_params);
  ui::NativeTheme::GetInstanceForWeb()->Paint(
      canvas, NativeThemePart(part), NativeThemeState(state), gfx::Rect(rect),
      native_theme_extra_params);
}
}  // namespace content
