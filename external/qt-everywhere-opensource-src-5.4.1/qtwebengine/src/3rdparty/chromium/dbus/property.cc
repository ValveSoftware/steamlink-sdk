// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/property.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"

#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace dbus {

//
// PropertyBase implementation.
//

void PropertyBase::Init(PropertySet* property_set, const std::string& name) {
  DCHECK(!property_set_);
  property_set_ = property_set;
  name_ = name;
}


//
// PropertySet implementation.
//

PropertySet::PropertySet(
    ObjectProxy* object_proxy,
    const std::string& interface,
    const PropertyChangedCallback& property_changed_callback)
    : object_proxy_(object_proxy),
      interface_(interface),
      property_changed_callback_(property_changed_callback),
      weak_ptr_factory_(this) {}

PropertySet::~PropertySet() {
}

void PropertySet::RegisterProperty(const std::string& name,
                                   PropertyBase* property) {
  property->Init(this, name);
  properties_map_[name] = property;
}

void PropertySet::ConnectSignals() {
  DCHECK(object_proxy_);
  object_proxy_->ConnectToSignal(
      kPropertiesInterface,
      kPropertiesChanged,
      base::Bind(&PropertySet::ChangedReceived,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&PropertySet::ChangedConnected,
                 weak_ptr_factory_.GetWeakPtr()));
}


void PropertySet::ChangedReceived(Signal* signal) {
  DCHECK(signal);
  MessageReader reader(signal);

  std::string interface;
  if (!reader.PopString(&interface)) {
    LOG(WARNING) << "Property changed signal has wrong parameters: "
                 << "expected interface name: " << signal->ToString();
    return;
  }

  if (interface != this->interface())
    return;

  if (!UpdatePropertiesFromReader(&reader)) {
    LOG(WARNING) << "Property changed signal has wrong parameters: "
                 << "expected dictionary: " << signal->ToString();
  }

  // TODO(keybuk): dbus properties api has invalidated properties array
  // on the end, we don't handle this right now because I don't know of
  // any service that sends it - or what they expect us to do with it.
  // Add later when we need it.
}

void PropertySet::ChangedConnected(const std::string& interface_name,
                                   const std::string& signal_name,
                                   bool success) {
  LOG_IF(WARNING, !success) << "Failed to connect to " << signal_name
                            << "signal.";
}


void PropertySet::Get(PropertyBase* property, GetCallback callback) {
  MethodCall method_call(kPropertiesInterface, kPropertiesGet);
  MessageWriter writer(&method_call);
  writer.AppendString(interface());
  writer.AppendString(property->name());

  DCHECK(object_proxy_);
  object_proxy_->CallMethod(&method_call,
                            ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::Bind(&PropertySet::OnGet,
                                       GetWeakPtr(),
                                       property,
                                       callback));
}

void PropertySet::OnGet(PropertyBase* property, GetCallback callback,
                        Response* response) {
  if (!response) {
    LOG(WARNING) << property->name() << ": Get: failed.";
    return;
  }

  MessageReader reader(response);
  if (property->PopValueFromReader(&reader))
    NotifyPropertyChanged(property->name());

  if (!callback.is_null())
    callback.Run(response);
}

void PropertySet::GetAll() {
  MethodCall method_call(kPropertiesInterface, kPropertiesGetAll);
  MessageWriter writer(&method_call);
  writer.AppendString(interface());

  DCHECK(object_proxy_);
  object_proxy_->CallMethod(&method_call,
                            ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::Bind(&PropertySet::OnGetAll,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void PropertySet::OnGetAll(Response* response) {
  if (!response) {
    LOG(WARNING) << "GetAll request failed.";
    return;
  }

  MessageReader reader(response);
  if (!UpdatePropertiesFromReader(&reader)) {
    LOG(WARNING) << "GetAll response has wrong parameters: "
                 << "expected dictionary: " << response->ToString();
  }
}

void PropertySet::Set(PropertyBase* property, SetCallback callback) {
  MethodCall method_call(kPropertiesInterface, kPropertiesSet);
  MessageWriter writer(&method_call);
  writer.AppendString(interface());
  writer.AppendString(property->name());
  property->AppendSetValueToWriter(&writer);

  DCHECK(object_proxy_);
  object_proxy_->CallMethod(&method_call,
                            ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::Bind(&PropertySet::OnSet,
                                       GetWeakPtr(),
                                       property,
                                       callback));
}

void PropertySet::OnSet(PropertyBase* property, SetCallback callback,
                        Response* response) {
  LOG_IF(WARNING, !response) << property->name() << ": Set: failed.";
  if (!callback.is_null())
    callback.Run(response);
}

bool PropertySet::UpdatePropertiesFromReader(MessageReader* reader) {
  DCHECK(reader);
  MessageReader array_reader(NULL);
  if (!reader->PopArray(&array_reader))
    return false;

  while (array_reader.HasMoreData()) {
    MessageReader dict_entry_reader(NULL);
    if (array_reader.PopDictEntry(&dict_entry_reader))
      UpdatePropertyFromReader(&dict_entry_reader);
  }

  return true;
}

bool PropertySet::UpdatePropertyFromReader(MessageReader* reader) {
  DCHECK(reader);

  std::string name;
  if (!reader->PopString(&name))
    return false;

  PropertiesMap::iterator it = properties_map_.find(name);
  if (it == properties_map_.end())
    return false;

  PropertyBase* property = it->second;
  if (property->PopValueFromReader(reader)) {
    NotifyPropertyChanged(name);
    return true;
  } else {
    return false;
  }
}


void PropertySet::NotifyPropertyChanged(const std::string& name) {
  if (!property_changed_callback_.is_null())
    property_changed_callback_.Run(name);
}

//
// Property<Byte> specialization.
//

template <>
Property<uint8>::Property() : value_(0) {
}

template <>
bool Property<uint8>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfByte(&value_);
}

template <>
void Property<uint8>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfByte(set_value_);
}

//
// Property<bool> specialization.
//

template <>
Property<bool>::Property() : value_(false) {
}

template <>
bool Property<bool>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfBool(&value_);
}

template <>
void Property<bool>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfBool(set_value_);
}

//
// Property<int16> specialization.
//

template <>
Property<int16>::Property() : value_(0) {
}

template <>
bool Property<int16>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfInt16(&value_);
}

template <>
void Property<int16>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfInt16(set_value_);
}

//
// Property<uint16> specialization.
//

template <>
Property<uint16>::Property() : value_(0) {
}

template <>
bool Property<uint16>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfUint16(&value_);
}

template <>
void Property<uint16>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfUint16(set_value_);
}

//
// Property<int32> specialization.
//

template <>
Property<int32>::Property() : value_(0) {
}

template <>
bool Property<int32>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfInt32(&value_);
}

template <>
void Property<int32>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfInt32(set_value_);
}

//
// Property<uint32> specialization.
//

template <>
Property<uint32>::Property() : value_(0) {
}

template <>
bool Property<uint32>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfUint32(&value_);
}

template <>
void Property<uint32>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfUint32(set_value_);
}

//
// Property<int64> specialization.
//

template <>
Property<int64>::Property() : value_(0), set_value_(0) {
}

template <>
bool Property<int64>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfInt64(&value_);
}

template <>
void Property<int64>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfInt64(set_value_);
}

//
// Property<uint64> specialization.
//

template <>
Property<uint64>::Property() : value_(0) {
}

template <>
bool Property<uint64>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfUint64(&value_);
}

template <>
void Property<uint64>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfUint64(set_value_);
}

//
// Property<double> specialization.
//

template <>
Property<double>::Property() : value_(0.0) {
}

template <>
bool Property<double>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfDouble(&value_);
}

template <>
void Property<double>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfDouble(set_value_);
}

//
// Property<std::string> specialization.
//

template <>
bool Property<std::string>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfString(&value_);
}

template <>
void Property<std::string>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfString(set_value_);
}

//
// Property<ObjectPath> specialization.
//

template <>
bool Property<ObjectPath>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfObjectPath(&value_);
}

template <>
void Property<ObjectPath>::AppendSetValueToWriter(MessageWriter* writer) {
  writer->AppendVariantOfObjectPath(set_value_);
}

//
// Property<std::vector<std::string> > specialization.
//

template <>
bool Property<std::vector<std::string> >::PopValueFromReader(
    MessageReader* reader) {
  MessageReader variant_reader(NULL);
  if (!reader->PopVariant(&variant_reader))
    return false;

  value_.clear();
  return variant_reader.PopArrayOfStrings(&value_);
}

template <>
void Property<std::vector<std::string> >::AppendSetValueToWriter(
    MessageWriter* writer) {
  MessageWriter variant_writer(NULL);
  writer->OpenVariant("as", &variant_writer);
  variant_writer.AppendArrayOfStrings(set_value_);
  writer->CloseContainer(&variant_writer);
}

//
// Property<std::vector<ObjectPath> > specialization.
//

template <>
bool Property<std::vector<ObjectPath> >::PopValueFromReader(
    MessageReader* reader) {
  MessageReader variant_reader(NULL);
  if (!reader->PopVariant(&variant_reader))
    return false;

  value_.clear();
  return variant_reader.PopArrayOfObjectPaths(&value_);
}

template <>
void Property<std::vector<ObjectPath> >::AppendSetValueToWriter(
    MessageWriter* writer) {
  MessageWriter variant_writer(NULL);
  writer->OpenVariant("ao", &variant_writer);
  variant_writer.AppendArrayOfObjectPaths(set_value_);
  writer->CloseContainer(&variant_writer);
}

//
// Property<std::vector<uint8> > specialization.
//

template <>
bool Property<std::vector<uint8> >::PopValueFromReader(MessageReader* reader) {
  MessageReader variant_reader(NULL);
  if (!reader->PopVariant(&variant_reader))
    return false;

  value_.clear();
  const uint8* bytes = NULL;
  size_t length = 0;
  if (!variant_reader.PopArrayOfBytes(&bytes, &length))
    return false;
  value_.assign(bytes, bytes + length);
  return true;
}

template <>
void Property<std::vector<uint8> >::AppendSetValueToWriter(
    MessageWriter* writer) {
  MessageWriter variant_writer(NULL);
  writer->OpenVariant("ay", &variant_writer);
  variant_writer.AppendArrayOfBytes(set_value_.data(), set_value_.size());
  writer->CloseContainer(&variant_writer);
}

}  // namespace dbus
