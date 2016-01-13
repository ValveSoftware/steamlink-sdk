// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_WEB_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_WEB_H_

#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/gesture_detection/motion_event.h"

namespace content {

// Implementation of ui::MotionEvent wrapping a WebTouchEvent.
class MotionEventWeb : public ui::MotionEvent {
 public:
  explicit MotionEventWeb(const blink::WebTouchEvent& event);
  virtual ~MotionEventWeb();

  // ui::MotionEvent
  virtual int GetId() const OVERRIDE;
  virtual Action GetAction() const OVERRIDE;
  virtual int GetActionIndex() const OVERRIDE;
  virtual size_t GetPointerCount() const OVERRIDE;
  virtual int GetPointerId(size_t pointer_index) const OVERRIDE;
  virtual float GetX(size_t pointer_index) const OVERRIDE;
  virtual float GetY(size_t pointer_index) const OVERRIDE;
  virtual float GetRawX(size_t pointer_index) const OVERRIDE;
  virtual float GetRawY(size_t pointer_index) const OVERRIDE;
  virtual float GetTouchMajor(size_t pointer_index) const OVERRIDE;
  virtual float GetPressure(size_t pointer_index) const OVERRIDE;
  virtual base::TimeTicks GetEventTime() const OVERRIDE;
  virtual size_t GetHistorySize() const OVERRIDE;
  virtual base::TimeTicks GetHistoricalEventTime(
      size_t historical_index) const OVERRIDE;
  virtual float GetHistoricalTouchMajor(
      size_t pointer_index,
      size_t historical_index) const OVERRIDE;
  virtual float GetHistoricalX(
      size_t pointer_index,
      size_t historical_index) const OVERRIDE;
  virtual float GetHistoricalY(
      size_t pointer_index,
      size_t historical_index) const OVERRIDE;
  virtual ToolType GetToolType(size_t pointer_index) const OVERRIDE;
  virtual int GetButtonState() const OVERRIDE;
  virtual scoped_ptr<MotionEvent> Clone() const OVERRIDE;
  virtual scoped_ptr<MotionEvent> Cancel() const OVERRIDE;

 private:
  blink::WebTouchEvent event_;
  Action cached_action_;
  int cached_action_index_;

  DISALLOW_COPY_AND_ASSIGN(MotionEventWeb);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_WEB_H_
