// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/java_method.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "content/browser/android/java/jni_helper.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::GetClass;
using base::android::MethodID;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace content {
namespace {

const char kGetName[] = "getName";
const char kGetDeclaringClass[] = "getDeclaringClass";
const char kGetModifiers[] = "getModifiers";
const char kGetParameterTypes[] = "getParameterTypes";
const char kGetReturnType[] = "getReturnType";
const char kIntegerReturningBoolean[] = "(I)Z";
const char kIsStatic[] = "isStatic";
const char kJavaLangClass[] = "java/lang/Class";
const char kJavaLangReflectMethod[] = "java/lang/reflect/Method";
const char kJavaLangReflectModifier[] = "java/lang/reflect/Modifier";
const char kReturningInteger[] = "()I";
const char kReturningJavaLangClass[] = "()Ljava/lang/Class;";
const char kReturningJavaLangClassArray[] = "()[Ljava/lang/Class;";
const char kReturningJavaLangString[] = "()Ljava/lang/String;";

struct ModifierClassTraits :
      public base::internal::LeakyLazyInstanceTraits<ScopedJavaGlobalRef<
                                                         jclass> > {
  static ScopedJavaGlobalRef<jclass>* New(void* instance) {
    JNIEnv* env = AttachCurrentThread();
    // Use placement new to initialize our instance in our preallocated space.
    return new (instance) ScopedJavaGlobalRef<jclass>(
        GetClass(env, kJavaLangReflectModifier));
  }
};

base::LazyInstance<ScopedJavaGlobalRef<jclass>, ModifierClassTraits>
    g_java_lang_reflect_modifier_class = LAZY_INSTANCE_INITIALIZER;

std::string BinaryNameToJNISignature(const std::string& binary_name,
                                     JavaType* type) {
  DCHECK(type);
  *type = JavaType::CreateFromBinaryName(binary_name);
  return type->JNISignature();
}

}  // namespace

JavaMethod::JavaMethod(const base::android::JavaRef<jobject>& method)
    : java_method_(method),
      have_calculated_num_parameters_(false),
      id_(NULL) {
  JNIEnv* env = AttachCurrentThread();
  // On construction, we do nothing except get the name. Everything else is
  // done lazily.
  ScopedJavaLocalRef<jstring> name(env, static_cast<jstring>(
      env->CallObjectMethod(java_method_.obj(), GetMethodIDFromClassName(
          env,
          kJavaLangReflectMethod,
          kGetName,
          kReturningJavaLangString))));
  name_ = ConvertJavaStringToUTF8(name);
}

JavaMethod::~JavaMethod() {
}

size_t JavaMethod::num_parameters() const {
  EnsureNumParametersIsSetUp();
  return num_parameters_;
}

bool JavaMethod::is_static() const {
  EnsureTypesAndIDAreSetUp();
  return is_static_;
}

const JavaType& JavaMethod::parameter_type(size_t index) const {
  EnsureTypesAndIDAreSetUp();
  return parameter_types_[index];
}

const JavaType& JavaMethod::return_type() const {
  EnsureTypesAndIDAreSetUp();
  return return_type_;
}

jmethodID JavaMethod::id() const {
  EnsureTypesAndIDAreSetUp();
  return id_;
}

void JavaMethod::EnsureNumParametersIsSetUp() const {
  if (have_calculated_num_parameters_) {
    return;
  }
  have_calculated_num_parameters_ = true;

  // The number of parameters will be used frequently when determining
  // whether to call this method. We don't get the ID etc until actually
  // required.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jarray> parameters(env, static_cast<jarray>(
      env->CallObjectMethod(java_method_.obj(), GetMethodIDFromClassName(
          env,
          kJavaLangReflectMethod,
          kGetParameterTypes,
          kReturningJavaLangClassArray))));
  num_parameters_ = env->GetArrayLength(parameters.obj());
}

void JavaMethod::EnsureTypesAndIDAreSetUp() const {
  if (id_) {
    return;
  }

  // Get the parameters
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> parameters(env, static_cast<jobjectArray>(
      env->CallObjectMethod(java_method_.obj(), GetMethodIDFromClassName(
          env,
          kJavaLangReflectMethod,
          kGetParameterTypes,
          kReturningJavaLangClassArray))));
  // Usually, this will already have been called.
  EnsureNumParametersIsSetUp();
  DCHECK_EQ(num_parameters_,
            static_cast<size_t>(env->GetArrayLength(parameters.obj())));

  // Java gives us the argument type using an extended version of the 'binary
  // name'. See
  // http://download.oracle.com/javase/1.4.2/docs/api/java/lang/Class.html#getName().
  // If we build the signature now, there's no need to store the binary name
  // of the arguments. We just store the simple type.
  std::string signature("(");

  // Form the signature and record the parameter types.
  parameter_types_.resize(num_parameters_);
  for (size_t i = 0; i < num_parameters_; ++i) {
    ScopedJavaLocalRef<jclass> parameter(
        env,
        static_cast<jclass>(env->GetObjectArrayElement(parameters.obj(), i)));
    ScopedJavaLocalRef<jstring> name(env, static_cast<jstring>(
        env->CallObjectMethod(parameter.obj(), GetMethodIDFromClassName(
            env,
            kJavaLangClass,
            kGetName,
            kReturningJavaLangString))));
    std::string name_utf8 = ConvertJavaStringToUTF8(name);
    signature += BinaryNameToJNISignature(name_utf8, &parameter_types_[i]);
    if (parameter_types_[i].type == JavaType::TypeObject) {
      parameter_types_[i].class_ref.Reset(parameter);
    }
  }
  signature += ")";

  // Get the return type
  ScopedJavaLocalRef<jclass> clazz(env, static_cast<jclass>(
      env->CallObjectMethod(java_method_.obj(), GetMethodIDFromClassName(
          env,
          kJavaLangReflectMethod,
          kGetReturnType,
          kReturningJavaLangClass))));
  ScopedJavaLocalRef<jstring> name(env, static_cast<jstring>(
      env->CallObjectMethod(clazz.obj(), GetMethodIDFromClassName(
          env,
          kJavaLangClass,
          kGetName,
          kReturningJavaLangString))));
  signature += BinaryNameToJNISignature(ConvertJavaStringToUTF8(name),
                                        &return_type_);

  // Determine whether the method is static.
  jint modifiers = env->CallIntMethod(
      java_method_.obj(), GetMethodIDFromClassName(env,
                                                   kJavaLangReflectMethod,
                                                   kGetModifiers,
                                                   kReturningInteger));
  is_static_ = env->CallStaticBooleanMethod(
      g_java_lang_reflect_modifier_class.Get().obj(),
      MethodID::Get<MethodID::TYPE_STATIC>(
          env, g_java_lang_reflect_modifier_class.Get().obj(), kIsStatic,
          kIntegerReturningBoolean),
      modifiers);

  // Get the ID for this method.
  ScopedJavaLocalRef<jclass> declaring_class(env, static_cast<jclass>(
      env->CallObjectMethod(java_method_.obj(), GetMethodIDFromClassName(
          env,
          kJavaLangReflectMethod,
          kGetDeclaringClass,
          kReturningJavaLangClass))));
  id_ = is_static_ ?
      MethodID::Get<MethodID::TYPE_STATIC>(
          env, declaring_class.obj(), name_.c_str(), signature.c_str()) :
      MethodID::Get<MethodID::TYPE_INSTANCE>(
          env, declaring_class.obj(), name_.c_str(), signature.c_str());
  java_method_.Reset();
}

}  // namespace content
