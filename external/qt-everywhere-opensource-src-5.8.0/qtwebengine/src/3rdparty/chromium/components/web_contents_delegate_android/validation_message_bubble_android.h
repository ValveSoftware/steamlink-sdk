// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_CONTENTS_DELEGATE_ANDROID_VALIDATION_MESSAGE_BUBBLE_ANDROID_H_
#define COMPONENTS_WEB_CONTENTS_DELEGATE_ANDROID_VALIDATION_MESSAGE_BUBBLE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/strings/string16.h"

namespace gfx {
class Rect;
}

namespace content {
class RenderWidgetHost;
}

namespace web_contents_delegate_android {

// An implementation of ValidationMessageBubble for Android. This class is a
// bridge to a Java implementation.
class ValidationMessageBubbleAndroid {
 public:
  ValidationMessageBubbleAndroid(content::RenderWidgetHost* widget_host,
                                 const gfx::Rect& anchor_in_screen,
                                 const base::string16& main_text,
                                 const base::string16& sub_text);
  virtual ~ValidationMessageBubbleAndroid();
  virtual void SetPositionRelativeToAnchor(
      content::RenderWidgetHost* widget_host,
      const gfx::Rect& anchor_in_screen);

  static bool Register(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_validation_message_bubble_;
};

}  // namespace web_contents_delegate_android

#endif  // COMPONENTS_WEB_CONTENTS_DELEGATE_ANDROID_VALIDATION_MESSAGE_BUBBLE_ANDROID_H_
