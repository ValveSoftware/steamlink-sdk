// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_GIN_JAVA_BOUND_OBJECT_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_GIN_JAVA_BOUND_OBJECT_DELEGATE_H_

#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/java/gin_java_bound_object.h"
#include "content/browser/renderer_host/java/gin_java_method_invocation_helper.h"

namespace content {

class GinJavaBoundObjectDelegate
    : public GinJavaMethodInvocationHelper::ObjectDelegate {
 public:
  GinJavaBoundObjectDelegate(scoped_refptr<GinJavaBoundObject> object);
  virtual ~GinJavaBoundObjectDelegate();

  virtual base::android::ScopedJavaLocalRef<jobject> GetLocalRef(
      JNIEnv* env) OVERRIDE;
  virtual base::android::ScopedJavaLocalRef<jclass> GetLocalClassRef(
      JNIEnv* env) OVERRIDE;
  virtual const JavaMethod* FindMethod(const std::string& method_name,
                                       size_t num_parameters) OVERRIDE;
  virtual bool IsObjectGetClassMethod(const JavaMethod* method) OVERRIDE;
  virtual const base::android::JavaRef<jclass>& GetSafeAnnotationClass()
      OVERRIDE;

 private:
  scoped_refptr<GinJavaBoundObject> object_;

  DISALLOW_COPY_AND_ASSIGN(GinJavaBoundObjectDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_GIN_JAVA_BOUND_OBJECT_DELEGATE_H_
