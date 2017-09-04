// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositing_recorder.h"

#include "cc/playback/compositing_display_item.h"
#include "cc/playback/display_item_list.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"

namespace ui {

CompositingRecorder::CompositingRecorder(const PaintContext& context,
                                         uint8_t alpha,
                                         bool lcd_text_requires_opaque_layer)
    : context_(context),
      saved_(alpha < 255) {
  if (!saved_)
    return;

  context_.list_->CreateAndAppendPairedBeginItem<cc::CompositingDisplayItem>(
      alpha, SkXfermode::kSrcOver_Mode, nullptr /* no bounds */,
      nullptr /* no color filter */, lcd_text_requires_opaque_layer);
}

CompositingRecorder::~CompositingRecorder() {
  if (!saved_)
    return;

  context_.list_->CreateAndAppendPairedEndItem<cc::EndCompositingDisplayItem>();
}

}  // namespace ui
