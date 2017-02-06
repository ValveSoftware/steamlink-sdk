// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/render_widget_screen_metrics_emulator.h"

#include "content/common/resize_params.h"
#include "content/public/common/context_menu_params.h"
#include "content/renderer/devtools/render_widget_screen_metrics_emulator_delegate.h"

namespace content {

RenderWidgetScreenMetricsEmulator::RenderWidgetScreenMetricsEmulator(
    RenderWidgetScreenMetricsEmulatorDelegate* delegate,
    const blink::WebDeviceEmulationParams& params,
    const ResizeParams& resize_params,
    const gfx::Rect& view_screen_rect,
    const gfx::Rect& window_screen_rect)
    : delegate_(delegate),
      emulation_params_(params),
      scale_(1.f),
      original_resize_params_(resize_params),
      original_view_screen_rect_(view_screen_rect),
      original_window_screen_rect_(window_screen_rect) {
}

RenderWidgetScreenMetricsEmulator::~RenderWidgetScreenMetricsEmulator() {
  delegate_->Resize(original_resize_params_);
  delegate_->SetScreenMetricsEmulationParameters(false, emulation_params_);
  delegate_->SetScreenRects(original_view_screen_rect_,
                            original_window_screen_rect_);
}

void RenderWidgetScreenMetricsEmulator::ChangeEmulationParams(
    const blink::WebDeviceEmulationParams& params) {
  emulation_params_ = params;
  Apply();
}

void RenderWidgetScreenMetricsEmulator::Apply() {
  ResizeParams modified_resize_params = original_resize_params_;
  applied_widget_rect_.set_size(gfx::Size(emulation_params_.viewSize));

  if (!applied_widget_rect_.width())
    applied_widget_rect_.set_width(original_size().width());

  if (!applied_widget_rect_.height())
    applied_widget_rect_.set_height(original_size().height());

  if (emulation_params_.fitToView && !original_size().IsEmpty()) {
    int original_width = std::max(original_size().width(), 1);
    int original_height = std::max(original_size().height(), 1);
    float width_ratio =
        static_cast<float>(applied_widget_rect_.width()) / original_width;
    float height_ratio =
        static_cast<float>(applied_widget_rect_.height()) / original_height;
    float ratio = std::max(1.0f, std::max(width_ratio, height_ratio));
    scale_ = 1.f / ratio;

    // Center emulated view inside available view space.
    offset_.set_x(
        (original_size().width() - scale_ * applied_widget_rect_.width()) / 2);
    offset_.set_y(
        (original_size().height() - scale_ * applied_widget_rect_.height()) /
        2);
  } else {
    scale_ = emulation_params_.scale;
    offset_.SetPoint(emulation_params_.offset.x, emulation_params_.offset.y);
    if (!emulation_params_.viewSize.width &&
        !emulation_params_.viewSize.height && scale_) {
      applied_widget_rect_.set_size(
          gfx::ScaleToRoundedSize(original_size(), 1.f / scale_));
    }
  }

  gfx::Rect window_screen_rect;
  if (emulation_params_.screenPosition ==
      blink::WebDeviceEmulationParams::Desktop) {
    applied_widget_rect_.set_origin(original_view_screen_rect_.origin());
    modified_resize_params.screen_info.rect = original_screen_info().rect;
    modified_resize_params.screen_info.availableRect =
        original_screen_info().availableRect;
    window_screen_rect = original_window_screen_rect_;
  } else {
    applied_widget_rect_.set_origin(emulation_params_.viewPosition);
    gfx::Rect screen_rect = applied_widget_rect_;
    if (!emulation_params_.screenSize.isEmpty()) {
      screen_rect = gfx::Rect(0, 0, emulation_params_.screenSize.width,
                              emulation_params_.screenSize.height);
    }
    modified_resize_params.screen_info.rect = screen_rect;
    modified_resize_params.screen_info.availableRect = screen_rect;
    window_screen_rect = applied_widget_rect_;
  }

  modified_resize_params.screen_info.deviceScaleFactor =
      emulation_params_.deviceScaleFactor
          ? emulation_params_.deviceScaleFactor
          : original_screen_info().deviceScaleFactor;

  if (emulation_params_.screenOrientationType !=
      blink::WebScreenOrientationUndefined) {
    modified_resize_params.screen_info.orientationType =
        emulation_params_.screenOrientationType;
    modified_resize_params.screen_info.orientationAngle =
        emulation_params_.screenOrientationAngle;
  }

  // Pass three emulation parameters to the blink side:
  // - we keep the real device scale factor in compositor to produce sharp image
  //   even when emulating different scale factor;
  // - in order to fit into view, WebView applies offset and scale to the
  //   root layer.
  blink::WebDeviceEmulationParams modified_emulation_params = emulation_params_;
  modified_emulation_params.deviceScaleFactor =
      original_screen_info().deviceScaleFactor;
  modified_emulation_params.offset =
      blink::WebFloatPoint(offset_.x(), offset_.y());
  modified_emulation_params.scale = scale_;
  delegate_->SetScreenMetricsEmulationParameters(true,
                                                 modified_emulation_params);

  delegate_->SetScreenRects(applied_widget_rect_, window_screen_rect);

  modified_resize_params.physical_backing_size =
      gfx::ScaleToCeiledSize(original_resize_params_.new_size,
                             original_screen_info().deviceScaleFactor);
  modified_resize_params.new_size = applied_widget_rect_.size();
  modified_resize_params.visible_viewport_size = applied_widget_rect_.size();
  modified_resize_params.needs_resize_ack = false;
  delegate_->Resize(modified_resize_params);
}

void RenderWidgetScreenMetricsEmulator::OnResize(const ResizeParams& params) {
  original_resize_params_ = params;
  Apply();

  if (params.needs_resize_ack)
    delegate_->Redraw();
}

void RenderWidgetScreenMetricsEmulator::OnUpdateWindowScreenRect(
    const gfx::Rect& window_screen_rect) {
  original_window_screen_rect_ = window_screen_rect;
  if (emulation_params_.screenPosition ==
      blink::WebDeviceEmulationParams::Desktop)
    Apply();
}

void RenderWidgetScreenMetricsEmulator::OnUpdateScreenRects(
    const gfx::Rect& view_screen_rect,
    const gfx::Rect& window_screen_rect) {
  original_view_screen_rect_ = view_screen_rect;
  original_window_screen_rect_ = window_screen_rect;
  if (emulation_params_.screenPosition ==
      blink::WebDeviceEmulationParams::Desktop) {
    Apply();
  }
}

void RenderWidgetScreenMetricsEmulator::OnShowContextMenu(
    ContextMenuParams* params) {
  params->x *= scale_;
  params->x += offset_.x();
  params->y *= scale_;
  params->y += offset_.y();
}

gfx::Rect RenderWidgetScreenMetricsEmulator::AdjustValidationMessageAnchor(
    const gfx::Rect& anchor) {
  gfx::Rect scaled = gfx::ScaleToEnclosedRect(anchor, scale_);
  scaled.set_x(scaled.x() + offset_.x());
  scaled.set_y(scaled.y() + offset_.y());
  return scaled;
}

}  // namespace content
