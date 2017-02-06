// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/popup_touch_handle_drawable.h"

#include "content/public/browser/android/content_view_core.h"
#include "jni/PopupTouchHandleDrawable_jni.h"

namespace content {

// static
std::unique_ptr<PopupTouchHandleDrawable> PopupTouchHandleDrawable::Create(
    ContentViewCore* content_view_core) {
  DCHECK(content_view_core);
  base::android::ScopedJavaLocalRef<jobject> content_view_core_obj =
      content_view_core->GetJavaObject();
  if (content_view_core_obj.is_null())
    return nullptr;
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> drawable_obj(
      Java_PopupTouchHandleDrawable_create(env, content_view_core_obj.obj()));
  return std::unique_ptr<PopupTouchHandleDrawable>(new PopupTouchHandleDrawable(
      env, drawable_obj.obj(), content_view_core->GetDpiScale()));
}

PopupTouchHandleDrawable::PopupTouchHandleDrawable(JNIEnv* env,
                                                   jobject obj,
                                                   float dpi_scale)
    : java_ref_(env, obj), dpi_scale_(dpi_scale) {
  DCHECK(!java_ref_.is_empty());
  drawable_horizontal_padding_ratio_ =
      Java_PopupTouchHandleDrawable_getHandleHorizontalPaddingRatio(env, obj);
}

PopupTouchHandleDrawable::~PopupTouchHandleDrawable() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_PopupTouchHandleDrawable_destroy(env, obj.obj());
}

void PopupTouchHandleDrawable::SetEnabled(bool enabled) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  if (enabled)
    Java_PopupTouchHandleDrawable_show(env, obj.obj());
  else
    Java_PopupTouchHandleDrawable_hide(env, obj.obj());
}

void PopupTouchHandleDrawable::SetOrientation(
    ui::TouchHandleOrientation orientation,
    bool mirror_vertical,
    bool mirror_horizontal) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    Java_PopupTouchHandleDrawable_setOrientation(
        env, obj.obj(), static_cast<int>(orientation), mirror_vertical,
        mirror_horizontal);
  }
}

void PopupTouchHandleDrawable::SetOrigin(const gfx::PointF& origin) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    const gfx::PointF origin_pix = gfx::ScalePoint(origin, dpi_scale_);
    Java_PopupTouchHandleDrawable_setOrigin(env, obj.obj(), origin_pix.x(),
                                            origin_pix.y());
  }
}

void PopupTouchHandleDrawable::SetAlpha(float alpha) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  bool visible = alpha > 0;
  if (!obj.is_null())
    Java_PopupTouchHandleDrawable_setVisible(env, obj.obj(), visible);
}

gfx::RectF PopupTouchHandleDrawable::GetVisibleBounds() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return gfx::RectF();
  gfx::RectF unscaled_rect(
      Java_PopupTouchHandleDrawable_getPositionX(env, obj.obj()),
      Java_PopupTouchHandleDrawable_getPositionY(env, obj.obj()),
      Java_PopupTouchHandleDrawable_getVisibleWidth(env, obj.obj()),
      Java_PopupTouchHandleDrawable_getVisibleHeight(env, obj.obj()));
  return gfx::ScaleRect(unscaled_rect, 1.f / dpi_scale_);
}

float PopupTouchHandleDrawable::GetDrawableHorizontalPaddingRatio() const {
  return drawable_horizontal_padding_ratio_;
}

// static
bool PopupTouchHandleDrawable::RegisterPopupTouchHandleDrawable(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
