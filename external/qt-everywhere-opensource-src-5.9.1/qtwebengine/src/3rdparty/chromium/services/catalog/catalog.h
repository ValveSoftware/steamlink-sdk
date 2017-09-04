// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_CATALOG_H_
#define SERVICES_CATALOG_CATALOG_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/catalog/types.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/interfaces/resolver.mojom.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace base {
class SequencedWorkerPool;
class SingleThreadTaskRunner;
}

namespace filesystem {
class LockTable;
}

namespace service_manager {
class ServiceContext;
}

namespace catalog {

class Instance;
class ManifestProvider;
class Reader;
class Store;

// Creates and owns an instance of the catalog. Exposes a ServicePtr that
// can be passed to the service manager, potentially in a different process.
class Catalog
    : public service_manager::InterfaceFactory<mojom::Catalog>,
      public service_manager::InterfaceFactory<filesystem::mojom::Directory>,
      public service_manager::InterfaceFactory<
          service_manager::mojom::Resolver>,
      public service_manager::InterfaceFactory<mojom::CatalogControl>,
      public mojom::CatalogControl {
 public:
  // |manifest_provider| may be null.
  Catalog(base::SequencedWorkerPool* worker_pool,
          std::unique_ptr<Store> store,
          ManifestProvider* manifest_provider);
  Catalog(base::SingleThreadTaskRunner* task_runner,
          std::unique_ptr<Store> store,
          ManifestProvider* manifest_provider);
  ~Catalog() override;

  // By default, "foo" resolves to a package named "foo". This allows
  // an embedder to override that behavior for specific service names. Must be
  // called before the catalog is connected to the ServiceManager.
  void OverridePackageName(const std::string& service_name,
                           const std::string& package_name);

  service_manager::mojom::ServicePtr TakeService();

 private:
  class ServiceImpl;

  explicit Catalog(std::unique_ptr<Store> store);

  // Starts a scane for system packages.
  void ScanSystemPackageDir();

  // service_manager::InterfaceFactory<service_manager::mojom::Resolver>:
  void Create(const service_manager::Identity& remote_identity,
              service_manager::mojom::ResolverRequest request) override;

  // service_manager::InterfaceFactory<mojom::Catalog>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::CatalogRequest request) override;

  // service_manager::InterfaceFactory<filesystem::mojom::Directory>:
  void Create(const service_manager::Identity& remote_identity,
              filesystem::mojom::DirectoryRequest request) override;

  // service_manager::InterfaceFactory<mojom::CatalogControl>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::CatalogControlRequest request) override;

  // mojom::CatalogControl:
  void OverrideManifestPath(
      const std::string& service_name,
      const base::FilePath& path,
      const OverrideManifestPathCallback& callback) override;

  Instance* GetInstanceForUserId(const std::string& user_id);

  void SystemPackageDirScanned();

  std::unique_ptr<Store> store_;

  service_manager::mojom::ServicePtr service_;
  std::unique_ptr<service_manager::ServiceContext> service_context_;

  std::map<std::string, std::unique_ptr<Instance>> instances_;

  std::unique_ptr<Reader> system_reader_;
  EntryCache system_cache_;
  bool loaded_ = false;

  scoped_refptr<filesystem::LockTable> lock_table_;

  mojo::BindingSet<mojom::CatalogControl> control_bindings_;

  base::WeakPtrFactory<Catalog> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Catalog);
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_CATALOG_H_
