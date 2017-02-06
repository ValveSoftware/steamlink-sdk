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
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/catalog/types.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"
#include "services/shell/public/interfaces/shell_resolver.mojom.h"

namespace base {
class SequencedWorkerPool;
class SingleThreadTaskRunner;
}

namespace filesystem {
class LockTable;
}

namespace shell {
class ShellConnection;
}

namespace catalog {

class Instance;
class ManifestProvider;
class Reader;
class Store;

// Creates and owns an instance of the catalog. Exposes a ShellClientPtr that
// can be passed to the Shell, potentially in a different process.
class Catalog : public shell::ShellClient,
                public shell::InterfaceFactory<mojom::Catalog>,
                public shell::InterfaceFactory<filesystem::mojom::Directory>,
                public shell::InterfaceFactory<shell::mojom::ShellResolver> {
 public:
  // |manifest_provider| may be null.
  Catalog(base::SequencedWorkerPool* worker_pool,
          std::unique_ptr<Store> store,
          ManifestProvider* manifest_provider);
  Catalog(base::SingleThreadTaskRunner* task_runner,
          std::unique_ptr<Store> store,
          ManifestProvider* manifest_provider);
  ~Catalog() override;

  shell::mojom::ShellClientPtr TakeShellClient();

 private:
  explicit Catalog(std::unique_ptr<Store> store);

  // Starts a scane for system packages.
  void ScanSystemPackageDir();

  // shell::ShellClient:
  bool AcceptConnection(shell::Connection* connection) override;

  // shell::InterfaceFactory<shell::mojom::ShellResolver>:
  void Create(shell::Connection* connection,
              shell::mojom::ShellResolverRequest request) override;

  // shell::InterfaceFactory<mojom::Catalog>:
  void Create(shell::Connection* connection,
              mojom::CatalogRequest request) override;

  // shell::InterfaceFactory<filesystem::mojom::Directory>:
  void Create(shell::Connection* connection,
              filesystem::mojom::DirectoryRequest request) override;

  Instance* GetInstanceForUserId(const std::string& user_id);

  void SystemPackageDirScanned();

  std::unique_ptr<Store> store_;

  shell::mojom::ShellClientPtr shell_client_;
  std::unique_ptr<shell::ShellConnection> shell_connection_;

  std::map<std::string, std::unique_ptr<Instance>> instances_;

  std::unique_ptr<Reader> system_reader_;
  EntryCache system_cache_;
  bool loaded_ = false;

  scoped_refptr<filesystem::LockTable> lock_table_;

  base::WeakPtrFactory<Catalog> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Catalog);
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_CATALOG_H_
