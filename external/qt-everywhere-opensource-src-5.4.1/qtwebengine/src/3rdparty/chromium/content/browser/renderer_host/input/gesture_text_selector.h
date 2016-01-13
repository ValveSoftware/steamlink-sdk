// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_TEXT_SELECTOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_TEXT_SELECTOR_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace ui {
struct GestureEventData;
class MotionEvent;
}

namespace content {
class GestureTextSelectorTest;

// Interface with which GestureTextSelector can select, unselect, show
// selection handles, or long press.
class CONTENT_EXPORT GestureTextSelectorClient {
 public:
  virtual ~GestureTextSelectorClient() {}

  virtual void ShowSelectionHandlesAutomatically() = 0;
  virtual void SelectRange(float x1, float y1, float x2, float y2) = 0;
  virtual void Unselect() = 0;
  virtual void LongPress(base::TimeTicks time, float x, float y) = 0;
};

// A class to handle gesture-based text selection, such as when clicking first
// button on stylus input. It also generates a synthetic long press gesture on
// tap so that a word can be selected or the contextual menu can be shown.
class CONTENT_EXPORT GestureTextSelector {
 public:
  explicit GestureTextSelector(GestureTextSelectorClient* client);
  virtual ~GestureTextSelector();

  // This should be called after gesture detection but before associated
  // gestures are dispatched. Returns whether it will consume |event|.
  bool OnTouchEvent(const ui::MotionEvent& event);

  // Returns whether it will consume the event.
  bool OnGestureEvent(const ui::GestureEventData& gesture);

 private:
  friend class GestureTextSelectorTest;
  FRIEND_TEST_ALL_PREFIXES(GestureTextSelectorTest,
                           ShouldStartTextSelection);

  static bool ShouldStartTextSelection(const ui::MotionEvent& event);

  GestureTextSelectorClient* client_;
  bool text_selection_triggered_;
  float anchor_x_;
  float anchor_y_;

  DISALLOW_COPY_AND_ASSIGN(GestureTextSelector);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_TEXT_SELECTOR_H_
