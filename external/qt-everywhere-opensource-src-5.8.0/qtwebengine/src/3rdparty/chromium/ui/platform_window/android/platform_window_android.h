// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_ANDROID_PLATFORM_WINDOW_ANDROID_H_
#define UI_PLATFORM_WINDOW_ANDROID_PLATFORM_WINDOW_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/sequential_id_generator.h"
#include "ui/platform_window/android/android_window_export.h"
#include "ui/platform_window/android/platform_ime_controller_android.h"
#include "ui/platform_window/platform_window.h"

struct ANativeWindow;

namespace ui {

class ANDROID_WINDOW_EXPORT PlatformWindowAndroid : public PlatformWindow {
 public:
  static bool Register(JNIEnv* env);

  explicit PlatformWindowAndroid(PlatformWindowDelegate* delegate);
  ~PlatformWindowAndroid() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SurfaceCreated(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      const base::android::JavaParamRef<jobject>& jsurface,
                      float device_pixel_ratio);
  void SurfaceDestroyed(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  void SurfaceSetSize(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jint width,
                      jint height,
                      jfloat density);
  bool TouchEvent(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  jlong time_ms,
                  jint masked_action,
                  jint pointer_id,
                  jfloat x,
                  jfloat y,
                  jfloat pressure,
                  jfloat touch_major,
                  jfloat touch_minor,
                  jfloat orientation,
                  jfloat h_wheel,
                  jfloat v_wheel);
  bool KeyEvent(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                bool pressed,
                jint key_code,
                jint unicode_character);

 private:
  void ReleaseWindow();

  // Overridden from PlatformWindow:
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetTitle(const base::string16& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  PlatformImeController* GetPlatformImeController() override;

  PlatformWindowDelegate* delegate_;

  JavaObjectWeakGlobalRef java_platform_window_android_;
  ANativeWindow* window_;
  ui::SequentialIDGenerator id_generator_;

  gfx::Size size_;  // Origin is always (0,0)

  PlatformImeControllerAndroid platform_ime_controller_;

  base::WeakPtrFactory<PlatformWindowAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlatformWindowAndroid);
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_ANDROID_PLATFORM_WINDOW_ANDROID_H_
