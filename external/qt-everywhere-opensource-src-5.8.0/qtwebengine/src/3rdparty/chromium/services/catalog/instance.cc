// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/instance.h"

#include "base/bind.h"
#include "services/catalog/entry.h"
#include "services/catalog/manifest_provider.h"
#include "services/catalog/reader.h"
#include "services/catalog/store.h"
#include "services/shell/public/cpp/names.h"

namespace catalog {
namespace {

void AddEntry(const Entry& entry, mojo::Array<mojom::EntryPtr>* ary) {
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

void Instance::BindShellResolver(
    shell::mojom::ShellResolverRequest request) {
  if (system_cache_)
    shell_resolver_bindings_.AddBinding(this, std::move(request));
  else
    pending_shell_resolver_requests_.push_back(std::move(request));
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
  for (auto& request : pending_shell_resolver_requests_)
    BindShellResolver(std::move(request));
  for (auto& request : pending_catalog_requests_)
    BindCatalog(std::move(request));
}

////////////////////////////////////////////////////////////////////////////////
// Instance, shell::mojom::ShellResolver:

void Instance::ResolveMojoName(const mojo::String& mojo_name,
                               const ResolveMojoNameCallback& callback) {
  DCHECK(system_cache_);

  std::string type = shell::GetNameType(mojo_name);
  if (type != shell::kNameType_Mojo && type != shell::kNameType_Exe) {
    std::unique_ptr<Entry> entry(new Entry(mojo_name));
    callback.Run(shell::mojom::ResolveResult::From(*entry));
    return;
  }

  // TODO(beng): per-user catalogs.
  auto entry = system_cache_->find(mojo_name);
  if (entry != system_cache_->end()) {
    callback.Run(shell::mojom::ResolveResult::From(*entry->second));
    return;
  }

  // Manifests for mojo: names should always be in the catalog by this point.
  //DCHECK(type == shell::kNameType_Exe);
  system_reader_->CreateEntryForName(
      mojo_name, system_cache_,
      base::Bind(&Instance::OnReadManifest, weak_factory_.GetWeakPtr(),
                 mojo_name, callback));
}

////////////////////////////////////////////////////////////////////////////////
// Instance, mojom::Catalog:

void Instance::GetEntries(mojo::Array<mojo::String> names,
                          const GetEntriesCallback& callback) {
  DCHECK(system_cache_);

  mojo::Array<mojom::EntryPtr> entries;
  if (names.is_null()) {
    // TODO(beng): user catalog.
    for (const auto& entry : *system_cache_)
      AddEntry(*entry.second, &entries);
  } else {
    std::vector<mojo::String> names_vec = names.PassStorage();
    for (const std::string& name : names_vec) {
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

void Instance::GetEntriesProvidingClass(
    const mojo::String& clazz,
    const GetEntriesProvidingClassCallback& callback) {
  mojo::Array<mojom::EntryPtr> entries;
  for (const auto& entry : *system_cache_)
    if (entry.second->ProvidesClass(clazz))
      entries.push_back(mojom::Entry::From(*entry.second));
  callback.Run(std::move(entries));
}

void Instance::GetEntriesConsumingMIMEType(
    const mojo::String& mime_type,
    const GetEntriesConsumingMIMETypeCallback& callback) {
  // TODO(beng): implement.
}

void Instance::GetEntriesSupportingScheme(
    const mojo::String& scheme,
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
                              const std::string& mojo_name,
                              const ResolveMojoNameCallback& callback,
                              shell::mojom::ResolveResultPtr result) {
  callback.Run(std::move(result));
  if (instance)
    instance->SerializeCatalog();
}

}  // namespace catalog
