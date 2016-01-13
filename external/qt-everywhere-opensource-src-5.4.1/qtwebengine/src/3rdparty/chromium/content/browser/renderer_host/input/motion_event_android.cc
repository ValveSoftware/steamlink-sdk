// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/motion_event_android.h"

#include "base/android/jni_android.h"
#include "jni/MotionEvent_jni.h"

using base::android::AttachCurrentThread;
using namespace JNI_MotionEvent;

namespace content {
namespace {

int ToAndroidAction(MotionEventAndroid::Action action) {
  switch (action) {
    case MotionEventAndroid::ACTION_DOWN:
      return ACTION_DOWN;
    case MotionEventAndroid::ACTION_UP:
      return ACTION_UP;
    case MotionEventAndroid::ACTION_MOVE:
      return ACTION_MOVE;
    case MotionEventAndroid::ACTION_CANCEL:
      return ACTION_CANCEL;
    case MotionEventAndroid::ACTION_POINTER_DOWN:
      return ACTION_POINTER_DOWN;
    case MotionEventAndroid::ACTION_POINTER_UP:
      return ACTION_POINTER_UP;
  };
  NOTREACHED() << "Invalid Android MotionEvent type for gesture detection: "
               << action;
  return ACTION_CANCEL;
}

MotionEventAndroid::Action FromAndroidAction(int android_action) {
  switch (android_action) {
    case ACTION_DOWN:
      return MotionEventAndroid::ACTION_DOWN;
    case ACTION_UP:
      return MotionEventAndroid::ACTION_UP;
    case ACTION_MOVE:
      return MotionEventAndroid::ACTION_MOVE;
    case ACTION_CANCEL:
      return MotionEventAndroid::ACTION_CANCEL;
    case ACTION_POINTER_DOWN:
      return MotionEventAndroid::ACTION_POINTER_DOWN;
    case ACTION_POINTER_UP:
      return MotionEventAndroid::ACTION_POINTER_UP;
    default:
      NOTREACHED() << "Invalid Android MotionEvent type for gesture detection: "
                   << android_action;
  };
  return MotionEventAndroid::ACTION_CANCEL;
}

MotionEventAndroid::ToolType FromAndroidToolType(int android_tool_type) {
  switch (android_tool_type) {
    case TOOL_TYPE_UNKNOWN:
      return MotionEventAndroid::TOOL_TYPE_UNKNOWN;
    case TOOL_TYPE_FINGER:
      return MotionEventAndroid::TOOL_TYPE_FINGER;
    case TOOL_TYPE_STYLUS:
      return MotionEventAndroid::TOOL_TYPE_STYLUS;
    case TOOL_TYPE_MOUSE:
      return MotionEventAndroid::TOOL_TYPE_MOUSE;
    default:
      NOTREACHED() << "Invalid Android MotionEvent tool type: "
                   << android_tool_type;
  };
  return MotionEventAndroid::TOOL_TYPE_UNKNOWN;
}

int FromAndroidButtonState(int button_state) {
  int result = 0;
  if ((button_state & BUTTON_BACK) != 0)
    result |= MotionEventAndroid::BUTTON_BACK;
  if ((button_state & BUTTON_FORWARD) != 0)
    result |= MotionEventAndroid::BUTTON_FORWARD;
  if ((button_state & BUTTON_PRIMARY) != 0)
    result |= MotionEventAndroid::BUTTON_PRIMARY;
  if ((button_state & BUTTON_SECONDARY) != 0)
    result |= MotionEventAndroid::BUTTON_SECONDARY;
  if ((button_state & BUTTON_TERTIARY) != 0)
    result |= MotionEventAndroid::BUTTON_TERTIARY;
  return result;
}

int64 ToAndroidTime(base::TimeTicks time) {
  return (time - base::TimeTicks()).InMilliseconds();
}

base::TimeTicks FromAndroidTime(int64 time_ms) {
  return base::TimeTicks() + base::TimeDelta::FromMilliseconds(time_ms);
}

}  // namespace

MotionEventAndroid::MotionEventAndroid(float pix_to_dip,
                                       JNIEnv* env,
                                       jobject event,
                                       jlong time_ms,
                                       jint android_action,
                                       jint pointer_count,
                                       jint history_size,
                                       jint action_index,
                                       jfloat pos_x_0_pixels,
                                       jfloat pos_y_0_pixels,
                                       jfloat pos_x_1_pixels,
                                       jfloat pos_y_1_pixels,
                                       jint pointer_id_0,
                                       jint pointer_id_1,
                                       jfloat touch_major_0_pixels,
                                       jfloat touch_major_1_pixels,
                                       jfloat raw_pos_x_pixels,
                                       jfloat raw_pos_y_pixels,
                                       jint android_tool_type_0,
                                       jint android_tool_type_1,
                                       jint android_button_state)
    : cached_time_(FromAndroidTime(time_ms)),
      cached_action_(FromAndroidAction(android_action)),
      cached_pointer_count_(pointer_count),
      cached_history_size_(history_size),
      cached_action_index_(action_index),
      cached_button_state_(FromAndroidButtonState(android_button_state)),
      pix_to_dip_(pix_to_dip),
      should_recycle_(false) {
  DCHECK_GT(pointer_count, 0);
  DCHECK_GE(history_size, 0);

  event_.Reset(env, event);
  DCHECK(event_.obj());

  cached_positions_[0] = ToDips(gfx::PointF(pos_x_0_pixels, pos_y_0_pixels));
  cached_positions_[1] = ToDips(gfx::PointF(pos_x_1_pixels, pos_y_1_pixels));
  cached_pointer_ids_[0] = pointer_id_0;
  cached_pointer_ids_[1] = pointer_id_1;
  cached_touch_majors_[0] = ToDips(touch_major_0_pixels);
  cached_touch_majors_[1] = ToDips(touch_major_1_pixels);
  cached_raw_position_offset_ =
      ToDips(gfx::PointF(raw_pos_x_pixels, raw_pos_y_pixels)) -
      cached_positions_[0];
  cached_tool_types_[0] = FromAndroidToolType(android_tool_type_0);
  cached_tool_types_[1] = FromAndroidToolType(android_tool_type_1);
}

MotionEventAndroid::MotionEventAndroid(float pix_to_dip,
                                       JNIEnv* env,
                                       jobject event)
    : cached_time_(FromAndroidTime(Java_MotionEvent_getEventTime(env, event))),
      cached_action_(
          FromAndroidAction(Java_MotionEvent_getActionMasked(env, event))),
      cached_pointer_count_(Java_MotionEvent_getPointerCount(env, event)),
      cached_history_size_(Java_MotionEvent_getHistorySize(env, event)),
      cached_action_index_(Java_MotionEvent_getActionIndex(env, event)),
      cached_button_state_(
          FromAndroidButtonState(Java_MotionEvent_getButtonState(env, event))),
      pix_to_dip_(pix_to_dip),
      should_recycle_(true) {
  event_.Reset(env, event);
  DCHECK(event_.obj());

  for (size_t i = 0; i < MAX_POINTERS_TO_CACHE; ++i) {
    if (i < cached_pointer_count_) {
      cached_positions_[i] =
          ToDips(gfx::PointF(Java_MotionEvent_getXF_I(env, event, i),
                             Java_MotionEvent_getYF_I(env, event, i)));
      cached_pointer_ids_[i] = Java_MotionEvent_getPointerId(env, event, i);
      cached_touch_majors_[i] =
          ToDips(Java_MotionEvent_getTouchMajorF_I(env, event, i));
      cached_tool_types_[i] =
          FromAndroidToolType(Java_MotionEvent_getToolType(env, event, i));
    } else {
      cached_pointer_ids_[i] = 0;
      cached_touch_majors_[i] = 0.f;
      cached_tool_types_[i] = MotionEvent::TOOL_TYPE_UNKNOWN;
    }
  }

  cached_raw_position_offset_ =
      ToDips(gfx::PointF(Java_MotionEvent_getRawX(env, event),
                         Java_MotionEvent_getRawY(env, event))) -
      cached_positions_[0];
}

MotionEventAndroid::MotionEventAndroid(const MotionEventAndroid& other)
    : event_(Obtain(other)),
      cached_time_(other.cached_time_),
      cached_action_(other.cached_action_),
      cached_pointer_count_(other.cached_pointer_count_),
      cached_history_size_(other.cached_history_size_),
      cached_action_index_(other.cached_action_index_),
      cached_raw_position_offset_(other.cached_raw_position_offset_),
      cached_button_state_(other.cached_button_state_),
      pix_to_dip_(other.pix_to_dip_),
      should_recycle_(true) {
  DCHECK(event_.obj());
  for (size_t i = 0; i < MAX_POINTERS_TO_CACHE; ++i) {
    cached_positions_[i] = other.cached_positions_[i];
    cached_pointer_ids_[i] = other.cached_pointer_ids_[i];
    cached_touch_majors_[i] = other.cached_touch_majors_[i];
    cached_tool_types_[i] = other.cached_tool_types_[i];
  }
}

MotionEventAndroid::~MotionEventAndroid() {
  if (should_recycle_)
    Java_MotionEvent_recycle(AttachCurrentThread(), event_.obj());
}

int MotionEventAndroid::GetId() const {
  return 0;
}

MotionEventAndroid::Action MotionEventAndroid::GetAction() const {
  return cached_action_;
}

int MotionEventAndroid::GetActionIndex() const {
  return cached_action_index_;
}

size_t MotionEventAndroid::GetPointerCount() const {
  return cached_pointer_count_;
}

int MotionEventAndroid::GetPointerId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointer_ids_[pointer_index];
  return Java_MotionEvent_getPointerId(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

float MotionEventAndroid::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_positions_[pointer_index].x();
  return ToDips(Java_MotionEvent_getXF_I(
      AttachCurrentThread(), event_.obj(), pointer_index));
}

float MotionEventAndroid::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_positions_[pointer_index].y();
  return ToDips(Java_MotionEvent_getYF_I(
      AttachCurrentThread(), event_.obj(), pointer_index));
}

float MotionEventAndroid::GetRawX(size_t pointer_index) const {
  return GetX(pointer_index) + cached_raw_position_offset_.x();
}

float MotionEventAndroid::GetRawY(size_t pointer_index) const {
  return GetY(pointer_index) + cached_raw_position_offset_.y();
}

float MotionEventAndroid::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_touch_majors_[pointer_index];
  return ToDips(Java_MotionEvent_getTouchMajorF_I(
      AttachCurrentThread(), event_.obj(), pointer_index));
}

float MotionEventAndroid::GetPressure(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  return Java_MotionEvent_getPressureF_I(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

base::TimeTicks MotionEventAndroid::GetEventTime() const {
  return cached_time_;
}

size_t MotionEventAndroid::GetHistorySize() const {
  return cached_history_size_;
}

base::TimeTicks MotionEventAndroid::GetHistoricalEventTime(
    size_t historical_index) const {
  return FromAndroidTime(Java_MotionEvent_getHistoricalEventTime(
      AttachCurrentThread(), event_.obj(), historical_index));
}

float MotionEventAndroid::GetHistoricalTouchMajor(
    size_t pointer_index,
    size_t historical_index) const {
  return ToDips(Java_MotionEvent_getHistoricalTouchMajorF_I_I(
      AttachCurrentThread(), event_.obj(), pointer_index, historical_index));
}

float MotionEventAndroid::GetHistoricalX(size_t pointer_index,
                                         size_t historical_index) const {
  return ToDips(Java_MotionEvent_getHistoricalXF_I_I(
      AttachCurrentThread(), event_.obj(), pointer_index, historical_index));
}

float MotionEventAndroid::GetHistoricalY(size_t pointer_index,
                                         size_t historical_index) const {
  return ToDips(Java_MotionEvent_getHistoricalYF_I_I(
      AttachCurrentThread(), event_.obj(), pointer_index, historical_index));
}

ui::MotionEvent::ToolType MotionEventAndroid::GetToolType(
    size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_tool_types_[pointer_index];
  return FromAndroidToolType(Java_MotionEvent_getToolType(
      AttachCurrentThread(), event_.obj(), pointer_index));
}

int MotionEventAndroid::GetButtonState() const {
  return cached_button_state_;
}

scoped_ptr<ui::MotionEvent> MotionEventAndroid::Clone() const {
  return scoped_ptr<MotionEvent>(new MotionEventAndroid(*this));
}

scoped_ptr<ui::MotionEvent> MotionEventAndroid::Cancel() const {
  // The input coordinates to |MotionEventAndroid| are always in device pixels,
  // but the cached coordinates are in DIPs.
  const gfx::PointF position_pixels =
      gfx::ScalePoint(cached_positions_[0], 1.f / pix_to_dip_);
  return scoped_ptr<MotionEvent>(
      new MotionEventAndroid(pix_to_dip_,
                             AttachCurrentThread(),
                             Obtain(GetDownTime(),
                                    GetEventTime(),
                                    MotionEventAndroid::ACTION_CANCEL,
                                    position_pixels.x(),
                                    position_pixels.y()).obj()));
}

float MotionEventAndroid::GetTouchMinor(size_t pointer_index) const {
  return ToDips(Java_MotionEvent_getTouchMinorF_I(
      AttachCurrentThread(), event_.obj(), pointer_index));
}

float MotionEventAndroid::GetOrientation() const {
  return Java_MotionEvent_getOrientationF(AttachCurrentThread(), event_.obj());
}

base::TimeTicks MotionEventAndroid::GetDownTime() const {
  return FromAndroidTime(
      Java_MotionEvent_getDownTime(AttachCurrentThread(), event_.obj()));
}

float MotionEventAndroid::ToDips(float pixels) const {
  return pixels * pix_to_dip_;
}

gfx::PointF MotionEventAndroid::ToDips(const gfx::PointF& point_pixels) const {
  return gfx::ScalePoint(point_pixels, pix_to_dip_);
}

// static
bool MotionEventAndroid::RegisterMotionEventAndroid(JNIEnv* env) {
  return JNI_MotionEvent::RegisterNativesImpl(env);
}

// static
base::android::ScopedJavaLocalRef<jobject> MotionEventAndroid::Obtain(
    const MotionEventAndroid& event) {
  return Java_MotionEvent_obtainAVME_AVME(AttachCurrentThread(),
                                          event.event_.obj());
}

// static
base::android::ScopedJavaLocalRef<jobject> MotionEventAndroid::Obtain(
    base::TimeTicks down_time,
    base::TimeTicks event_time,
    Action action,
    float x_pixels,
    float y_pixels) {
  return Java_MotionEvent_obtainAVME_J_J_I_F_F_I(AttachCurrentThread(),
                                                 ToAndroidTime(down_time),
                                                 ToAndroidTime(event_time),
                                                 ToAndroidAction(action),
                                                 x_pixels,
                                                 y_pixels,
                                                 0);
}

}  // namespace content
