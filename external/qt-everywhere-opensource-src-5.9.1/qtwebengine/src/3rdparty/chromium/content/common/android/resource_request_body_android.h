// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_RESOURCE_REQUEST_BODY_JNI_H_
#define CONTENT_BROWSER_ANDROID_RESOURCE_REQUEST_BODY_JNI_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"

namespace content {

class ResourceRequestBodyImpl;

bool RegisterResourceRequestBody(JNIEnv* env);

// Returns an instance of org.chromium.content_public.common.ResourceRequestBody
// that contains serialized representation of the |native_object|.
base::android::ScopedJavaLocalRef<jobject>
ConvertResourceRequestBodyToJavaObject(
    JNIEnv* env,
    const scoped_refptr<ResourceRequestBodyImpl>& native_object);

// Reconstructs the native C++ content::ResourceRequestBody object based on
// org.chromium.content_public.common.ResourceRequestBody (|java_object|) passed
// in as an argument.
scoped_refptr<ResourceRequestBodyImpl> ExtractResourceRequestBodyFromJavaObject(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_object);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_RESOURCE_REQUEST_BODY_JNI_H_
