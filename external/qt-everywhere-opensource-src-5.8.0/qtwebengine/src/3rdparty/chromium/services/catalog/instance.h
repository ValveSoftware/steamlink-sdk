// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_INSTANCE_H_
#define SERVICES_CATALOG_INSTANCE_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/catalog/entry.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/catalog/store.h"
#include "services/catalog/types.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/interfaces/shell_resolver.mojom.h"

namespace catalog {

class Reader;
class Store;

class Instance : public shell::mojom::ShellResolver,
                 public mojom::Catalog {
 public:
  // |manifest_provider| may be null.
  Instance(std::unique_ptr<Store> store, Reader* system_reader);
  ~Instance() override;

  void BindShellResolver(shell::mojom::ShellResolverRequest request);
  void BindCatalog(mojom::CatalogRequest request);

  // Called when |cache| has been populated by a directory scan.
  void CacheReady(EntryCache* cache);

 private:
  // shell::mojom::ShellResolver:
  void ResolveMojoName(const mojo::String& mojo_name,
                       const ResolveMojoNameCallback& callback) override;

  // mojom::Catalog:
  void GetEntries(mojo::Array<mojo::String> names,
                  const GetEntriesCallback& callback) override;
  void GetEntriesProvidingClass(
      const mojo::String& clazz,
      const GetEntriesProvidingClassCallback& callback) override;
  void GetEntriesConsumingMIMEType(
      const mojo::String& mime_type,
      const GetEntriesConsumingMIMETypeCallback& callback) override;
  void GetEntriesSupportingScheme(
      const mojo::String& scheme,
      const GetEntriesSupportingSchemeCallback& callback) override;

  // Populate/serialize the cache from/to the supplied store.
  void DeserializeCatalog();
  void SerializeCatalog();

  // Receives the result of manifest parsing, may be received after the
  // catalog object that issued the request is destroyed.
  static void OnReadManifest(base::WeakPtr<Instance> instance,
                             const std::string& mojo_name,
                             const ResolveMojoNameCallback& callback,
                             shell::mojom::ResolveResultPtr result);

  // User-specific persistent storage of package manifests and other settings.
  std::unique_ptr<Store> store_;

  mojo::BindingSet<shell::mojom::ShellResolver> shell_resolver_bindings_;
  mojo::BindingSet<mojom::Catalog> catalog_bindings_;

  Reader* system_reader_;

  // A map of name -> Entry data structure for system-level packages (i.e. those
  // that are visible to all users).
  // TODO(beng): eventually add per-user applications.
  EntryCache* system_cache_ = nullptr;

  // We only bind requests for these interfaces once the catalog has been
  // populated. These data structures queue requests until that happens.
  std::vector<shell::mojom::ShellResolverRequest>
      pending_shell_resolver_requests_;
  std::vector<mojom::CatalogRequest> pending_catalog_requests_;

  base::WeakPtrFactory<Instance> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Instance);
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_INSTANCE_H_
