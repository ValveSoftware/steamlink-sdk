// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_READER_H_
#define SERVICES_CATALOG_READER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/catalog/types.h"
#include "services/service_manager/public/interfaces/resolver.mojom.h"

namespace base {
class SequencedWorkerPool;
class SingleThreadTaskRunner;
}

namespace catalog {

class Entry;
class ManifestProvider;

// Responsible for loading manifests & building the Entry data structures.
class Reader {
 public:
  using ReadManifestCallback = base::Callback<void(std::unique_ptr<Entry>)>;
  using CreateEntryForNameCallback =
      base::Callback<void(service_manager::mojom::ResolveResultPtr)>;

  Reader(base::SequencedWorkerPool* worker_pool,
         ManifestProvider* manifest_provider);
  Reader(base::SingleThreadTaskRunner* task_runner,
         ManifestProvider* manifest_provider);
  ~Reader();

  // Scans the contents of |package_dir|, reading all application manifests and
  // populating |cache|. Runs |read_complete_closure| when done.
  void Read(const base::FilePath& package_dir,
            EntryCache* cache,
            const base::Closure& read_complete_closure);

  // Returns an Entry for |mojo_name| via |callback|, assuming a manifest file
  // in the canonical location
  void CreateEntryForName(
      const std::string& mojo_name,
      EntryCache* cache,
      const CreateEntryForNameCallback& entry_created_callback);

  // Overrides the package name used for a specific service name.
  void OverridePackageName(const std::string& service_name,
                           const std::string& package_name);

  // Overrides the manifest path used for a specific service name.
  void OverrideManifestPath(const std::string& service_name,
                            const base::FilePath& path);

 private:
  explicit Reader(ManifestProvider* manifest_provider);

  void OnReadManifest(EntryCache* cache,
                      const CreateEntryForNameCallback& entry_created_callback,
                      std::unique_ptr<Entry> entry);

  base::FilePath system_package_dir_;
  scoped_refptr<base::TaskRunner> file_task_runner_;
  ManifestProvider* const manifest_provider_;
  std::map<std::string, std::string> package_name_overrides_;
  std::map<std::string, base::FilePath> manifest_path_overrides_;
  base::WeakPtrFactory<Reader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Reader);
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_READER_H_
