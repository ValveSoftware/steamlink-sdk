// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BODY_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BODY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if defined(OS_ANDROID)
#include <jni.h>
#include "base/android/scoped_java_ref.h"
#endif

namespace content {

// ResourceRequestBody represents body (i.e. upload data) of a HTTP request.
//
// This class is intentionally opaque:
// *) Embedders cannot inspect the payload of ResourceRequestBody.  Only the
//    //content layer can decompose ResourceRequestBody into references to file
//    ranges, byte vectors, blob uris, etc.
// *) Embedders can get instances of ResourceRequestBody only by
//    - receiving an instance created inside //content layer (e.g. receiving it
//      via content::OpenURLParams),
//    - calling CreateFromBytes with a vector of bytes (e.g. to support
//      Android's WebView::postUrl API, to support DoSearchByImageInNewTab and
//      to support test code).
// *) Embedders typically end up passing ResourceRequestBody back into the
//    //content layer via content::NavigationController::LoadUrlParams.
class CONTENT_EXPORT ResourceRequestBody
    : public base::RefCountedThreadSafe<ResourceRequestBody> {
 public:
  // Creates ResourceRequestBody that holds a copy of |bytes|.
  static scoped_refptr<ResourceRequestBody> CreateFromBytes(const char* bytes,
                                                            size_t length);

#if defined(OS_ANDROID)
  // Returns a org.chromium.content_public.common.ResourceRequestBody Java
  // object that wraps a ref-counted pointer to the |resource_request_body|.
  base::android::ScopedJavaLocalRef<jobject> ToJavaObject(JNIEnv* env);

  // Extracts and returns a C++ object out of Java's
  // org.chromium.content_public.common.ResourceRequestBody.
  static scoped_refptr<ResourceRequestBody> FromJavaObject(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_object);
#endif

 protected:
  ResourceRequestBody();
  virtual ~ResourceRequestBody();

 private:
  friend class base::RefCountedThreadSafe<ResourceRequestBody>;
  DISALLOW_COPY_AND_ASSIGN(ResourceRequestBody);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BODY_H_
