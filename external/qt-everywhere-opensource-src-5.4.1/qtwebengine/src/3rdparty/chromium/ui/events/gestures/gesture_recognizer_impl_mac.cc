// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "ui/events/gestures/gesture_recognizer.h"

namespace ui {

namespace {

// Stub implementation of GestureRecognizer for Mac. Currently only serves to
// provide a no-op implementation of TransferEventsTo().
class GestureRecognizerImplMac : public GestureRecognizer {
 public:
  GestureRecognizerImplMac() {}
  virtual ~GestureRecognizerImplMac() {}

 private:
  virtual Gestures* ProcessTouchEventForGesture(
      const TouchEvent& event,
      ui::EventResult result,
      GestureConsumer* consumer) OVERRIDE {
    return NULL;
  }
  virtual bool CleanupStateForConsumer(GestureConsumer* consumer) OVERRIDE {
    return false;
  }
  virtual GestureConsumer* GetTouchLockedTarget(
      const TouchEvent& event) OVERRIDE {
    return NULL;
  }
  virtual GestureConsumer* GetTargetForGestureEvent(
      const GestureEvent& event) OVERRIDE {
    return NULL;
  }
  virtual GestureConsumer* GetTargetForLocation(const gfx::PointF& location,
                                                int source_device_id) OVERRIDE {
    return NULL;
  }
  virtual void TransferEventsTo(GestureConsumer* current_consumer,
                                GestureConsumer* new_consumer) OVERRIDE {}
  virtual bool GetLastTouchPointForTarget(GestureConsumer* consumer,
                                          gfx::PointF* point) OVERRIDE {
    return false;
  }
  virtual bool CancelActiveTouches(GestureConsumer* consumer) OVERRIDE {
    return false;
  }
  virtual void AddGestureEventHelper(GestureEventHelper* helper) OVERRIDE {}
  virtual void RemoveGestureEventHelper(GestureEventHelper* helper) OVERRIDE {}

  DISALLOW_COPY_AND_ASSIGN(GestureRecognizerImplMac);
};

}  // namespace

// static
GestureRecognizer* GestureRecognizer::Get() {
  CR_DEFINE_STATIC_LOCAL(GestureRecognizerImplMac, instance, ());
  return &instance;
}

}  // namespace ui
