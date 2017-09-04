// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/catalog.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/filesystem/directory_impl.h"
#include "components/filesystem/lock_table.h"
#include "components/filesystem/public/interfaces/types.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/catalog/constants.h"
#include "services/catalog/instance.h"
#include "services/catalog/reader.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace catalog {
namespace {

bool IsPathNameValid(const std::string& name) {
  if (name.empty() || name == "." || name == "..")
    return false;

  for (auto c : name) {
    if (!base::IsAsciiAlpha(c) && !base::IsAsciiDigit(c) &&
        c != '_' && c != '.')
      return false;
  }
  return true;
}

base::FilePath GetPathForApplicationName(const std::string& application_name) {
  static const char kServicePrefix[] = "";
  std::string path = application_name;
  const bool is_service = base::StartsWith(
      path, kServicePrefix, base::CompareCase::INSENSITIVE_ASCII);
  if (!is_service)
    return base::FilePath();
  if (path.find('.') != std::string::npos)
    return base::FilePath();
  path.erase(path.begin(), path.begin() + strlen(kServicePrefix));
  base::TrimString(path, "/", &path);
  size_t end_of_name = path.find('/');
  if (end_of_name != std::string::npos)
    path.erase(path.begin() + end_of_name, path.end());

  if (!IsPathNameValid(path))
    return base::FilePath();

  base::FilePath base_path;
  PathService::Get(base::DIR_EXE, &base_path);
  // TODO(beng): this won't handle user-specific components.
  return base_path.AppendASCII(kPackagesDirName).AppendASCII(path).
      AppendASCII("resources");
}

}  // namespace

class Catalog::ServiceImpl : public service_manager::Service {
 public:
  explicit ServiceImpl(Catalog* catalog) : catalog_(catalog) {}
  ~ServiceImpl() override {}

  // service_manager::Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<mojom::Catalog>(catalog_);
    registry->AddInterface<mojom::CatalogControl>(catalog_);
    registry->AddInterface<filesystem::mojom::Directory>(catalog_);
    registry->AddInterface<service_manager::mojom::Resolver>(catalog_);
    return true;
  }

 private:
  Catalog* const catalog_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

Catalog::Catalog(base::SequencedWorkerPool* worker_pool,
                 std::unique_ptr<Store> store,
                 ManifestProvider* manifest_provider)
    : Catalog(std::move(store)) {
  system_reader_.reset(new Reader(worker_pool, manifest_provider));
  ScanSystemPackageDir();
}

Catalog::Catalog(base::SingleThreadTaskRunner* task_runner,
                 std::unique_ptr<Store> store,
                 ManifestProvider* manifest_provider)
    : Catalog(std::move(store)) {
  system_reader_.reset(new Reader(task_runner, manifest_provider));
  ScanSystemPackageDir();
}

Catalog::~Catalog() {}

void Catalog::OverridePackageName(const std::string& service_name,
                                  const std::string& package_name) {
  system_reader_->OverridePackageName(service_name, package_name);
}

service_manager::mojom::ServicePtr Catalog::TakeService() {
  return std::move(service_);
}

Catalog::Catalog(std::unique_ptr<Store> store)
    : store_(std::move(store)), weak_factory_(this) {
  service_manager::mojom::ServiceRequest request = GetProxy(&service_);
  service_context_.reset(new service_manager::ServiceContext(
      base::MakeUnique<ServiceImpl>(this), std::move(request)));
}

void Catalog::ScanSystemPackageDir() {
  base::FilePath system_package_dir;
  PathService::Get(base::DIR_MODULE, &system_package_dir);
  system_package_dir = system_package_dir.AppendASCII(kPackagesDirName);
  system_reader_->Read(system_package_dir, &system_cache_,
                       base::Bind(&Catalog::SystemPackageDirScanned,
                                  weak_factory_.GetWeakPtr()));
}

void Catalog::Create(const service_manager::Identity& remote_identity,
                     service_manager::mojom::ResolverRequest request) {
  Instance* instance = GetInstanceForUserId(remote_identity.user_id());
  instance->BindResolver(std::move(request));
}

void Catalog::Create(const service_manager::Identity& remote_identity,
                     mojom::CatalogRequest request) {
  Instance* instance = GetInstanceForUserId(remote_identity.user_id());
  instance->BindCatalog(std::move(request));
}

void Catalog::Create(const service_manager::Identity& remote_identity,
                     filesystem::mojom::DirectoryRequest request) {
  if (!lock_table_)
    lock_table_ = new filesystem::LockTable;
  base::FilePath resources_path =
      GetPathForApplicationName(remote_identity.name());
  mojo::MakeStrongBinding(
      base::MakeUnique<filesystem::DirectoryImpl>(
          resources_path, scoped_refptr<filesystem::SharedTempDir>(),
          lock_table_),
      std::move(request));
}

void Catalog::Create(const service_manager::Identity& remote_identity,
                     mojom::CatalogControlRequest request) {
  control_bindings_.AddBinding(this, std::move(request));
}

void Catalog::OverrideManifestPath(
    const std::string& service_name,
    const base::FilePath& path,
    const OverrideManifestPathCallback& callback) {
  system_reader_->OverrideManifestPath(service_name, path);
  callback.Run();
}

Instance* Catalog::GetInstanceForUserId(const std::string& user_id) {
  auto it = instances_.find(user_id);
  if (it != instances_.end())
    return it->second.get();

  // TODO(beng): There needs to be a way to load the store from different users.
  Instance* instance = new Instance(std::move(store_), system_reader_.get());
  instances_[user_id] = base::WrapUnique(instance);
  if (loaded_)
    instance->CacheReady(&system_cache_);

  return instance;
}

void Catalog::SystemPackageDirScanned() {
  loaded_ = true;
  for (auto& instance : instances_)
    instance.second->CacheReady(&system_cache_);
}

}  // namespace catalog
