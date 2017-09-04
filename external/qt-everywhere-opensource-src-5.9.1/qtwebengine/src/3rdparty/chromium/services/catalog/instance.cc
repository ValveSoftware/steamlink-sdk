// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/instance.h"

#include "base/bind.h"
#include "services/catalog/entry.h"
#include "services/catalog/manifest_provider.h"
#include "services/catalog/reader.h"
#include "services/catalog/store.h"

namespace catalog {
namespace {

void AddEntry(const Entry& entry, std::vector<mojom::EntryPtr>* ary) {
  mojom::EntryPtr entry_ptr(mojom::Entry::New());
  entry_ptr->name = entry.name();
  entry_ptr->display_name = entry.display_name();
  ary->push_back(std::move(entry_ptr));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Instance, public:

Instance::Instance(std::unique_ptr<Store> store, Reader* system_reader)
    : store_(std::move(store)),
      system_reader_(system_reader),
      weak_factory_(this) {}
Instance::~Instance() {}

void Instance::BindResolver(service_manager::mojom::ResolverRequest request) {
  if (system_cache_)
    resolver_bindings_.AddBinding(this, std::move(request));
  else
    pending_resolver_requests_.push_back(std::move(request));
}

void Instance::BindCatalog(mojom::CatalogRequest request) {
  if (system_cache_)
    catalog_bindings_.AddBinding(this, std::move(request));
  else
    pending_catalog_requests_.push_back(std::move(request));
}

void Instance::CacheReady(EntryCache* cache) {
  system_cache_ = cache;
  DeserializeCatalog();
  for (auto& request : pending_resolver_requests_)
    BindResolver(std::move(request));
  for (auto& request : pending_catalog_requests_)
    BindCatalog(std::move(request));
}

////////////////////////////////////////////////////////////////////////////////
// Instance, service_manager::mojom::Resolver:

void Instance::ResolveMojoName(const std::string& service_name,
                               const ResolveMojoNameCallback& callback) {
  DCHECK(system_cache_);

  // TODO(beng): per-user catalogs.
  auto entry = system_cache_->find(service_name);
  if (entry != system_cache_->end()) {
    callback.Run(service_manager::mojom::ResolveResult::From(*entry->second));
    return;
  }

  // Manifests for mojo: names should always be in the catalog by this point.
  // DCHECK(type == service_manager::kNameType_Exe);
  system_reader_->CreateEntryForName(
      service_name, system_cache_,
      base::Bind(&Instance::OnReadManifest, weak_factory_.GetWeakPtr(),
                 service_name, callback));
}

////////////////////////////////////////////////////////////////////////////////
// Instance, mojom::Catalog:

void Instance::GetEntries(const base::Optional<std::vector<std::string>>& names,
                          const GetEntriesCallback& callback) {
  DCHECK(system_cache_);

  std::vector<mojom::EntryPtr> entries;
  if (!names.has_value()) {
    // TODO(beng): user catalog.
    for (const auto& entry : *system_cache_)
      AddEntry(*entry.second, &entries);
  } else {
    for (const std::string& name : names.value()) {
      Entry* entry = nullptr;
      // TODO(beng): user catalog.
      if (system_cache_->find(name) != system_cache_->end())
        entry = (*system_cache_)[name].get();
      else
        continue;
      AddEntry(*entry, &entries);
    }
  }
  callback.Run(std::move(entries));
}

void Instance::GetEntriesProvidingCapability(
    const std::string& capability,
    const GetEntriesProvidingCapabilityCallback& callback) {
  std::vector<mojom::EntryPtr> entries;
  for (const auto& entry : *system_cache_)
    if (entry.second->ProvidesCapability(capability))
      entries.push_back(mojom::Entry::From(*entry.second));
  callback.Run(std::move(entries));
}

void Instance::GetEntriesConsumingMIMEType(
    const std::string& mime_type,
    const GetEntriesConsumingMIMETypeCallback& callback) {
  // TODO(beng): implement.
}

void Instance::GetEntriesSupportingScheme(
    const std::string& scheme,
    const GetEntriesSupportingSchemeCallback& callback) {
  // TODO(beng): implement.
}

////////////////////////////////////////////////////////////////////////////////
// Instance, private:

void Instance::DeserializeCatalog() {
  DCHECK(system_cache_);
  if (!store_)
    return;
  const base::ListValue* catalog = store_->GetStore();
  CHECK(catalog);
  // TODO(sky): make this handle aliases.
  // TODO(beng): implement this properly!
  for (const auto& v : *catalog) {
    const base::DictionaryValue* dictionary = nullptr;
    CHECK(v->GetAsDictionary(&dictionary));
    std::unique_ptr<Entry> entry = Entry::Deserialize(*dictionary);
    // TODO(beng): user catalog.
    if (entry)
      (*system_cache_)[entry->name()] = std::move(entry);
  }
}

void Instance::SerializeCatalog() {
  DCHECK(system_cache_);
  std::unique_ptr<base::ListValue> catalog(new base::ListValue);
  // TODO(beng): user catalog.
  for (const auto& entry : *system_cache_)
    catalog->Append(entry.second->Serialize());
  if (store_)
    store_->UpdateStore(std::move(catalog));
}

// static
void Instance::OnReadManifest(base::WeakPtr<Instance> instance,
                              const std::string& service_name,
                              const ResolveMojoNameCallback& callback,
                              service_manager::mojom::ResolveResultPtr result) {
  callback.Run(std::move(result));
  if (instance)
    instance->SerializeCatalog();
}

}  // namespace catalog
