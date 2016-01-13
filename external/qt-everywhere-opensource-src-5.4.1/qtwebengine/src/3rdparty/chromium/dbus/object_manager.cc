// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/object_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"

namespace dbus {

ObjectManager::Object::Object()
  : object_proxy(NULL) {
}

ObjectManager::Object::~Object() {
}

ObjectManager::ObjectManager(Bus* bus,
                             const std::string& service_name,
                             const ObjectPath& object_path)
    : bus_(bus),
      service_name_(service_name),
      object_path_(object_path),
      weak_ptr_factory_(this) {
  DVLOG(1) << "Creating ObjectManager for " << service_name_
           << " " << object_path_.value();

  DCHECK(bus_);
  object_proxy_ = bus_->GetObjectProxy(service_name_, object_path_);
  object_proxy_->SetNameOwnerChangedCallback(
      base::Bind(&ObjectManager::NameOwnerChanged,
                 weak_ptr_factory_.GetWeakPtr()));

  object_proxy_->ConnectToSignal(
      kObjectManagerInterface,
      kObjectManagerInterfacesAdded,
      base::Bind(&ObjectManager::InterfacesAddedReceived,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ObjectManager::InterfacesAddedConnected,
                 weak_ptr_factory_.GetWeakPtr()));

  object_proxy_->ConnectToSignal(
      kObjectManagerInterface,
      kObjectManagerInterfacesRemoved,
      base::Bind(&ObjectManager::InterfacesRemovedReceived,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ObjectManager::InterfacesRemovedConnected,
                 weak_ptr_factory_.GetWeakPtr()));

  GetManagedObjects();
}

ObjectManager::~ObjectManager() {
  // Clean up Object structures
  for (ObjectMap::iterator iter = object_map_.begin();
       iter != object_map_.end(); ++iter) {
    Object* object = iter->second;

    for (Object::PropertiesMap::iterator piter = object->properties_map.begin();
         piter != object->properties_map.end(); ++piter) {
      PropertySet* properties = piter->second;
      delete properties;
    }

    delete object;
  }
}

void ObjectManager::RegisterInterface(const std::string& interface_name,
                                      Interface* interface) {
  interface_map_[interface_name] = interface;
}

void ObjectManager::UnregisterInterface(const std::string& interface_name) {
  InterfaceMap::iterator iter = interface_map_.find(interface_name);
  if (iter != interface_map_.end())
    interface_map_.erase(iter);
}

std::vector<ObjectPath> ObjectManager::GetObjects() {
  std::vector<ObjectPath> object_paths;

  for (ObjectMap::iterator iter = object_map_.begin();
       iter != object_map_.end(); ++iter)
    object_paths.push_back(iter->first);

  return object_paths;
}

std::vector<ObjectPath> ObjectManager::GetObjectsWithInterface(
      const std::string& interface_name) {
  std::vector<ObjectPath> object_paths;

  for (ObjectMap::iterator oiter = object_map_.begin();
       oiter != object_map_.end(); ++oiter) {
    Object* object = oiter->second;

    Object::PropertiesMap::iterator piter =
        object->properties_map.find(interface_name);
    if (piter != object->properties_map.end())
      object_paths.push_back(oiter->first);
  }

  return object_paths;
}

ObjectProxy* ObjectManager::GetObjectProxy(const ObjectPath& object_path) {
  ObjectMap::iterator iter = object_map_.find(object_path);
  if (iter == object_map_.end())
    return NULL;

  Object* object = iter->second;
  return object->object_proxy;
}

PropertySet* ObjectManager::GetProperties(const ObjectPath& object_path,
                                          const std::string& interface_name) {
  ObjectMap::iterator iter = object_map_.find(object_path);
  if (iter == object_map_.end())
    return NULL;

  Object* object = iter->second;
  Object::PropertiesMap::iterator piter =
      object->properties_map.find(interface_name);
  if (piter == object->properties_map.end())
    return NULL;

  return piter->second;
}

void ObjectManager::GetManagedObjects() {
  MethodCall method_call(kObjectManagerInterface,
                         kObjectManagerGetManagedObjects);

  object_proxy_->CallMethod(
      &method_call,
      ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&ObjectManager::OnGetManagedObjects,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ObjectManager::OnGetManagedObjects(Response* response) {
  if (response != NULL) {
    MessageReader reader(response);
    MessageReader array_reader(NULL);
    if (!reader.PopArray(&array_reader))
      return;

    while (array_reader.HasMoreData()) {
      MessageReader dict_entry_reader(NULL);
      ObjectPath object_path;
      if (!array_reader.PopDictEntry(&dict_entry_reader) ||
          !dict_entry_reader.PopObjectPath(&object_path))
        continue;

      UpdateObject(object_path, &dict_entry_reader);
    }

  } else {
    LOG(WARNING) << service_name_ << " " << object_path_.value()
                 << ": Failed to get managed objects";
  }
}

void ObjectManager::InterfacesAddedReceived(Signal* signal) {
  DCHECK(signal);
  MessageReader reader(signal);
  ObjectPath object_path;
  if (!reader.PopObjectPath(&object_path)) {
    LOG(WARNING) << service_name_ << " " << object_path_.value()
                 << ": InterfacesAdded signal has incorrect parameters: "
                 << signal->ToString();
    return;
  }

  UpdateObject(object_path, &reader);
}

void ObjectManager::InterfacesAddedConnected(const std::string& interface_name,
                                             const std::string& signal_name,
                                             bool success) {
  LOG_IF(WARNING, !success) << service_name_ << " " << object_path_.value()
                            << ": Failed to connect to InterfacesAdded signal.";
}

void ObjectManager::InterfacesRemovedReceived(Signal* signal) {
  DCHECK(signal);
  MessageReader reader(signal);
  ObjectPath object_path;
  std::vector<std::string> interface_names;
  if (!reader.PopObjectPath(&object_path) ||
      !reader.PopArrayOfStrings(&interface_names)) {
    LOG(WARNING) << service_name_ << " " << object_path_.value()
                 << ": InterfacesRemoved signal has incorrect parameters: "
                 << signal->ToString();
    return;
  }

  for (size_t i = 0; i < interface_names.size(); ++i)
    RemoveInterface(object_path, interface_names[i]);
}

void ObjectManager::InterfacesRemovedConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  LOG_IF(WARNING, !success) << service_name_ << " " << object_path_.value()
                            << ": Failed to connect to "
                            << "InterfacesRemoved signal.";
}

void ObjectManager::UpdateObject(const ObjectPath& object_path,
                                 MessageReader* reader) {
  DCHECK(reader);
  MessageReader array_reader(NULL);
  if (!reader->PopArray(&array_reader))
    return;

  while (array_reader.HasMoreData()) {
    MessageReader dict_entry_reader(NULL);
    std::string interface_name;
    if (!array_reader.PopDictEntry(&dict_entry_reader) ||
        !dict_entry_reader.PopString(&interface_name))
      continue;

    AddInterface(object_path, interface_name, &dict_entry_reader);
  }
}


void ObjectManager::AddInterface(const ObjectPath& object_path,
                                 const std::string& interface_name,
                                 MessageReader* reader) {
  InterfaceMap::iterator iiter = interface_map_.find(interface_name);
  if (iiter == interface_map_.end())
    return;
  Interface* interface = iiter->second;

  ObjectMap::iterator oiter = object_map_.find(object_path);
  Object* object;
  if (oiter == object_map_.end()) {
    object = object_map_[object_path] = new Object;
    object->object_proxy = bus_->GetObjectProxy(service_name_, object_path);
  } else
    object = oiter->second;

  Object::PropertiesMap::iterator piter =
      object->properties_map.find(interface_name);
  PropertySet* property_set;
  const bool interface_added = (piter == object->properties_map.end());
  if (interface_added) {
    property_set = object->properties_map[interface_name] =
        interface->CreateProperties(object->object_proxy,
                                    object_path, interface_name);
    property_set->ConnectSignals();
  } else
    property_set = piter->second;

  property_set->UpdatePropertiesFromReader(reader);

  if (interface_added)
    interface->ObjectAdded(object_path, interface_name);
}

void ObjectManager::RemoveInterface(const ObjectPath& object_path,
                                    const std::string& interface_name) {
  ObjectMap::iterator oiter = object_map_.find(object_path);
  if (oiter == object_map_.end())
    return;
  Object* object = oiter->second;

  Object::PropertiesMap::iterator piter =
      object->properties_map.find(interface_name);
  if (piter == object->properties_map.end())
    return;

  // Inform the interface before removing the properties structure or object
  // in case it needs details from them to make its own decisions.
  InterfaceMap::iterator iiter = interface_map_.find(interface_name);
  if (iiter != interface_map_.end()) {
    Interface* interface = iiter->second;
    interface->ObjectRemoved(object_path, interface_name);
  }

  object->properties_map.erase(piter);

  if (object->properties_map.empty()) {
    object_map_.erase(oiter);
    delete object;
  }
}

void ObjectManager::NameOwnerChanged(const std::string& old_owner,
                                     const std::string& new_owner) {
  if (!old_owner.empty()) {
    ObjectMap::iterator iter = object_map_.begin();
    while (iter != object_map_.end()) {
      ObjectMap::iterator tmp = iter;
      ++iter;

      // PropertiesMap is mutated by RemoveInterface, and also Object is
      // destroyed; easier to collect the object path and interface names
      // and remove them safely.
      const dbus::ObjectPath object_path = tmp->first;
      Object* object = tmp->second;
      std::vector<std::string> interfaces;

      for (Object::PropertiesMap::iterator piter =
              object->properties_map.begin();
           piter != object->properties_map.end(); ++piter)
        interfaces.push_back(piter->first);

      for (std::vector<std::string>::iterator iiter = interfaces.begin();
           iiter != interfaces.end(); ++iiter)
        RemoveInterface(object_path, *iiter);
    }

  }

  if (!new_owner.empty())
    GetManagedObjects();
}

}  // namespace dbus
