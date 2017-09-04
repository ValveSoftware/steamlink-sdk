// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/browser/readback_types.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class WindowAndroid;
}

namespace content {

class WebContents;

// DEPRECATED. Do not add methods.
// Refer to the public WebContents interface or elsewhere instead.
class CONTENT_EXPORT ContentViewCore {
 public:
  // Returns the existing ContentViewCore for |web_contents|, or nullptr.
  static ContentViewCore* FromWebContents(WebContents* web_contents);

  virtual WebContents* GetWebContents() const = 0;

  // May return null reference.
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;
  virtual void ShowPastePopup(int x, int y) = 0;

  virtual ui::WindowAndroid* GetWindowAndroid() const = 0;

 protected:
 ~ContentViewCore() {}
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_
