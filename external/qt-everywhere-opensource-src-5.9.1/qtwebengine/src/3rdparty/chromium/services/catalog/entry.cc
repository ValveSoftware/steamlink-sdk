// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/entry.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "services/catalog/store.h"


namespace catalog {
namespace {

bool ReadStringSet(const base::ListValue& list_value,
                   std::set<std::string>* string_set) {
  DCHECK(string_set);
  for (const auto& value_value : list_value) {
    std::string value;
    if (!value_value->GetAsString(&value)) {
      LOG(ERROR) << "Entry::Deserialize: list member must be a string";
      return false;
    }
    string_set->insert(value);
  }
  return true;
}

bool ReadStringSetFromValue(const base::Value& value,
                            std::set<std::string>* string_set) {
  const base::ListValue* list_value = nullptr;
  if (!value.GetAsList(&list_value)) {
    LOG(ERROR) << "Entry::Deserialize: Value must be a list.";
    return false;
  }
  return ReadStringSet(*list_value, string_set);
}

bool BuildInterfaceProviderSpec(
    const base::DictionaryValue& value,
    service_manager::InterfaceProviderSpec* interface_provider_specs) {
  DCHECK(interface_provider_specs);
  const base::DictionaryValue* provides_value = nullptr;
  if (value.HasKey(Store::kInterfaceProviderSpecs_ProvidesKey) &&
      !value.GetDictionary(Store::kInterfaceProviderSpecs_ProvidesKey,
                           &provides_value)) {
    LOG(ERROR) << "Entry::Deserialize: "
               << Store::kInterfaceProviderSpecs_ProvidesKey
               << " must be a dictionary.";
    return false;
  }
  if (provides_value) {
    base::DictionaryValue::Iterator it(*provides_value);
    for(; !it.IsAtEnd(); it.Advance()) {
      service_manager::InterfaceSet interfaces;
      if (!ReadStringSetFromValue(it.value(), &interfaces)) {
        LOG(ERROR) << "Entry::Deserialize: Invalid interface list in provided "
                   << " capabilities dictionary";
        return false;
      }
      interface_provider_specs->provides[it.key()] = interfaces;
    }
  }

  const base::DictionaryValue* requires_value = nullptr;
  if (value.HasKey(Store::kInterfaceProviderSpecs_RequiresKey) &&
      !value.GetDictionary(Store::kInterfaceProviderSpecs_RequiresKey,
                           &requires_value)) {
    LOG(ERROR) << "Entry::Deserialize: "
               << Store::kInterfaceProviderSpecs_RequiresKey
               << " must be a dictionary.";
    return false;
  }
  if (requires_value) {
    base::DictionaryValue::Iterator it(*requires_value);
    for (; !it.IsAtEnd(); it.Advance()) {
      service_manager::CapabilitySet capabilities;
      const base::ListValue* entry_value = nullptr;
      if (!it.value().GetAsList(&entry_value)) {
        LOG(ERROR) << "Entry::Deserialize: "
                   << Store::kInterfaceProviderSpecs_RequiresKey
                   << " entry must be a list.";
        return false;
      }
      if (!ReadStringSet(*entry_value, &capabilities)) {
        LOG(ERROR) << "Entry::Deserialize: Invalid capabilities list in "
                   << "requires dictionary.";
        return false;
      }

      interface_provider_specs->requires[it.key()] = capabilities;
    }
  }
  return true;
}

}  // namespace

Entry::Entry() {}
Entry::Entry(const std::string& name)
    : name_(name),
      display_name_(name) {}
Entry::~Entry() {}

std::unique_ptr<base::DictionaryValue> Entry::Serialize() const {
  auto value = base::MakeUnique<base::DictionaryValue>();
  value->SetString(Store::kNameKey, name_);
  value->SetString(Store::kDisplayNameKey, display_name_);

  auto specs = base::MakeUnique<base::DictionaryValue>();
  for (const auto& it : interface_provider_specs_) {
    auto spec = base::MakeUnique<base::DictionaryValue>();

    auto provides = base::MakeUnique<base::DictionaryValue>();
    for (const auto& i : it.second.provides) {
      auto interfaces = base::MakeUnique<base::ListValue>();
      for (const auto& interface_name : i.second)
        interfaces->AppendString(interface_name);
      provides->Set(i.first, std::move(interfaces));
    }
    spec->Set(Store::kInterfaceProviderSpecs_ProvidesKey, std::move(provides));

    auto requires = base::MakeUnique<base::DictionaryValue>();
    for (const auto& i : it.second.requires) {
      auto capabilities = base::MakeUnique<base::ListValue>();
      for (const auto& capability : i.second)
        capabilities->AppendString(capability);
      requires->Set(i.first, std::move(capabilities));
    }
    spec->Set(Store::kInterfaceProviderSpecs_RequiresKey, std::move(requires));
    specs->Set(it.first, std::move(spec));
  }
  value->Set(Store::kInterfaceProviderSpecsKey, std::move(specs));
  return value;
}

// static
std::unique_ptr<Entry> Entry::Deserialize(const base::DictionaryValue& value) {
  auto entry = base::MakeUnique<Entry>();

  // Name.
  std::string name;
  if (!value.GetString(Store::kNameKey, &name)) {
    LOG(ERROR) << "Entry::Deserialize: dictionary has no "
               << Store::kNameKey << " key";
    return nullptr;
  }
  if (name.empty()) {
    LOG(ERROR) << "Entry::Deserialize: empty service name.";
    return nullptr;
  }
  entry->set_name(name);

  // Human-readable name.
  std::string display_name;
  if (!value.GetString(Store::kDisplayNameKey, &display_name)) {
    LOG(ERROR) << "Entry::Deserialize: dictionary has no "
               << Store::kDisplayNameKey << " key";
    return nullptr;
  }
  entry->set_display_name(display_name);

  // InterfaceProvider specs.
  const base::DictionaryValue* interface_provider_specs = nullptr;
  if (!value.GetDictionary(Store::kInterfaceProviderSpecsKey,
                           &interface_provider_specs)) {
    LOG(ERROR) << "Entry::Deserialize: dictionary has no "
               << Store::kInterfaceProviderSpecsKey << " key";
    return nullptr;
  }

  base::DictionaryValue::Iterator it(*interface_provider_specs);
  for (; !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* spec_value = nullptr;
    if (!interface_provider_specs->GetDictionary(it.key(), &spec_value)) {
      LOG(ERROR) << "Entry::Deserialize: value of InterfaceProvider map for "
                 << "key: " << it.key() << " not a dictionary.";
      return nullptr;
    }

    service_manager::InterfaceProviderSpec spec;
    if (!BuildInterfaceProviderSpec(*spec_value, &spec)) {
      LOG(ERROR) << "Entry::Deserialize: failed to build InterfaceProvider "
                 << "spec for key: " << it.key();
      return nullptr;
    }
    entry->AddInterfaceProviderSpec(it.key(), spec);
  }

  if (value.HasKey(Store::kServicesKey)) {
    const base::ListValue* services = nullptr;
    value.GetList(Store::kServicesKey, &services);
    for (size_t i = 0; i < services->GetSize(); ++i) {
      const base::DictionaryValue* service = nullptr;
      services->GetDictionary(i, &service);
      std::unique_ptr<Entry> child = Entry::Deserialize(*service);
      if (child) {
        child->set_package(entry.get());
        // Caller must assume ownership of these items.
        entry->children_.emplace_back(std::move(child));
      }
    }
  }

  return entry;
}

bool Entry::ProvidesCapability(const std::string& capability) const {
  auto it = interface_provider_specs_.find(
      service_manager::mojom::kServiceManager_ConnectorSpec);
  if (it == interface_provider_specs_.end())
    return false;

  auto connection_spec = it->second;
  return connection_spec.provides.find(capability) !=
      connection_spec.provides.end();
}

bool Entry::operator==(const Entry& other) const {
  return other.name_ == name_ &&
         other.display_name_ == display_name_ &&
         other.interface_provider_specs_ == interface_provider_specs_;
}

void Entry::AddInterfaceProviderSpec(
    const std::string& name,
    const service_manager::InterfaceProviderSpec& spec) {
  interface_provider_specs_[name] = spec;
}

}  // catalog

namespace mojo {

// static
service_manager::mojom::ResolveResultPtr
TypeConverter<service_manager::mojom::ResolveResultPtr,
              catalog::Entry>::Convert(const catalog::Entry& input) {
  service_manager::mojom::ResolveResultPtr result(
      service_manager::mojom::ResolveResult::New());
  result->name = input.name();
  const catalog::Entry& package = input.package() ? *input.package() : input;
  result->resolved_name = package.name();
  result->interface_provider_specs = input.interface_provider_specs();
  if (input.package()) {
    auto it = package.interface_provider_specs().find(
        service_manager::mojom::kServiceManager_ConnectorSpec);
    if (it != package.interface_provider_specs().end())
      result->package_spec = it->second;
  }
  result->package_path = package.path();
  return result;
}

// static
catalog::mojom::EntryPtr
    TypeConverter<catalog::mojom::EntryPtr, catalog::Entry>::Convert(
        const catalog::Entry& input) {
  catalog::mojom::EntryPtr result(catalog::mojom::Entry::New());
  result->name = input.name();
  result->display_name = input.display_name();
  return result;
}

}  // namespace mojo
