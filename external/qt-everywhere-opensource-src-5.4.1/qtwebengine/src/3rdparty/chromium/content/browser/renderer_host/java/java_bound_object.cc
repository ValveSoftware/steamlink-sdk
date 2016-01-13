// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/java/java_bound_object.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/singleton.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/browser/renderer_host/java/java_bridge_dispatcher_host_manager.h"
#include "content/browser/renderer_host/java/java_type.h"
#include "content/browser/renderer_host/java/jni_helper.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/public/web/WebBindings.h"

using base::StringPrintf;
using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using blink::WebBindings;

// The conversion between JavaScript and Java types is based on the Live
// Connect 2 spec. See
// http://jdk6.java.net/plugin2/liveconnect/#JS_JAVA_CONVERSIONS.

// Note that in some cases, we differ from from the spec in order to maintain
// existing behavior. These areas are marked LIVECONNECT_COMPLIANCE. We may
// revisit this decision in the future.

namespace content {
namespace {

const char kJavaLangClass[] = "java/lang/Class";
const char kJavaLangObject[] = "java/lang/Object";
const char kJavaLangReflectMethod[] = "java/lang/reflect/Method";
const char kJavaLangSecurityExceptionClass[] = "java/lang/SecurityException";
const char kGetClass[] = "getClass";
const char kGetMethods[] = "getMethods";
const char kIsAnnotationPresent[] = "isAnnotationPresent";
const char kReturningJavaLangClass[] = "()Ljava/lang/Class;";
const char kReturningJavaLangReflectMethodArray[] =
    "()[Ljava/lang/reflect/Method;";
const char kTakesJavaLangClassReturningBoolean[] = "(Ljava/lang/Class;)Z";
// This is an exception message, so no need to localize.
const char kAccessToObjectGetClassIsBlocked[] =
    "Access to java.lang.Object.getClass is blocked";

// Our special NPObject type.  We extend an NPObject with a pointer to a
// JavaBoundObject.  We also add static methods for each of the NPObject
// callbacks, which are registered by our NPClass. These methods simply
// delegate to the private implementation methods of JavaBoundObject.
struct JavaNPObject : public NPObject {
  JavaBoundObject* bound_object;

  static const NPClass kNPClass;

  static NPObject* Allocate(NPP npp, NPClass* np_class);
  static void Deallocate(NPObject* np_object);
  static bool HasMethod(NPObject* np_object, NPIdentifier np_identifier);
  static bool Invoke(NPObject* np_object, NPIdentifier np_identifier,
                     const NPVariant *args, uint32_t arg_count,
                     NPVariant *result);
  static bool HasProperty(NPObject* np_object, NPIdentifier np_identifier);
  static bool GetProperty(NPObject* np_object, NPIdentifier np_identifier,
                          NPVariant *result);
  static bool Enumerate(NPObject* object, NPIdentifier** values,
                        uint32_t* count);
};

const NPClass JavaNPObject::kNPClass = {
  NP_CLASS_STRUCT_VERSION,
  JavaNPObject::Allocate,
  JavaNPObject::Deallocate,
  NULL,  // NPInvalidate
  JavaNPObject::HasMethod,
  JavaNPObject::Invoke,
  NULL,  // NPInvokeDefault
  JavaNPObject::HasProperty,
  JavaNPObject::GetProperty,
  NULL,  // NPSetProperty,
  NULL,  // NPRemoveProperty
  JavaNPObject::Enumerate,
  NULL,
};

NPObject* JavaNPObject::Allocate(NPP npp, NPClass* np_class) {
  JavaNPObject* obj = new JavaNPObject();
  return obj;
}

void JavaNPObject::Deallocate(NPObject* np_object) {
  JavaNPObject* obj = reinterpret_cast<JavaNPObject*>(np_object);
  delete obj->bound_object;
  delete obj;
}

bool JavaNPObject::HasMethod(NPObject* np_object, NPIdentifier np_identifier) {
  std::string name(WebBindings::utf8FromIdentifier(np_identifier));
  JavaNPObject* obj = reinterpret_cast<JavaNPObject*>(np_object);
  return obj->bound_object->HasMethod(name);
}

bool JavaNPObject::Invoke(NPObject* np_object, NPIdentifier np_identifier,
                          const NPVariant* args, uint32_t arg_count,
                          NPVariant* result) {
  std::string name(WebBindings::utf8FromIdentifier(np_identifier));
  JavaNPObject* obj = reinterpret_cast<JavaNPObject*>(np_object);
  return obj->bound_object->Invoke(name, args, arg_count, result);
}

bool JavaNPObject::HasProperty(NPObject* np_object,
                               NPIdentifier np_identifier) {
  // LIVECONNECT_COMPLIANCE: Existing behavior is to return false to indicate
  // that the property is not present. Spec requires supporting this correctly.
  return false;
}

bool JavaNPObject::GetProperty(NPObject* np_object,
                               NPIdentifier np_identifier,
                               NPVariant* result) {
  // LIVECONNECT_COMPLIANCE: Existing behavior is to return false to indicate
  // that the property is undefined. Spec requires supporting this correctly.
  return false;
}

bool JavaNPObject::Enumerate(NPObject* np_object, NPIdentifier** values,
                             uint32_t* count) {
  JavaNPObject* obj = reinterpret_cast<JavaNPObject*>(np_object);
  if (!obj->bound_object->CanEnumerateMethods()) return false;
  std::vector<std::string> method_names = obj->bound_object->GetMethodNames();
  *count = base::saturated_cast<uint32_t>(method_names.size());
  *values = static_cast<NPIdentifier*>(calloc(*count, sizeof(NPIdentifier)));
  for (uint32_t i = 0; i < *count; ++i) {
    (*values)[i] = WebBindings::getStringIdentifier(method_names[i].c_str());
  }
  return true;
}

// Calls a Java method through JNI. If the Java method raises an uncaught
// exception, it is cleared and this method returns false. Otherwise, this
// method returns true and the Java method's return value is provided as an
// NPVariant. Note that this method does not do any type coercion. The Java
// return value is simply converted to the corresponding NPAPI type.
bool CallJNIMethod(
    jobject object,
    jclass clazz,
    const JavaType& return_type,
    jmethodID id,
    jvalue* parameters,
    NPVariant* result,
    const JavaRef<jclass>& safe_annotation_clazz,
    const base::WeakPtr<JavaBridgeDispatcherHostManager>& manager,
    bool can_enumerate_methods) {
  DCHECK(object || clazz);
  JNIEnv* env = AttachCurrentThread();
  switch (return_type.type) {
    case JavaType::TypeBoolean:
      BOOLEAN_TO_NPVARIANT(
          object ? env->CallBooleanMethodA(object, id, parameters)
                 : env->CallStaticBooleanMethodA(clazz, id, parameters),
          *result);
      break;
    case JavaType::TypeByte:
      INT32_TO_NPVARIANT(
          object ? env->CallByteMethodA(object, id, parameters)
                 : env->CallStaticByteMethodA(clazz, id, parameters),
          *result);
      break;
    case JavaType::TypeChar:
      INT32_TO_NPVARIANT(
          object ? env->CallCharMethodA(object, id, parameters)
                 : env->CallStaticCharMethodA(clazz, id, parameters),
          *result);
      break;
    case JavaType::TypeShort:
      INT32_TO_NPVARIANT(
          object ? env->CallShortMethodA(object, id, parameters)
                 : env->CallStaticShortMethodA(clazz, id, parameters),
          *result);
      break;
    case JavaType::TypeInt:
      INT32_TO_NPVARIANT(object
                             ? env->CallIntMethodA(object, id, parameters)
                             : env->CallStaticIntMethodA(clazz, id, parameters),
                         *result);
      break;
    case JavaType::TypeLong:
      DOUBLE_TO_NPVARIANT(
          object ? env->CallLongMethodA(object, id, parameters)
                 : env->CallStaticLongMethodA(clazz, id, parameters),
          *result);
      break;
    case JavaType::TypeFloat:
      DOUBLE_TO_NPVARIANT(
          object ? env->CallFloatMethodA(object, id, parameters)
                 : env->CallStaticFloatMethodA(clazz, id, parameters),
          *result);
      break;
    case JavaType::TypeDouble:
      DOUBLE_TO_NPVARIANT(
          object ? env->CallDoubleMethodA(object, id, parameters)
                 : env->CallStaticDoubleMethodA(clazz, id, parameters),
          *result);
      break;
    case JavaType::TypeVoid:
      if (object)
        env->CallVoidMethodA(object, id, parameters);
      else
        env->CallStaticVoidMethodA(clazz, id, parameters);
      VOID_TO_NPVARIANT(*result);
      break;
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to not call methods that
      // return arrays. Spec requires calling the method and converting the
      // result to a JavaScript array.
      VOID_TO_NPVARIANT(*result);
      break;
    case JavaType::TypeString: {
      jstring java_string = static_cast<jstring>(
          object ? env->CallObjectMethodA(object, id, parameters)
                 : env->CallStaticObjectMethodA(clazz, id, parameters));
      // If an exception was raised, we must clear it before calling most JNI
      // methods. ScopedJavaLocalRef is liable to make such calls, so we test
      // first.
      if (base::android::ClearException(env)) {
        return false;
      }
      ScopedJavaLocalRef<jstring> scoped_java_string(env, java_string);
      if (!scoped_java_string.obj()) {
        // LIVECONNECT_COMPLIANCE: Existing behavior is to return undefined.
        // Spec requires returning a null string.
        VOID_TO_NPVARIANT(*result);
        break;
      }
      std::string str =
          base::android::ConvertJavaStringToUTF8(scoped_java_string);
      size_t length = str.length();
      // This pointer is freed in _NPN_ReleaseVariantValue in
      // third_party/WebKit/Source/WebCore/bindings/v8/npruntime.cpp.
      char* buffer = static_cast<char*>(malloc(length));
      str.copy(buffer, length, 0);
      STRINGN_TO_NPVARIANT(buffer, length, *result);
      break;
    }
    case JavaType::TypeObject: {
      // If an exception was raised, we must clear it before calling most JNI
      // methods. ScopedJavaLocalRef is liable to make such calls, so we test
      // first.
      jobject java_object =
          object ? env->CallObjectMethodA(object, id, parameters)
                 : env->CallStaticObjectMethodA(clazz, id, parameters);
      if (base::android::ClearException(env)) {
        return false;
      }
      ScopedJavaLocalRef<jobject> scoped_java_object(env, java_object);
      if (!scoped_java_object.obj()) {
        NULL_TO_NPVARIANT(*result);
        break;
      }
      OBJECT_TO_NPVARIANT(JavaBoundObject::Create(scoped_java_object,
                                                  safe_annotation_clazz,
                                                  manager,
                                                  can_enumerate_methods),
                          *result);
      break;
    }
  }
  return !base::android::ClearException(env);
}

double RoundDoubleTowardsZero(const double& x) {
  if (std::isnan(x)) {
    return 0.0;
  }
  return x > 0.0 ? floor(x) : ceil(x);
}

// Rounds to jlong using Java's type conversion rules.
jlong RoundDoubleToLong(const double& x) {
  double intermediate = RoundDoubleTowardsZero(x);
  // The int64 limits can not be converted exactly to double values, so we
  // compare to custom constants. kint64max is 2^63 - 1, but the spacing
  // between double values in the the range 2^62 to 2^63 is 2^10. The cast is
  // required to silence a spurious gcc warning for integer overflow.
  const int64 limit = (GG_INT64_C(1) << 63) - static_cast<uint64>(1 << 10);
  DCHECK(limit > 0);
  const double kLargestDoubleLessThanInt64Max = limit;
  const double kSmallestDoubleGreaterThanInt64Min = -limit;
  if (intermediate > kLargestDoubleLessThanInt64Max) {
    return kint64max;
  }
  if (intermediate < kSmallestDoubleGreaterThanInt64Min) {
    return kint64min;
  }
  return static_cast<jlong>(intermediate);
}

// Rounds to jint using Java's type conversion rules.
jint RoundDoubleToInt(const double& x) {
  double intermediate = RoundDoubleTowardsZero(x);
  // The int32 limits cast exactly to double values.
  intermediate = std::min(intermediate, static_cast<double>(kint32max));
  intermediate = std::max(intermediate, static_cast<double>(kint32min));
  return static_cast<jint>(intermediate);
}

jvalue CoerceJavaScriptNumberToJavaValue(const NPVariant& variant,
                                         const JavaType& target_type,
                                         bool coerce_to_string) {
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_NUMBER_VALUES.

  // For conversion to numeric types, we need to replicate Java's type
  // conversion rules. This requires that for integer values, we simply discard
  // all but the lowest n buts, where n is the number of bits in the target
  // type. For double values, the logic is more involved.
  jvalue result;
  DCHECK(variant.type == NPVariantType_Int32 ||
         variant.type == NPVariantType_Double);
  bool is_double = variant.type == NPVariantType_Double;
  switch (target_type.type) {
    case JavaType::TypeByte:
      result.b = is_double ?
          static_cast<jbyte>(RoundDoubleToInt(NPVARIANT_TO_DOUBLE(variant))) :
          static_cast<jbyte>(NPVARIANT_TO_INT32(variant));
      break;
    case JavaType::TypeChar:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert double to 0.
      // Spec requires converting doubles similarly to how we convert doubles to
      // other numeric types.
      result.c = is_double ? 0 :
                             static_cast<jchar>(NPVARIANT_TO_INT32(variant));
      break;
    case JavaType::TypeShort:
      result.s = is_double ?
          static_cast<jshort>(RoundDoubleToInt(NPVARIANT_TO_DOUBLE(variant))) :
          static_cast<jshort>(NPVARIANT_TO_INT32(variant));
      break;
    case JavaType::TypeInt:
      result.i = is_double ? RoundDoubleToInt(NPVARIANT_TO_DOUBLE(variant)) :
                             NPVARIANT_TO_INT32(variant);
      break;
    case JavaType::TypeLong:
      result.j = is_double ? RoundDoubleToLong(NPVARIANT_TO_DOUBLE(variant)) :
                             NPVARIANT_TO_INT32(variant);
      break;
    case JavaType::TypeFloat:
      result.f = is_double ? static_cast<jfloat>(NPVARIANT_TO_DOUBLE(variant)) :
                             NPVARIANT_TO_INT32(variant);
      break;
    case JavaType::TypeDouble:
      result.d = is_double ? NPVARIANT_TO_DOUBLE(variant) :
                             NPVARIANT_TO_INT32(variant);
      break;
    case JavaType::TypeObject:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to null. Spec
      // requires handling object equivalents of primitive types.
      result.l = NULL;
      break;
    case JavaType::TypeString:
      result.l = coerce_to_string ?
          ConvertUTF8ToJavaString(
              AttachCurrentThread(),
              is_double ?
                  base::StringPrintf("%.6lg", NPVARIANT_TO_DOUBLE(variant)) :
                  base::Int64ToString(NPVARIANT_TO_INT32(variant))).Release() :
          NULL;
      break;
    case JavaType::TypeBoolean:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to false. Spec
      // requires converting to false for 0 or NaN, true otherwise.
      result.z = JNI_FALSE;
      break;
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to null. Spec
      // requires raising a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

jvalue CoerceJavaScriptBooleanToJavaValue(const NPVariant& variant,
                                          const JavaType& target_type,
                                          bool coerce_to_string) {
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_BOOLEAN_VALUES.
  DCHECK_EQ(NPVariantType_Bool, variant.type);
  bool boolean_value = NPVARIANT_TO_BOOLEAN(variant);
  jvalue result;
  switch (target_type.type) {
    case JavaType::TypeBoolean:
      result.z = boolean_value ? JNI_TRUE : JNI_FALSE;
      break;
    case JavaType::TypeObject:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
      // requires handling java.lang.Boolean and java.lang.Object.
      result.l = NULL;
      break;
    case JavaType::TypeString:
      result.l = coerce_to_string ?
          ConvertUTF8ToJavaString(AttachCurrentThread(),
                                  boolean_value ? "true" : "false").Release() :
          NULL;
      break;
    case JavaType::TypeByte:
    case JavaType::TypeChar:
    case JavaType::TypeShort:
    case JavaType::TypeInt:
    case JavaType::TypeLong:
    case JavaType::TypeFloat:
    case JavaType::TypeDouble: {
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to 0. Spec
      // requires converting to 0 or 1.
      jvalue null_value = {0};
      result = null_value;
      break;
    }
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
      // requires raising a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

jvalue CoerceJavaScriptStringToJavaValue(const NPVariant& variant,
                                         const JavaType& target_type) {
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_STRING_VALUES.
  DCHECK_EQ(NPVariantType_String, variant.type);
  jvalue result;
  switch (target_type.type) {
    case JavaType::TypeString:
      result.l = ConvertUTF8ToJavaString(
          AttachCurrentThread(),
          base::StringPiece(NPVARIANT_TO_STRING(variant).UTF8Characters,
                            NPVARIANT_TO_STRING(variant).UTF8Length)).Release();
      break;
    case JavaType::TypeObject:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
      // requires handling java.lang.Object.
      result.l = NULL;
      break;
    case JavaType::TypeByte:
    case JavaType::TypeShort:
    case JavaType::TypeInt:
    case JavaType::TypeLong:
    case JavaType::TypeFloat:
    case JavaType::TypeDouble: {
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to 0. Spec
      // requires using valueOf() method of corresponding object type.
      jvalue null_value = {0};
      result = null_value;
      break;
    }
    case JavaType::TypeChar:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to 0. Spec
      // requires using java.lang.Short.decode().
      result.c = 0;
      break;
    case JavaType::TypeBoolean:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to false. Spec
      // requires converting the empty string to false, otherwise true.
      result.z = JNI_FALSE;
      break;
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
      // requires raising a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

// Note that this only handles primitive types and strings.
jobject CreateJavaArray(const JavaType& type, jsize length) {
  JNIEnv* env = AttachCurrentThread();
  switch (type.type) {
    case JavaType::TypeBoolean:
      return env->NewBooleanArray(length);
    case JavaType::TypeByte:
      return env->NewByteArray(length);
    case JavaType::TypeChar:
      return env->NewCharArray(length);
    case JavaType::TypeShort:
      return env->NewShortArray(length);
    case JavaType::TypeInt:
      return env->NewIntArray(length);
    case JavaType::TypeLong:
      return env->NewLongArray(length);
    case JavaType::TypeFloat:
      return env->NewFloatArray(length);
    case JavaType::TypeDouble:
      return env->NewDoubleArray(length);
    case JavaType::TypeString: {
      ScopedJavaLocalRef<jclass> clazz(GetClass(env, "java/lang/String"));
      return env->NewObjectArray(length, clazz.obj(), NULL);
    }
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
    case JavaType::TypeArray:
    case JavaType::TypeObject:
      // Not handled.
      NOTREACHED();
  }
  return NULL;
}

// Sets the specified element of the supplied array to the value of the
// supplied jvalue. Requires that the type of the array matches that of the
// jvalue. Handles only primitive types and strings. Note that in the case of a
// string, the array takes a new reference to the string object.
void SetArrayElement(jobject array,
                     const JavaType& type,
                     jsize index,
                     const jvalue& value) {
  JNIEnv* env = AttachCurrentThread();
  switch (type.type) {
    case JavaType::TypeBoolean:
      env->SetBooleanArrayRegion(static_cast<jbooleanArray>(array), index, 1,
                                 &value.z);
      break;
    case JavaType::TypeByte:
      env->SetByteArrayRegion(static_cast<jbyteArray>(array), index, 1,
                              &value.b);
      break;
    case JavaType::TypeChar:
      env->SetCharArrayRegion(static_cast<jcharArray>(array), index, 1,
                              &value.c);
      break;
    case JavaType::TypeShort:
      env->SetShortArrayRegion(static_cast<jshortArray>(array), index, 1,
                               &value.s);
      break;
    case JavaType::TypeInt:
      env->SetIntArrayRegion(static_cast<jintArray>(array), index, 1,
                             &value.i);
      break;
    case JavaType::TypeLong:
      env->SetLongArrayRegion(static_cast<jlongArray>(array), index, 1,
                              &value.j);
      break;
    case JavaType::TypeFloat:
      env->SetFloatArrayRegion(static_cast<jfloatArray>(array), index, 1,
                               &value.f);
      break;
    case JavaType::TypeDouble:
      env->SetDoubleArrayRegion(static_cast<jdoubleArray>(array), index, 1,
                                &value.d);
      break;
    case JavaType::TypeString:
      env->SetObjectArrayElement(static_cast<jobjectArray>(array), index,
                                 value.l);
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
    case JavaType::TypeArray:
    case JavaType::TypeObject:
      // Not handled.
      NOTREACHED();
  }
  base::android::CheckException(env);
}

void ReleaseJavaValueIfRequired(JNIEnv* env,
                                jvalue* value,
                                const JavaType& type) {
  if (type.type == JavaType::TypeString ||
      type.type == JavaType::TypeObject ||
      type.type == JavaType::TypeArray) {
    env->DeleteLocalRef(value->l);
    value->l = NULL;
  }
}

jvalue CoerceJavaScriptValueToJavaValue(const NPVariant& variant,
                                        const JavaType& target_type,
                                        bool coerce_to_string);

// Returns a new local reference to a Java array.
jobject CoerceJavaScriptObjectToArray(const NPVariant& variant,
                                      const JavaType& target_type) {
  DCHECK_EQ(JavaType::TypeArray, target_type.type);
  NPObject* object = NPVARIANT_TO_OBJECT(variant);
  DCHECK_NE(&JavaNPObject::kNPClass, object->_class);

  const JavaType& target_inner_type = *target_type.inner_type.get();
  // LIVECONNECT_COMPLIANCE: Existing behavior is to return null for
  // multi-dimensional arrays. Spec requires handling multi-demensional arrays.
  if (target_inner_type.type == JavaType::TypeArray) {
    return NULL;
  }

  // LIVECONNECT_COMPLIANCE: Existing behavior is to return null for object
  // arrays. Spec requires handling object arrays.
  if (target_inner_type.type == JavaType::TypeObject) {
    return NULL;
  }

  // If the object does not have a length property, return null.
  NPVariant length_variant;
  if (!WebBindings::getProperty(0, object,
                                WebBindings::getStringIdentifier("length"),
                                &length_variant)) {
    WebBindings::releaseVariantValue(&length_variant);
    return NULL;
  }

  // If the length property does not have numeric type, or is outside the valid
  // range for a Java array length, return null.
  jsize length = -1;
  if (NPVARIANT_IS_INT32(length_variant)
      && NPVARIANT_TO_INT32(length_variant) >= 0) {
    length = NPVARIANT_TO_INT32(length_variant);
  } else if (NPVARIANT_IS_DOUBLE(length_variant)
             && NPVARIANT_TO_DOUBLE(length_variant) >= 0.0
             && NPVARIANT_TO_DOUBLE(length_variant) <= kint32max) {
    length = static_cast<jsize>(NPVARIANT_TO_DOUBLE(length_variant));
  }
  WebBindings::releaseVariantValue(&length_variant);
  if (length == -1) {
    return NULL;
  }

  // Create the Java array.
  // TODO(steveblock): Handle failure to create the array.
  jobject result = CreateJavaArray(target_inner_type, length);
  NPVariant value_variant;
  JNIEnv* env = AttachCurrentThread();
  for (jsize i = 0; i < length; ++i) {
    // It seems that getProperty() will set the variant to type void on failure,
    // but this doesn't seem to be documented, so do it explicitly here for
    // safety.
    VOID_TO_NPVARIANT(value_variant);
    // If this fails, for example due to a missing element, we simply treat the
    // value as JavaScript undefined.
    WebBindings::getProperty(0, object, WebBindings::getIntIdentifier(i),
                             &value_variant);
    jvalue element = CoerceJavaScriptValueToJavaValue(value_variant,
                                                      target_inner_type,
                                                      false);
    SetArrayElement(result, target_inner_type, i, element);
    // CoerceJavaScriptValueToJavaValue() creates new local references to
    // strings, objects and arrays. Of these, only strings can occur here.
    // SetArrayElement() causes the array to take its own reference to the
    // string, so we can now release the local reference.
    DCHECK_NE(JavaType::TypeObject, target_inner_type.type);
    DCHECK_NE(JavaType::TypeArray, target_inner_type.type);
    ReleaseJavaValueIfRequired(env, &element, target_inner_type);
    WebBindings::releaseVariantValue(&value_variant);
  }

  return result;
}

jvalue CoerceJavaScriptObjectToJavaValue(const NPVariant& variant,
                                         const JavaType& target_type,
                                         bool coerce_to_string) {
  // This covers both JavaScript objects (including arrays) and Java objects.
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_OTHER_OBJECTS,
  // http://jdk6.java.net/plugin2/liveconnect/#JS_ARRAY_VALUES and
  // http://jdk6.java.net/plugin2/liveconnect/#JS_JAVA_OBJECTS
  DCHECK_EQ(NPVariantType_Object, variant.type);

  NPObject* object = NPVARIANT_TO_OBJECT(variant);
  bool is_java_object = &JavaNPObject::kNPClass == object->_class;

  jvalue result;
  switch (target_type.type) {
    case JavaType::TypeObject:
      if (is_java_object) {
        // LIVECONNECT_COMPLIANCE: Existing behavior is to pass all Java
        // objects. Spec requires passing only Java objects which are
        // assignment-compatibile.
        result.l = AttachCurrentThread()->NewLocalRef(
            JavaBoundObject::GetJavaObject(object).obj());
      } else {
        // LIVECONNECT_COMPLIANCE: Existing behavior is to pass null. Spec
        // requires converting if the target type is
        // netscape.javascript.JSObject, otherwise raising a JavaScript
        // exception.
        result.l = NULL;
      }
      break;
    case JavaType::TypeString:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to
      // "undefined". Spec requires calling toString() on the Java object.
      result.l = coerce_to_string ?
          ConvertUTF8ToJavaString(AttachCurrentThread(), "undefined").
              Release() :
          NULL;
      break;
    case JavaType::TypeByte:
    case JavaType::TypeShort:
    case JavaType::TypeInt:
    case JavaType::TypeLong:
    case JavaType::TypeFloat:
    case JavaType::TypeDouble:
    case JavaType::TypeChar: {
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to 0. Spec
      // requires raising a JavaScript exception.
      jvalue null_value = {0};
      result = null_value;
      break;
    }
    case JavaType::TypeBoolean:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to false. Spec
      // requires raising a JavaScript exception.
      result.z = JNI_FALSE;
      break;
    case JavaType::TypeArray:
      if (is_java_object) {
        // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
        // requires raising a JavaScript exception.
        result.l = NULL;
      } else {
        result.l = CoerceJavaScriptObjectToArray(variant, target_type);
      }
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

jvalue CoerceJavaScriptNullOrUndefinedToJavaValue(const NPVariant& variant,
                                                  const JavaType& target_type,
                                                  bool coerce_to_string) {
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_NULL.
  DCHECK(variant.type == NPVariantType_Null ||
         variant.type == NPVariantType_Void);
  jvalue result;
  switch (target_type.type) {
    case JavaType::TypeObject:
      result.l = NULL;
      break;
    case JavaType::TypeString:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert undefined to
      // "undefined". Spec requires converting undefined to NULL.
      result.l = (coerce_to_string && variant.type == NPVariantType_Void) ?
          ConvertUTF8ToJavaString(AttachCurrentThread(), "undefined").
              Release() :
          NULL;
      break;
    case JavaType::TypeByte:
    case JavaType::TypeChar:
    case JavaType::TypeShort:
    case JavaType::TypeInt:
    case JavaType::TypeLong:
    case JavaType::TypeFloat:
    case JavaType::TypeDouble: {
      jvalue null_value = {0};
      result = null_value;
      break;
    }
    case JavaType::TypeBoolean:
      result.z = JNI_FALSE;
      break;
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
      // requires raising a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

// coerce_to_string means that we should try to coerce all JavaScript values to
// strings when required, rather than simply converting to NULL. This is used
// to maintain current behaviour, which differs slightly depending upon whether
// or not the coercion in question is for an array element.
//
// Note that the jvalue returned by this method may contain a new local
// reference to an object (string, object or array). This must be released by
// the caller.
jvalue CoerceJavaScriptValueToJavaValue(const NPVariant& variant,
                                        const JavaType& target_type,
                                        bool coerce_to_string) {
  // Note that in all these conversions, the relevant field of the jvalue must
  // always be explicitly set, as jvalue does not initialize its fields.

  switch (variant.type) {
    case NPVariantType_Int32:
    case NPVariantType_Double:
      return CoerceJavaScriptNumberToJavaValue(variant, target_type,
                                               coerce_to_string);
    case NPVariantType_Bool:
      return CoerceJavaScriptBooleanToJavaValue(variant, target_type,
                                                coerce_to_string);
    case NPVariantType_String:
      return CoerceJavaScriptStringToJavaValue(variant, target_type);
    case NPVariantType_Object:
      return CoerceJavaScriptObjectToJavaValue(variant, target_type,
                                               coerce_to_string);
    case NPVariantType_Null:
    case NPVariantType_Void:
      return CoerceJavaScriptNullOrUndefinedToJavaValue(variant, target_type,
                                                        coerce_to_string);
  }
  NOTREACHED();
  return jvalue();
}

}  // namespace

NPObject* JavaBoundObject::Create(
    const JavaRef<jobject>& object,
    const JavaRef<jclass>& safe_annotation_clazz,
    const base::WeakPtr<JavaBridgeDispatcherHostManager>& manager,
    bool can_enumerate_methods) {
  // The first argument (a plugin's instance handle) is passed through to the
  // allocate function directly, and we don't use it, so it's ok to be 0.
  // The object is created with a ref count of one.
  NPObject* np_object = WebBindings::createObject(0, const_cast<NPClass*>(
      &JavaNPObject::kNPClass));
  // The NPObject takes ownership of the JavaBoundObject.
  reinterpret_cast<JavaNPObject*>(np_object)->bound_object =
      new JavaBoundObject(
          object, safe_annotation_clazz, manager, can_enumerate_methods);
  return np_object;
}

JavaBoundObject::JavaBoundObject(
    const JavaRef<jobject>& object,
    const JavaRef<jclass>& safe_annotation_clazz,
    const base::WeakPtr<JavaBridgeDispatcherHostManager>& manager,
    bool can_enumerate_methods)
    : java_object_(AttachCurrentThread(), object.obj()),
      manager_(manager),
      are_methods_set_up_(false),
      object_get_class_method_id_(NULL),
      can_enumerate_methods_(can_enumerate_methods),
      safe_annotation_clazz_(safe_annotation_clazz) {
  BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&JavaBridgeDispatcherHostManager::JavaBoundObjectCreated,
                   manager_,
                   base::android::ScopedJavaGlobalRef<jobject>(object)));
  // Other than informing the JavaBridgeDispatcherHostManager that a java bound
  // object has been created (above), we don't do anything else with our Java
  // object when first created. We do it all lazily when a method is first
  // invoked.
}

JavaBoundObject::~JavaBoundObject() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&JavaBridgeDispatcherHostManager::JavaBoundObjectDestroyed,
                 manager_,
                 base::android::ScopedJavaGlobalRef<jobject>(
                     java_object_.get(AttachCurrentThread()))));
}

ScopedJavaLocalRef<jobject> JavaBoundObject::GetJavaObject(NPObject* object) {
  DCHECK_EQ(&JavaNPObject::kNPClass, object->_class);
  JavaBoundObject* jbo = reinterpret_cast<JavaNPObject*>(object)->bound_object;
  return jbo->java_object_.get(AttachCurrentThread());
}

std::vector<std::string> JavaBoundObject::GetMethodNames() const {
  EnsureMethodsAreSetUp();
  std::vector<std::string> result;
  for (JavaMethodMap::const_iterator it = methods_.begin();
       it != methods_.end();
       it = methods_.upper_bound(it->first)) {
    result.push_back(it->first);
  }
  return result;
}

bool JavaBoundObject::HasMethod(const std::string& name) const {
  EnsureMethodsAreSetUp();
  return methods_.find(name) != methods_.end();
}

bool JavaBoundObject::Invoke(const std::string& name, const NPVariant* args,
                             size_t arg_count, NPVariant* result) {
  EnsureMethodsAreSetUp();

  // Get all methods with the correct name.
  std::pair<JavaMethodMap::const_iterator, JavaMethodMap::const_iterator>
      iters = methods_.equal_range(name);
  if (iters.first == iters.second) {
    return false;
  }

  // Take the first method with the correct number of arguments.
  JavaMethod* method = NULL;
  for (JavaMethodMap::const_iterator iter = iters.first; iter != iters.second;
       ++iter) {
    if (iter->second->num_parameters() == arg_count) {
      method = iter->second.get();
      break;
    }
  }
  if (!method) {
    return false;
  }

  // Block access to java.lang.Object.getClass.
  // As it is declared to be final, it is sufficient to compare methodIDs.
  if (method->id() == object_get_class_method_id_) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&JavaBoundObject::ThrowSecurityException,
                   kAccessToObjectGetClassIsBlocked));
    return false;
  }

  // Coerce
  std::vector<jvalue> parameters(arg_count);
  for (size_t i = 0; i < arg_count; ++i) {
    parameters[i] = CoerceJavaScriptValueToJavaValue(args[i],
                                                     method->parameter_type(i),
                                                     true);
  }

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj;
  ScopedJavaLocalRef<jclass> cls;
  bool ok = false;
  if (method->is_static()) {
    cls = GetLocalClassRef(env);
  } else {
    obj = java_object_.get(env);
  }
  if (!obj.is_null() || !cls.is_null()) {
    // Call
    ok = CallJNIMethod(obj.obj(), cls.obj(), method->return_type(),
                       method->id(), &parameters[0], result,
                       safe_annotation_clazz_,
                       manager_,
                       can_enumerate_methods_);
  }

  // Now that we're done with the jvalue, release any local references created
  // by CoerceJavaScriptValueToJavaValue().
  for (size_t i = 0; i < arg_count; ++i) {
    ReleaseJavaValueIfRequired(env, &parameters[i], method->parameter_type(i));
  }

  return ok;
}

ScopedJavaLocalRef<jclass> JavaBoundObject::GetLocalClassRef(
    JNIEnv* env) const {
  if (!object_get_class_method_id_) {
    object_get_class_method_id_ = GetMethodIDFromClassName(
        env, kJavaLangObject, kGetClass, kReturningJavaLangClass);
  }

  ScopedJavaLocalRef<jobject> obj = java_object_.get(env);
  if (!obj.is_null()) {
    return ScopedJavaLocalRef<jclass>(env, static_cast<jclass>(
        env->CallObjectMethod(obj.obj(), object_get_class_method_id_)));
  } else {
    return ScopedJavaLocalRef<jclass>();
  }
}

void JavaBoundObject::EnsureMethodsAreSetUp() const {
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
    methods_.insert(std::make_pair(method->name(), method));
  }
}

// static
void JavaBoundObject::ThrowSecurityException(const char* message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jclass> clazz(
      env, env->FindClass(kJavaLangSecurityExceptionClass));
  env->ThrowNew(clazz.obj(), message);
}

}  // namespace content
