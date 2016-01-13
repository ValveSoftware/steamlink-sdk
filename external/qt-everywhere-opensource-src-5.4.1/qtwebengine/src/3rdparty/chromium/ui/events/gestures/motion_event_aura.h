// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_UI_MOTION_EVENT_H_
#define UI_EVENTS_GESTURE_DETECTION_UI_MOTION_EVENT_H_

#include "ui/events/gesture_detection/motion_event.h"

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/events_export.h"
#include "ui/events/gestures/gesture_sequence.h"

namespace ui {

// Implementation of MotionEvent which takes a stream of ui::TouchEvents.
class EVENTS_EXPORT MotionEventAura : public MotionEvent {
 public:
  MotionEventAura();
  virtual ~MotionEventAura();

  void OnTouch(const TouchEvent& touch);

  // MotionEvent implementation.
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
  virtual base::TimeTicks GetHistoricalEventTime(size_t historical_index) const
      OVERRIDE;
  virtual float GetHistoricalTouchMajor(size_t pointer_index,
                                        size_t historical_index) const OVERRIDE;
  virtual float GetHistoricalX(size_t pointer_index,
                               size_t historical_index) const OVERRIDE;
  virtual float GetHistoricalY(size_t pointer_index,
                               size_t historical_index) const OVERRIDE;
  virtual ToolType GetToolType(size_t pointer_index) const OVERRIDE;
  virtual int GetButtonState() const OVERRIDE;

  virtual scoped_ptr<MotionEvent> Clone() const OVERRIDE;
  virtual scoped_ptr<MotionEvent> Cancel() const OVERRIDE;

  int GetSourceDeviceId(size_t pointer_index) const;

  // We can't cleanup removed touch points immediately upon receipt of a
  // TouchCancel or TouchRelease, as the MotionEvent needs to be able to report
  // information about those touch events. Once the MotionEvent has been
  // processed, we call CleanupRemovedTouchPoints to do the required
  // book-keeping.
  void CleanupRemovedTouchPoints(const TouchEvent& event);

 private:
  struct PointData {
    PointData();
    float x;
    float y;
    float raw_x;
    float raw_y;
    int touch_id;
    float pressure;
    int source_device_id;
    float major_radius;
  };

  MotionEventAura(
      size_t pointer_count,
      const base::TimeTicks& last_touch_time,
      Action cached_action,
      int cached_action_index,
      const PointData (&active_touches)[GestureSequence::kMaxGesturePoints]);

  static PointData GetPointDataFromTouchEvent(const TouchEvent& touch);
  void AddTouch(const TouchEvent& touch);
  void UpdateTouch(const TouchEvent& touch);
  void UpdateCachedAction(const TouchEvent& touch);
  size_t GetIndexFromId(int id) const;

  size_t pointer_count_;
  base::TimeTicks last_touch_time_;
  Action cached_action_;
  // The index of the touch responsible for last ACTION_POINTER_DOWN or
  // ACTION_POINTER_UP. -1 if no such action has occurred.
  int cached_action_index_;

  // We want constant time indexing by pointer_index, and fast indexing by id.
  // TODO(tdresser): figure out which constant to use here.
  PointData active_touches_[GestureSequence::kMaxGesturePoints];

  DISALLOW_COPY_AND_ASSIGN(MotionEventAura);
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_UI_MOTION_EVENT_H_
