// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/skip_image_canvas.h"

#include "third_party/skia/include/core/SkShader.h"

namespace cc {

SkipImageCanvas::SkipImageCanvas(SkCanvas* canvas)
    : SkPaintFilterCanvas(canvas) {}

bool SkipImageCanvas::onFilter(SkTCopyOnFirstWrite<SkPaint>* paint,
                               Type type) const {
  if (type == kBitmap_Type)
    return false;

  SkShader* shader = (*paint) ? (*paint)->getShader() : nullptr;
  return !shader || !shader->isAImage();
}

void SkipImageCanvas::onDrawPicture(const SkPicture* picture,
                                    const SkMatrix* matrix,
                                    const SkPaint* paint) {
  SkTCopyOnFirstWrite<SkPaint> filteredPaint(paint);

  // To filter nested draws, we must unfurl pictures at this stage.
  if (onFilter(&filteredPaint, kPicture_Type))
    SkCanvas::onDrawPicture(picture, matrix, filteredPaint);
}

}  // namespace cc
