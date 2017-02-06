// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "ui/views/views_export.h"
#include "ui/wm/core/masked_window_targeter.h"

namespace aura {
class Window;
}

namespace views {

class BubbleDialogDelegateView;

// A convenient window-targeter that uses a mask based on the content-bounds of
// the bubble-frame.
class VIEWS_EXPORT BubbleWindowTargeter
    : public NON_EXPORTED_BASE(wm::MaskedWindowTargeter) {
 public:
  explicit BubbleWindowTargeter(BubbleDialogDelegateView* bubble);
  ~BubbleWindowTargeter() override;

 private:
  // wm::MaskedWindowTargeter:
  bool GetHitTestMask(aura::Window* window, gfx::Path* mask) const override;

  views::BubbleDialogDelegateView* bubble_;

  DISALLOW_COPY_AND_ASSIGN(BubbleWindowTargeter);
};

}  // namespace views
