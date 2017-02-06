// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_VIEW_ANDROID_H_
#define UI_ANDROID_VIEW_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "ui/android/ui_android_export.h"

namespace ui {

class WindowAndroid;

// This is a NativeView interface for getting access to
// WindowAndroid(NativeWindow).
class UI_ANDROID_EXPORT ViewAndroid {
 public:
  virtual WindowAndroid* GetWindowAndroid() const = 0;

  virtual base::android::ScopedJavaLocalRef<jobject>
      GetViewAndroidDelegate() const = 0 ;

 protected:
  virtual ~ViewAndroid() {}
};

}  // namespace ui

#endif  // UI_ANDROID_VIEW_ANDROID_H_
