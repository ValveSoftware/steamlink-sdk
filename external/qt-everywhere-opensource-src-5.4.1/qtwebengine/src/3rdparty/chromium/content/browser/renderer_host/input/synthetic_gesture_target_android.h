// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"

namespace content {

class ContentViewCoreImpl;

class SyntheticGestureTargetAndroid : public SyntheticGestureTargetBase {
 public:
  SyntheticGestureTargetAndroid(
      RenderWidgetHostImpl* host,
      base::android::ScopedJavaLocalRef<jobject> touch_event_synthesizer);
  virtual ~SyntheticGestureTargetAndroid();

  static bool RegisterTouchEventSynthesizer(JNIEnv* env);

  // SyntheticGestureTargetBase:
  virtual void DispatchWebTouchEventToPlatform(
      const blink::WebTouchEvent& web_touch,
      const ui::LatencyInfo& latency_info) OVERRIDE;
  virtual void DispatchWebMouseWheelEventToPlatform(
      const blink::WebMouseWheelEvent& web_wheel,
      const ui::LatencyInfo& latency_info) OVERRIDE;
  virtual void DispatchWebMouseEventToPlatform(
      const blink::WebMouseEvent& web_mouse,
      const ui::LatencyInfo& latency_info) OVERRIDE;

  // SyntheticGestureTarget:
  virtual SyntheticGestureParams::GestureSourceType
      GetDefaultSyntheticGestureSourceType() const OVERRIDE;

  virtual float GetTouchSlopInDips() const OVERRIDE;

  virtual float GetMinScalingSpanInDips() const OVERRIDE;

 private:
  // Enum values below need to be kept in sync with TouchEventSynthesizer.java
  enum Action {
    ActionInvalid = -1,
    ActionStart = 0,
    ActionMove = 1,
    ActionCancel = 2,
    ActionEnd = 3
  };

  void TouchSetPointer(JNIEnv* env, int index, int x, int y, int id);
  void TouchInject(
      JNIEnv* env, Action action, int pointer_count, int64 time_in_ms);

  base::android::ScopedJavaGlobalRef<jobject> touch_event_synthesizer_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticGestureTargetAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_ANDROID_H_
