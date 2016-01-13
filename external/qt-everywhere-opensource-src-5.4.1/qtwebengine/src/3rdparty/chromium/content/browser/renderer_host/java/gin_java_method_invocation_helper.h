// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_GIN_JAVA_METHOD_INVOCATION_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_GIN_JAVA_METHOD_INVOCATION_HELPER_H_

#include <map>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "content/browser/renderer_host/java/gin_java_bound_object.h"
#include "content/browser/renderer_host/java/java_type.h"
#include "content/common/content_export.h"

namespace content {

class JavaMethod;

// Instances of this class are created and initialized on the UI thread, do
// their work on the background thread, and then again examined on the UI
// thread. They don't work on both threads simultaneously, though.
class CONTENT_EXPORT GinJavaMethodInvocationHelper
    : public base::RefCountedThreadSafe<GinJavaMethodInvocationHelper> {
 public:
  // DispatcherDelegate is used on the UI thread
  class DispatcherDelegate {
   public:
    DispatcherDelegate() {}
    virtual ~DispatcherDelegate() {}
    virtual JavaObjectWeakGlobalRef GetObjectWeakRef(
        GinJavaBoundObject::ObjectID object_id) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(DispatcherDelegate);
  };

  // ObjectDelegate is used in the background thread
  class ObjectDelegate {
   public:
    ObjectDelegate() {}
    virtual ~ObjectDelegate() {}
    virtual base::android::ScopedJavaLocalRef<jobject> GetLocalRef(
        JNIEnv* env) = 0;
    virtual base::android::ScopedJavaLocalRef<jclass> GetLocalClassRef(
        JNIEnv* env) = 0;
    virtual const JavaMethod* FindMethod(const std::string& method_name,
                                         size_t num_parameters) = 0;
    virtual bool IsObjectGetClassMethod(const JavaMethod* method) = 0;
    virtual const base::android::JavaRef<jclass>& GetSafeAnnotationClass() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ObjectDelegate);
  };

  GinJavaMethodInvocationHelper(scoped_ptr<ObjectDelegate> object,
                                const std::string& method_name,
                                const base::ListValue& arguments);
  void Init(DispatcherDelegate* dispatcher);

  // Called on the background thread
  void Invoke();

  // Called on the UI thread
  bool HoldsPrimitiveResult();
  const base::ListValue& GetPrimitiveResult();
  const base::android::JavaRef<jobject>& GetObjectResult();
  const base::android::JavaRef<jclass>& GetSafeAnnotationClass();
  const std::string& GetErrorMessage();

 private:
  friend class base::RefCountedThreadSafe<GinJavaMethodInvocationHelper>;
  ~GinJavaMethodInvocationHelper();

  // Called on the UI thread
  void BuildObjectRefsFromListValue(DispatcherDelegate* dispatcher,
                                    const base::Value* list_value);
  void BuildObjectRefsFromDictionaryValue(DispatcherDelegate* dispatcher,
                                          const base::Value* dict_value);

  bool AppendObjectRef(DispatcherDelegate* dispatcher,
                       const base::Value* raw_value);

  // Called on the background thread.
  void InvokeMethod(jobject object,
                    jclass clazz,
                    const JavaType& return_type,
                    jmethodID id,
                    jvalue* parameters);
  void SetInvocationFailure(const char* error_message);
  void SetPrimitiveResult(const base::ListValue& result_wrapper);
  void SetObjectResult(
      const base::android::JavaRef<jobject>& object,
      const base::android::JavaRef<jclass>& safe_annotation_clazz);

  typedef std::map<GinJavaBoundObject::ObjectID,
                   JavaObjectWeakGlobalRef> ObjectRefs;

  scoped_ptr<ObjectDelegate> object_;
  const std::string method_name_;
  scoped_ptr<base::ListValue> arguments_;
  ObjectRefs object_refs_;
  bool holds_primitive_result_;
  scoped_ptr<base::ListValue> primitive_result_;
  std::string error_message_;
  base::android::ScopedJavaGlobalRef<jobject> object_result_;
  base::android::ScopedJavaGlobalRef<jclass> safe_annotation_clazz_;

  DISALLOW_COPY_AND_ASSIGN(GinJavaMethodInvocationHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_GIN_JAVA_METHOD_INVOCATION_HELPER_H_
