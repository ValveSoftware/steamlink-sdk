// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_GIN_JAVA_BOUND_OBJECT_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_GIN_JAVA_BOUND_OBJECT_H_

#include <map>
#include <set>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/id_map.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/browser/renderer_host/java/java_method.h"

namespace content {

class RenderFrameHost;

class GinJavaBoundObject
    : public base::RefCountedThreadSafe<GinJavaBoundObject> {
 public:
  // Using scoped_refptr<> as a value type is somewhat not IDMap had been
  // designed for (it returns T* from lookups, so we have to de-ref it), but on
  // the other hand, this allows us to manage lifetime of GinJavaBoundObject
  // automatically.
  typedef IDMap<scoped_refptr<GinJavaBoundObject>, IDMapOwnPointer> ObjectMap;
  typedef ObjectMap::KeyType ObjectID;

  static GinJavaBoundObject* CreateNamed(
      const JavaObjectWeakGlobalRef& ref,
      const base::android::JavaRef<jclass>& safe_annotation_clazz);
  static GinJavaBoundObject* CreateTransient(
      const JavaObjectWeakGlobalRef& ref,
      const base::android::JavaRef<jclass>& safe_annotation_clazz,
      RenderFrameHost* holder);

  JavaObjectWeakGlobalRef& GetWeakRef() { return ref_; }
  base::android::ScopedJavaLocalRef<jobject> GetLocalRef(JNIEnv* env) {
    return ref_.get(env);
  }

  bool IsNamed() { return names_count_ > 0; }
  void AddName() { ++names_count_; }
  void RemoveName() { --names_count_; }

  bool HasHolders() { return !holders_.empty(); }
  void AddHolder(RenderFrameHost* holder) { holders_.insert(holder); }
  void RemoveHolder(RenderFrameHost* holder) { holders_.erase(holder); }

  // The following methods are called on the background thread.
  std::set<std::string> GetMethodNames();
  bool HasMethod(const std::string& method_name);
  const JavaMethod* FindMethod(const std::string& method_name,
                               size_t num_parameters);
  bool IsObjectGetClassMethod(const JavaMethod* method);
  const base::android::JavaRef<jclass>& GetSafeAnnotationClass();
  base::android::ScopedJavaLocalRef<jclass> GetLocalClassRef(JNIEnv* env);

 private:
  friend class base::RefCountedThreadSafe<GinJavaBoundObject>;

  GinJavaBoundObject(
      const JavaObjectWeakGlobalRef& ref,
      const base::android::JavaRef<jclass>& safe_annotation_clazz);
  GinJavaBoundObject(
      const JavaObjectWeakGlobalRef& ref,
      const base::android::JavaRef<jclass>& safe_annotation_clazz,
      const std::set<RenderFrameHost*> holders);
  ~GinJavaBoundObject();

  // The following methods are called on the background thread.
  void EnsureMethodsAreSetUp();

  JavaObjectWeakGlobalRef ref_;

  // An object must be kept in retained_object_set_ either if it has
  // names or if it has a non-empty holders set.
  int names_count_;
  std::set<RenderFrameHost*> holders_;

  // The following fields are accessed on the background thread.
  typedef std::multimap<std::string, linked_ptr<JavaMethod> > JavaMethodMap;
  JavaMethodMap methods_;
  jmethodID object_get_class_method_id_;
  bool are_methods_set_up_;
  base::android::ScopedJavaGlobalRef<jclass> safe_annotation_clazz_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_GIN_JAVA_BOUND_OBJECT_H_
