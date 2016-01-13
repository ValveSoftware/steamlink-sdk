// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BOUND_OBJECT_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BOUND_OBJECT_H_

#include <jni.h>
#include <map>
#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/java/java_method.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace content {

class JavaBridgeDispatcherHostManager;

// Wrapper around a Java object.
//
// Represents a Java object for use in the Java bridge. Holds a global ref to
// the Java object and provides the ability to invoke methods on it.
// Interrogation of the Java object for its methods is done lazily. This class
// is not generally threadsafe. However, it does allow for instances to be
// created and destroyed on different threads.
class JavaBoundObject {
 public:
  // Takes a Java object and creates a JavaBoundObject around it. If
  // |safe_annotation_clazz| annotation is non-null, the method is exposed
  // to JavaScript. Returns an NPObject with a ref count of one which owns the
  // JavaBoundObject.
  // See also comment below for |manager_|.
  static NPObject* Create(
      const base::android::JavaRef<jobject>& object,
      const base::android::JavaRef<jclass>& safe_annotation_clazz,
      const base::WeakPtr<JavaBridgeDispatcherHostManager>& manager,
      bool can_enumerate_methods);

  virtual ~JavaBoundObject();

  // Gets a local ref to the underlying JavaObject from a JavaBoundObject
  // wrapped as an NPObject. May return null if the underlying object has
  // been garbage collected.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      NPObject* object);

  // Methods to implement the NPObject callbacks.
  bool CanEnumerateMethods() const { return can_enumerate_methods_; }
  std::vector<std::string> GetMethodNames() const;
  bool HasMethod(const std::string& name) const;
  bool Invoke(const std::string& name, const NPVariant* args, size_t arg_count,
              NPVariant* result);

 private:
  JavaBoundObject(
      const base::android::JavaRef<jobject>& object,
      const base::android::JavaRef<jclass>& safe_annotation_clazz,
      const base::WeakPtr<JavaBridgeDispatcherHostManager>& manager,
      bool can_enumerate_methods);

  void EnsureMethodsAreSetUp() const;
  base::android::ScopedJavaLocalRef<jclass> GetLocalClassRef(JNIEnv* env) const;

  static void ThrowSecurityException(const char* message);

  // The weak ref to the underlying Java object that this JavaBoundObject
  // instance represents.
  JavaObjectWeakGlobalRef java_object_;

  // Keep a pointer back to the JavaBridgeDispatcherHostManager so that we
  // can notify it when this JavaBoundObject is destroyed. JavaBoundObjects
  // may outlive the manager so keep a WeakPtr. Note the WeakPtr may only be
  // dereferenced on the UI thread.
  base::WeakPtr<JavaBridgeDispatcherHostManager> manager_;

  // Map of public methods, from method name to Method instance. Multiple
  // entries will be present for overloaded methods. Note that we can't use
  // scoped_ptr in STL containers as we can't copy it.
  typedef std::multimap<std::string, linked_ptr<JavaMethod> > JavaMethodMap;
  mutable JavaMethodMap methods_;
  mutable bool are_methods_set_up_;
  mutable jmethodID object_get_class_method_id_;
  const bool can_enumerate_methods_;

  base::android::ScopedJavaGlobalRef<jclass> safe_annotation_clazz_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(JavaBoundObject);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BOUND_OBJECT_H_
