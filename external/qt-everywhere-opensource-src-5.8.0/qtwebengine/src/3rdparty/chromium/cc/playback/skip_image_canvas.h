// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_SKIP_IMAGE_CANVAS_H_
#define CC_PLAYBACK_SKIP_IMAGE_CANVAS_H_

#include "third_party/skia/include/utils/SkPaintFilterCanvas.h"

namespace cc {

class SkipImageCanvas : public SkPaintFilterCanvas {
 public:
  explicit SkipImageCanvas(SkCanvas* canvas);

 private:
  bool onFilter(SkTCopyOnFirstWrite<SkPaint>* paint, Type type) const override;

  void onDrawPicture(const SkPicture* picture,
                     const SkMatrix* matrix,
                     const SkPaint* paint) override;
};

}  // namespace cc

#endif  // CC_PLAYBACK_SKIP_IMAGE_CANVAS_H_
