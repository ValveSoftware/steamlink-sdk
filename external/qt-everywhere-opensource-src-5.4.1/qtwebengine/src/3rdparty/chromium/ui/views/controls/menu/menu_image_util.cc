// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_image_util.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace {

// Size of the radio button inciator.
const int kSelectedIndicatorSize = 5;
const int kIndicatorSize = 10;

// Used for the radio indicator. See theme_draw for details.
const double kGradientStop = .5;
const SkColor kGradient0 = SkColorSetRGB(255, 255, 255);
const SkColor kGradient1 = SkColorSetRGB(255, 255, 255);
const SkColor kGradient2 = SkColorSetRGB(0xD8, 0xD8, 0xD8);
const SkColor kBaseStroke = SkColorSetRGB(0x8F, 0x8F, 0x8F);
const SkColor kRadioButtonIndicatorGradient0 = SkColorSetRGB(0, 0, 0);
const SkColor kRadioButtonIndicatorGradient1 = SkColorSetRGB(0x83, 0x83, 0x83);
const SkColor kIndicatorStroke = SkColorSetRGB(0, 0, 0);

class RadioButtonImageSource : public gfx::CanvasImageSource {
 public:
  explicit RadioButtonImageSource(bool selected)
      : CanvasImageSource(gfx::Size(kIndicatorSize + 2, kIndicatorSize + 2),
                          false),
        selected_(selected) {
  }
  virtual ~RadioButtonImageSource() {}

  virtual void Draw(gfx::Canvas* canvas) OVERRIDE {
    canvas->Translate(gfx::Vector2d(1, 1));

    SkPoint gradient_points[3];
    gradient_points[0].iset(0, 0);
    gradient_points[1].iset(0,
                            static_cast<int>(kIndicatorSize * kGradientStop));
    gradient_points[2].iset(0, kIndicatorSize);
    SkColor gradient_colors[3] = { kGradient0, kGradient1, kGradient2 };
    skia::RefPtr<SkShader> shader = skia::AdoptRef(
        SkGradientShader::CreateLinear(
            gradient_points, gradient_colors, NULL, arraysize(gradient_points),
            SkShader::kClamp_TileMode));
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    paint.setShader(shader.get());
    int radius = kIndicatorSize / 2;
    canvas->sk_canvas()->drawCircle(radius, radius, radius, paint);
    paint.setStrokeWidth(SkIntToScalar(0));
    paint.setShader(NULL);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(kBaseStroke);
    canvas->sk_canvas()->drawCircle(radius, radius, radius, paint);

    if (selected_) {
      SkPoint selected_gradient_points[2];
      selected_gradient_points[0].iset(0, 0);
      selected_gradient_points[1].iset(0, kSelectedIndicatorSize);
      SkColor selected_gradient_colors[2] = { kRadioButtonIndicatorGradient0,
                                              kRadioButtonIndicatorGradient1 };
      shader = skia::AdoptRef(
          SkGradientShader::CreateLinear(
              selected_gradient_points, selected_gradient_colors, NULL,
              arraysize(selected_gradient_points), SkShader::kClamp_TileMode));
      paint.setShader(shader.get());
      paint.setStyle(SkPaint::kFill_Style);
      canvas->sk_canvas()->drawCircle(radius, radius,
                                      kSelectedIndicatorSize / 2, paint);

      paint.setStrokeWidth(SkIntToScalar(0));
      paint.setShader(NULL);
      paint.setStyle(SkPaint::kStroke_Style);
      paint.setColor(kIndicatorStroke);
      canvas->sk_canvas()->drawCircle(radius, radius,
                                      kSelectedIndicatorSize / 2, paint);
    }
  }

 private:
  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(RadioButtonImageSource);
};

class SubmenuArrowImageSource : public gfx::CanvasImageSource {
 public:
  SubmenuArrowImageSource(int image_id)
      : gfx::CanvasImageSource(ui::ResourceBundle::GetSharedInstance().
            GetImageNamed(image_id).ToImageSkia()->size(), false),
        image_id_(image_id) {}
  virtual ~SubmenuArrowImageSource() {}

  virtual void Draw(gfx::Canvas* canvas) OVERRIDE {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::ImageSkia* r = rb.GetImageNamed(image_id_).ToImageSkia();
    canvas->Scale(-1, 1);
    canvas->DrawImageInt(*r, - r->width(), 0);
  }

 private:
  static gfx::Size GetSubmenuArrowSize() {
    return ui::ResourceBundle::GetSharedInstance()
        .GetImageNamed(IDR_MENU_HIERARCHY_ARROW).ToImageSkia()->size();
  }

  int image_id_;

  DISALLOW_COPY_AND_ASSIGN(SubmenuArrowImageSource);
};

gfx::ImageSkia GetRtlSubmenuArrowImage(bool rtl,
                                       bool dark_background) {
  int image_id = dark_background ? IDR_MENU_HIERARCHY_ARROW_DARK_BACKGROUND :
                                   IDR_MENU_HIERARCHY_ARROW;

  if (!rtl) {
    return ui::ResourceBundle::GetSharedInstance().GetImageNamed(image_id).
        AsImageSkia();
  }

  static gfx::ImageSkia* kRtlArrow = NULL;
  static gfx::ImageSkia* kRtlArrowDarkBg = NULL;
  gfx::ImageSkia** image = dark_background ? &kRtlArrowDarkBg : &kRtlArrow;

  if (!*image) {
    SubmenuArrowImageSource* source = new SubmenuArrowImageSource(image_id);
    *image = new gfx::ImageSkia(source, source->size());
  }

  return **image;
}

}  // namespace

namespace views {

gfx::ImageSkia GetMenuCheckImage(bool dark_background) {
  int image_id = dark_background ? IDR_MENU_CHECK_CHECKED_DARK_BACKGROUND :
                                   IDR_MENU_CHECK_CHECKED;
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(image_id).
      AsImageSkia();
}

gfx::ImageSkia GetRadioButtonImage(bool selected) {
  int image_id = selected ? IDR_MENU_RADIO_SELECTED : IDR_MENU_RADIO_EMPTY;
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(image_id).
      AsImageSkia();
}

gfx::ImageSkia GetSubmenuArrowImage(bool dark_background) {
  return GetRtlSubmenuArrowImage(base::i18n::IsRTL(), dark_background);
}

}  // namespace views
