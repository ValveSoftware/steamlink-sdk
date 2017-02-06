// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_WINDOW_ANDROID_H_
#define UI_ANDROID_WINDOW_ANDROID_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {

class WindowAndroidCompositor;
class WindowAndroidObserver;

// Android implementation of the activity window.
class UI_ANDROID_EXPORT WindowAndroid {
 public:
  WindowAndroid(JNIEnv* env, jobject obj);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  static bool RegisterWindowAndroid(JNIEnv* env);

  // The content offset is used to translate snapshots to the correct part of
  // the window.
  void set_content_offset(const gfx::Vector2dF& content_offset) {
    content_offset_ = content_offset;
  }

  gfx::Vector2dF content_offset() const {
    return content_offset_;
  }

  // Compositor callback relay.
  void OnCompositingDidCommit();

  void AttachCompositor(WindowAndroidCompositor* compositor);
  void DetachCompositor();

  void AddObserver(WindowAndroidObserver* observer);
  void RemoveObserver(WindowAndroidObserver* observer);

  WindowAndroidCompositor* GetCompositor() { return compositor_; }

  void RequestVSyncUpdate();
  void SetNeedsAnimate();
  void Animate(base::TimeTicks begin_frame_time);
  void OnVSync(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jlong time_micros,
               jlong period_micros);
  void OnVisibilityChanged(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           bool visible);
  void OnActivityStopped(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);
  void OnActivityStarted(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  // Return whether the specified Android permission is granted.
  bool HasPermission(const std::string& permission);
  // Return whether the specified Android permission can be requested by Chrome.
  bool CanRequestPermission(const std::string& permission);

  static WindowAndroid* createForTesting();

 private:
  ~WindowAndroid();

  base::android::ScopedJavaGlobalRef<jobject> java_window_;
  gfx::Vector2dF content_offset_;
  WindowAndroidCompositor* compositor_;

  base::ObserverList<WindowAndroidObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(WindowAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_WINDOW_ANDROID_H_
