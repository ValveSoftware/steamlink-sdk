// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/gin_java_bridge_value.h"

namespace content {

namespace {

// The magic value is only used to prevent accidental attempts of reading
// GinJavaBridgeValue from a random BinaryValue.  GinJavaBridgeValue is not
// intended for scenarios where with BinaryValues are being used for anything
// else than holding GinJavaBridgeValues.  If a need for such scenario ever
// emerges, the best solution would be to extend GinJavaBridgeValue to be able
// to wrap raw BinaryValues.
const uint32 kHeaderMagic = 0xBEEFCAFE;

#pragma pack(push, 4)
struct Header : public Pickle::Header {
  uint32 magic;
  int32 type;
};
#pragma pack(pop)

}

// static
scoped_ptr<base::BinaryValue> GinJavaBridgeValue::CreateUndefinedValue() {
  GinJavaBridgeValue gin_value(TYPE_UNDEFINED);
  return make_scoped_ptr(gin_value.SerializeToBinaryValue());
}

// static
scoped_ptr<base::BinaryValue> GinJavaBridgeValue::CreateNonFiniteValue(
    float in_value) {
  GinJavaBridgeValue gin_value(TYPE_NONFINITE);
  gin_value.pickle_.WriteFloat(in_value);
  return make_scoped_ptr(gin_value.SerializeToBinaryValue());
}

// static
scoped_ptr<base::BinaryValue> GinJavaBridgeValue::CreateNonFiniteValue(
    double in_value) {
  return CreateNonFiniteValue(static_cast<float>(in_value)).Pass();
}

// static
scoped_ptr<base::BinaryValue> GinJavaBridgeValue::CreateObjectIDValue(
    int32 in_value) {
  GinJavaBridgeValue gin_value(TYPE_OBJECT_ID);
  gin_value.pickle_.WriteInt(in_value);
  return make_scoped_ptr(gin_value.SerializeToBinaryValue());
}

// static
bool GinJavaBridgeValue::ContainsGinJavaBridgeValue(const base::Value* value) {
  if (!value->IsType(base::Value::TYPE_BINARY))
    return false;
  const base::BinaryValue* binary_value =
      reinterpret_cast<const base::BinaryValue*>(value);
  if (binary_value->GetSize() < sizeof(Header))
    return false;
  Pickle pickle(binary_value->GetBuffer(), binary_value->GetSize());
  // Broken binary value: payload or header size is wrong
  if (!pickle.data() || pickle.size() - pickle.payload_size() != sizeof(Header))
    return false;
  Header* header = pickle.headerT<Header>();
  return (header->magic == kHeaderMagic &&
          header->type >= TYPE_FIRST_VALUE && header->type < TYPE_LAST_VALUE);
}

// static
scoped_ptr<const GinJavaBridgeValue> GinJavaBridgeValue::FromValue(
    const base::Value* value) {
  return scoped_ptr<const GinJavaBridgeValue>(
      value->IsType(base::Value::TYPE_BINARY)
          ? new GinJavaBridgeValue(
                reinterpret_cast<const base::BinaryValue*>(value))
          : NULL);
}

GinJavaBridgeValue::Type GinJavaBridgeValue::GetType() const {
  const Header* header = pickle_.headerT<Header>();
  DCHECK(header->type >= TYPE_FIRST_VALUE && header->type < TYPE_LAST_VALUE);
  return static_cast<Type>(header->type);
}

bool GinJavaBridgeValue::IsType(Type type) const {
  return GetType() == type;
}

bool GinJavaBridgeValue::GetAsNonFinite(float* out_value) const {
  if (GetType() == TYPE_NONFINITE) {
    PickleIterator iter(pickle_);
    return iter.ReadFloat(out_value);
  } else {
    return false;
  }
}

bool GinJavaBridgeValue::GetAsObjectID(int32* out_object_id) const {
  if (GetType() == TYPE_OBJECT_ID) {
    PickleIterator iter(pickle_);
    return iter.ReadInt(out_object_id);
  } else {
    return false;
  }
}

GinJavaBridgeValue::GinJavaBridgeValue(Type type) :
    pickle_(sizeof(Header)) {
  Header* header = pickle_.headerT<Header>();
  header->magic = kHeaderMagic;
  header->type = type;
}

GinJavaBridgeValue::GinJavaBridgeValue(const base::BinaryValue* value)
    : pickle_(value->GetBuffer(), value->GetSize()) {
  DCHECK(ContainsGinJavaBridgeValue(value));
}

base::BinaryValue* GinJavaBridgeValue::SerializeToBinaryValue() {
  return base::BinaryValue::CreateWithCopiedBuffer(
      reinterpret_cast<const char*>(pickle_.data()), pickle_.size());
}

}  // namespace content
