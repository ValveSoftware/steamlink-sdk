// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABLE_PAGE_UTILS_ANDROID_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABLE_PAGE_UTILS_ANDROID_H_

#include <jni.h>

namespace dom_distiller {
namespace android {
bool RegisterDistillablePageUtils(JNIEnv* env);
}
}

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABLE_PAGE_UTILS_ANDROID_H_
