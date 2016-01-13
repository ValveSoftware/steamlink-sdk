// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow.h"

#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/wm/core/image_grid.h"

namespace {

// Shadow opacity for different styles.
const float kActiveShadowOpacity = 1.0f;
const float kInactiveShadowOpacity = 0.2f;
const float kSmallShadowOpacity = 1.0f;

// Interior inset for different styles.
const int kActiveInteriorInset = 0;
const int kInactiveInteriorInset = 0;
const int kSmallInteriorInset = 5;

// Duration for opacity animation in milliseconds.
const int kShadowAnimationDurationMs = 100;

float GetOpacityForStyle(wm::Shadow::Style style) {
  switch (style) {
    case wm::Shadow::STYLE_ACTIVE:
      return kActiveShadowOpacity;
    case wm::Shadow::STYLE_INACTIVE:
      return kInactiveShadowOpacity;
    case wm::Shadow::STYLE_SMALL:
      return kSmallShadowOpacity;
  }
  return 1.0f;
}

int GetInteriorInsetForStyle(wm::Shadow::Style style) {
  switch (style) {
    case wm::Shadow::STYLE_ACTIVE:
      return kActiveInteriorInset;
    case wm::Shadow::STYLE_INACTIVE:
      return kInactiveInteriorInset;
    case wm::Shadow::STYLE_SMALL:
      return kSmallInteriorInset;
  }
  return 0;
}

}  // namespace

namespace wm {

Shadow::Shadow() : style_(STYLE_ACTIVE), interior_inset_(0) {
}

Shadow::~Shadow() {
}

void Shadow::Init(Style style) {
  style_ = style;
  image_grid_.reset(new ImageGrid);
  UpdateImagesForStyle();
  image_grid_->layer()->set_name("Shadow");
  image_grid_->layer()->SetOpacity(GetOpacityForStyle(style_));
}

void Shadow::SetContentBounds(const gfx::Rect& content_bounds) {
  content_bounds_ = content_bounds;
  UpdateImageGridBounds();
}

ui::Layer* Shadow::layer() const {
  return image_grid_->layer();
}

void Shadow::SetStyle(Style style) {
  if (style_ == style)
    return;

  Style old_style = style_;
  style_ = style;

  // Stop waiting for any as yet unfinished implicit animations.
  StopObservingImplicitAnimations();

  // If we're switching to or from the small style, don't bother with
  // animations.
  if (style == STYLE_SMALL || old_style == STYLE_SMALL) {
    UpdateImagesForStyle();
    image_grid_->layer()->SetOpacity(GetOpacityForStyle(style));
    return;
  }

  // If we're becoming active, switch images now.  Because the inactive image
  // has a very low opacity the switch isn't noticeable and this approach
  // allows us to use only a single set of shadow images at a time.
  if (style == STYLE_ACTIVE) {
    UpdateImagesForStyle();
    // Opacity was baked into inactive image, start opacity low to match.
    image_grid_->layer()->SetOpacity(kInactiveShadowOpacity);
  }

  {
    // Property sets within this scope will be implicitly animated.
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.AddObserver(this);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kShadowAnimationDurationMs));
    switch (style_) {
      case STYLE_ACTIVE:
        image_grid_->layer()->SetOpacity(kActiveShadowOpacity);
        break;
      case STYLE_INACTIVE:
        image_grid_->layer()->SetOpacity(kInactiveShadowOpacity);
        break;
      default:
        NOTREACHED() << "Unhandled style " << style_;
        break;
    }
  }
}

void Shadow::OnImplicitAnimationsCompleted() {
  // If we just finished going inactive, switch images.  This doesn't cause
  // a visual pop because the inactive image opacity is so low.
  if (style_ == STYLE_INACTIVE) {
    UpdateImagesForStyle();
    // Opacity is baked into inactive image, so set fully opaque.
    image_grid_->layer()->SetOpacity(1.0f);
  }
}

void Shadow::UpdateImagesForStyle() {
  ResourceBundle& res = ResourceBundle::GetSharedInstance();
  switch (style_) {
    case STYLE_ACTIVE:
      image_grid_->SetImages(
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_TOP_LEFT),
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_TOP),
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_TOP_RIGHT),
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_LEFT),
          NULL,
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_RIGHT),
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_BOTTOM_LEFT),
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_BOTTOM),
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_BOTTOM_RIGHT));
      break;
    case STYLE_INACTIVE:
      image_grid_->SetImages(
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_TOP_LEFT),
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_TOP),
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_TOP_RIGHT),
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_LEFT),
          NULL,
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_RIGHT),
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_BOTTOM_LEFT),
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_BOTTOM),
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_BOTTOM_RIGHT));
      break;
    case STYLE_SMALL:
      image_grid_->SetImages(
          &res.GetImageNamed(IDR_WINDOW_BUBBLE_SHADOW_SMALL_TOP_LEFT),
          &res.GetImageNamed(IDR_WINDOW_BUBBLE_SHADOW_SMALL_TOP),
          &res.GetImageNamed(IDR_WINDOW_BUBBLE_SHADOW_SMALL_TOP_RIGHT),
          &res.GetImageNamed(IDR_WINDOW_BUBBLE_SHADOW_SMALL_LEFT),
          NULL,
          &res.GetImageNamed(IDR_WINDOW_BUBBLE_SHADOW_SMALL_RIGHT),
          &res.GetImageNamed(IDR_WINDOW_BUBBLE_SHADOW_SMALL_BOTTOM_LEFT),
          &res.GetImageNamed(IDR_WINDOW_BUBBLE_SHADOW_SMALL_BOTTOM),
          &res.GetImageNamed(IDR_WINDOW_BUBBLE_SHADOW_SMALL_BOTTOM_RIGHT));
      break;
    default:
      NOTREACHED() << "Unhandled style " << style_;
      break;
  }

  // Update interior inset for style.
  interior_inset_ = GetInteriorInsetForStyle(style_);

  // Image sizes may have changed.
  UpdateImageGridBounds();
}

void Shadow::UpdateImageGridBounds() {
  // Update bounds based on content bounds and image sizes.
  gfx::Rect image_grid_bounds = content_bounds_;
  image_grid_bounds.Inset(interior_inset_, interior_inset_);
  image_grid_->SetContentBounds(image_grid_bounds);
}

}  // namespace wm
