// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/resource_request_body.h"

#include "content/common/resource_request_body_impl.h"

#if defined(OS_ANDROID)
#include "content/common/android/resource_request_body_android.h"
#endif

namespace content {

ResourceRequestBody::ResourceRequestBody() {}

ResourceRequestBody::~ResourceRequestBody() {}

// static
scoped_refptr<ResourceRequestBody> ResourceRequestBody::CreateFromBytes(
    const char* bytes,
    size_t length) {
  scoped_refptr<ResourceRequestBodyImpl> result = new ResourceRequestBodyImpl();
  result->AppendBytes(bytes, length);
  return result;
}

#if defined(OS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> ResourceRequestBody::ToJavaObject(
    JNIEnv* env) {
  return ConvertResourceRequestBodyToJavaObject(
      env, static_cast<ResourceRequestBodyImpl*>(this));
}

// static
scoped_refptr<ResourceRequestBody> ResourceRequestBody::FromJavaObject(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_object) {
  return ExtractResourceRequestBodyFromJavaObject(env, java_object);
}
#endif

}  // namespace content
