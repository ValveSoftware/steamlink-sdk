// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/java/java_type.h"

#include "base/logging.h"

namespace content {
namespace {

JavaType JavaTypeFromJNIName(const std::string& jni_name) {
  JavaType result;
  DCHECK(!jni_name.empty());
  switch (jni_name[0]) {
    case 'Z':
      result.type = JavaType::TypeBoolean;
      break;
    case 'B':
      result.type = JavaType::TypeByte;
      break;
    case 'C':
      result.type = JavaType::TypeChar;
      break;
    case 'S':
      result.type = JavaType::TypeShort;
      break;
    case 'I':
      result.type = JavaType::TypeInt;
      break;
    case 'J':
      result.type = JavaType::TypeLong;
      break;
    case 'F':
      result.type = JavaType::TypeFloat;
      break;
    case 'D':
      result.type = JavaType::TypeDouble;
      break;
    case '[':
      result.type = JavaType::TypeArray;
      // LIVECONNECT_COMPLIANCE: We don't support multi-dimensional arrays, so
      // there's no need to populate the inner types.
      break;
    case 'L':
      result.type = jni_name == "Ljava.lang.String;" ?
                    JavaType::TypeString :
                    JavaType::TypeObject;
      break;
    default:
      // Includes void (V).
      NOTREACHED();
  }
  return result;
}

}  // namespace

JavaType::JavaType() {
}

JavaType::JavaType(const JavaType& other) {
  *this = other;
}

JavaType::~JavaType() {
}

JavaType& JavaType::operator=(const JavaType& other) {
  type = other.type;
  if (other.inner_type) {
    DCHECK_EQ(JavaType::TypeArray, type);
    inner_type.reset(new JavaType(*other.inner_type));
  } else {
    inner_type.reset();
  }
  return *this;
}

JavaType JavaType::CreateFromBinaryName(const std::string& binary_name) {
  JavaType result;
  DCHECK(!binary_name.empty());
  if (binary_name == "boolean") {
    result.type = JavaType::TypeBoolean;
  } else if (binary_name == "byte") {
    result.type = JavaType::TypeByte;
  } else if (binary_name == "char") {
    result.type = JavaType::TypeChar;
  } else if (binary_name == "short") {
    result.type = JavaType::TypeShort;
  } else if (binary_name == "int") {
    result.type = JavaType::TypeInt;
  } else if (binary_name == "long") {
    result.type = JavaType::TypeLong;
  } else if (binary_name == "float") {
    result.type = JavaType::TypeFloat;
  } else if (binary_name == "double") {
    result.type = JavaType::TypeDouble;
  } else if (binary_name == "void") {
    result.type = JavaType::TypeVoid;
  } else if (binary_name[0] == '[') {
    result.type = JavaType::TypeArray;
    // The inner type of an array is represented in JNI format.
    result.inner_type.reset(new JavaType(JavaTypeFromJNIName(
        binary_name.substr(1))));
  } else if (binary_name == "java.lang.String") {
    result.type = JavaType::TypeString;
  } else {
    result.type = JavaType::TypeObject;
  }
  return result;
}

}  // namespace content
