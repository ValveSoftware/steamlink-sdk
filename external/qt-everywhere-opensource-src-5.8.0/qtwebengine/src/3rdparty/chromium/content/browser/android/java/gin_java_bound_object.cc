// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/gin_java_bound_object.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/android/java/jni_helper.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

const char kJavaLangClass[] = "java/lang/Class";
const char kJavaLangObject[] = "java/lang/Object";
const char kJavaLangReflectMethod[] = "java/lang/reflect/Method";
const char kGetClass[] = "getClass";
const char kGetMethods[] = "getMethods";
const char kIsAnnotationPresent[] = "isAnnotationPresent";
const char kReturningJavaLangClass[] = "()Ljava/lang/Class;";
const char kReturningJavaLangReflectMethodArray[] =
    "()[Ljava/lang/reflect/Method;";
const char kTakesJavaLangClassReturningBoolean[] = "(Ljava/lang/Class;)Z";

}  // namespace


// static
GinJavaBoundObject* GinJavaBoundObject::CreateNamed(
    const JavaObjectWeakGlobalRef& ref,
    const base::android::JavaRef<jclass>& safe_annotation_clazz) {
  return new GinJavaBoundObject(ref, safe_annotation_clazz);
}

// static
GinJavaBoundObject* GinJavaBoundObject::CreateTransient(
    const JavaObjectWeakGlobalRef& ref,
    const base::android::JavaRef<jclass>& safe_annotation_clazz,
    int32_t holder) {
  std::set<int32_t> holders;
  holders.insert(holder);
  return new GinJavaBoundObject(ref, safe_annotation_clazz, holders);
}

GinJavaBoundObject::GinJavaBoundObject(
    const JavaObjectWeakGlobalRef& ref,
    const base::android::JavaRef<jclass>& safe_annotation_clazz)
    : ref_(ref),
      names_count_(1),
      object_get_class_method_id_(NULL),
      are_methods_set_up_(false),
      safe_annotation_clazz_(safe_annotation_clazz) {
}

GinJavaBoundObject::GinJavaBoundObject(
    const JavaObjectWeakGlobalRef& ref,
    const base::android::JavaRef<jclass>& safe_annotation_clazz,
    const std::set<int32_t>& holders)
    : ref_(ref),
      names_count_(0),
      holders_(holders),
      object_get_class_method_id_(NULL),
      are_methods_set_up_(false),
      safe_annotation_clazz_(safe_annotation_clazz) {}

GinJavaBoundObject::~GinJavaBoundObject() {
}

std::set<std::string> GinJavaBoundObject::GetMethodNames() {
  EnsureMethodsAreSetUp();
  std::set<std::string> result;
  for (JavaMethodMap::const_iterator it = methods_.begin();
       it != methods_.end();
       ++it) {
    result.insert(it->first);
  }
  return result;
}

bool GinJavaBoundObject::HasMethod(const std::string& method_name) {
  EnsureMethodsAreSetUp();
  return methods_.find(method_name) != methods_.end();
}

const JavaMethod* GinJavaBoundObject::FindMethod(
    const std::string& method_name,
    size_t num_parameters) {
  EnsureMethodsAreSetUp();

  // Get all methods with the correct name.
  std::pair<JavaMethodMap::const_iterator, JavaMethodMap::const_iterator>
      iters = methods_.equal_range(method_name);
  if (iters.first == iters.second) {
    return NULL;
  }

  // LIVECONNECT_COMPLIANCE: We just take the first method with the correct
  // number of arguments, while the spec proposes using cost-based algorithm:
  // https://jdk6.java.net/plugin2/liveconnect/#OVERLOADED_METHODS
  for (JavaMethodMap::const_iterator iter = iters.first; iter != iters.second;
       ++iter) {
    if (iter->second->num_parameters() == num_parameters) {
      return iter->second.get();
    }
  }

  return NULL;
}

bool GinJavaBoundObject::IsObjectGetClassMethod(const JavaMethod* method) {
  EnsureMethodsAreSetUp();
  // As java.lang.Object.getClass is declared to be final, it is sufficient to
  // compare methodIDs.
  return method->id() == object_get_class_method_id_;
}

const base::android::JavaRef<jclass>&
GinJavaBoundObject::GetSafeAnnotationClass() {
  return safe_annotation_clazz_;
}

base::android::ScopedJavaLocalRef<jclass> GinJavaBoundObject::GetLocalClassRef(
    JNIEnv* env) {
  if (!object_get_class_method_id_) {
    object_get_class_method_id_ = GetMethodIDFromClassName(
        env, kJavaLangObject, kGetClass, kReturningJavaLangClass);
  }
  ScopedJavaLocalRef<jobject> obj = GetLocalRef(env);
  if (obj.obj()) {
    return base::android::ScopedJavaLocalRef<jclass>(
        env,
        static_cast<jclass>(
            env->CallObjectMethod(obj.obj(), object_get_class_method_id_)));
  } else {
    return base::android::ScopedJavaLocalRef<jclass>();
  }
}

void GinJavaBoundObject::EnsureMethodsAreSetUp() {
  if (are_methods_set_up_)
    return;
  are_methods_set_up_ = true;

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jclass> clazz = GetLocalClassRef(env);
  if (clazz.is_null()) {
    return;
  }

  ScopedJavaLocalRef<jobjectArray> methods(env, static_cast<jobjectArray>(
      env->CallObjectMethod(clazz.obj(), GetMethodIDFromClassName(
          env,
          kJavaLangClass,
          kGetMethods,
          kReturningJavaLangReflectMethodArray))));

  size_t num_methods = env->GetArrayLength(methods.obj());
  // Java objects always have public methods.
  DCHECK(num_methods);

  for (size_t i = 0; i < num_methods; ++i) {
    ScopedJavaLocalRef<jobject> java_method(
        env,
        env->GetObjectArrayElement(methods.obj(), i));

    if (!safe_annotation_clazz_.is_null()) {
      jboolean safe = env->CallBooleanMethod(java_method.obj(),
          GetMethodIDFromClassName(
              env,
              kJavaLangReflectMethod,
              kIsAnnotationPresent,
              kTakesJavaLangClassReturningBoolean),
          safe_annotation_clazz_.obj());

      if (!safe)
        continue;
    }

    JavaMethod* method = new JavaMethod(java_method);
    methods_.insert(std::make_pair(method->name(), make_linked_ptr(method)));
  }
}

}  // namespace content
