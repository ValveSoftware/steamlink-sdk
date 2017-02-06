// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/reader.h"

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/catalog/constants.h"
#include "services/catalog/entry.h"
#include "services/catalog/manifest_provider.h"
#include "services/shell/public/cpp/names.h"

namespace catalog {
namespace {

base::FilePath GetManifestPath(const base::FilePath& package_dir,
                               const std::string& name) {
  // TODO(beng): think more about how this should be done for exe targets.
  std::string type = shell::GetNameType(name);
  std::string path = shell::GetNamePath(name);
  if (type == shell::kNameType_Mojo) {
    return package_dir.AppendASCII(kMojoApplicationsDirName).AppendASCII(
        path + "/manifest.json");
  }
  if (type == shell::kNameType_Exe)
    return package_dir.AppendASCII(path + "_manifest.json");
  return base::FilePath();
}


base::FilePath GetPackagePath(const base::FilePath& package_dir,
                              const std::string& name) {
  std::string type = shell::GetNameType(name);
  if (type == shell::kNameType_Mojo) {
    // It's still a mojo: URL, use the default mapping scheme.
    const std::string host = shell::GetNamePath(name);
    return package_dir.AppendASCII(host + "/" + host + ".mojo");
  }
  if (type == shell::kNameType_Exe) {
#if defined OS_WIN
    std::string extension = ".exe";
#else
    std::string extension;
#endif
    return package_dir.AppendASCII(shell::GetNamePath(name) + extension);
  }
  return base::FilePath();
}

std::unique_ptr<Entry> ProcessManifest(
    std::unique_ptr<base::Value> manifest_root,
    const base::FilePath& package_dir) {
  // Manifest was malformed or did not exist.
  if (!manifest_root)
    return nullptr;

  const base::DictionaryValue* dictionary = nullptr;
  if (!manifest_root->GetAsDictionary(&dictionary))
    return nullptr;

  std::unique_ptr<Entry> entry = Entry::Deserialize(*dictionary);
  if (!entry)
    return nullptr;
  entry->set_path(GetPackagePath(package_dir, entry->name()));
  return entry;
}

std::unique_ptr<Entry> CreateEntryForManifestAt(
    const base::FilePath& manifest_path,
    const base::FilePath& package_dir) {
  JSONFileValueDeserializer deserializer(manifest_path);
  int error = 0;
  std::string message;

  // TODO(beng): probably want to do more detailed error checking. This should
  //             be done when figuring out if to unblock connection completion.
  return ProcessManifest(deserializer.Deserialize(&error, &message),
                         package_dir);
}

void ScanDir(
    const base::FilePath& package_dir,
    const Reader::ReadManifestCallback& read_manifest_callback,
    scoped_refptr<base::SingleThreadTaskRunner> original_thread_task_runner,
    const base::Closure& read_complete_closure) {
  base::FileEnumerator enumerator(package_dir, false,
                                  base::FileEnumerator::DIRECTORIES);
  while (1) {
    base::FilePath path = enumerator.Next();
    if (path.empty())
      break;
    base::FilePath manifest_path = path.AppendASCII("manifest.json");
    std::unique_ptr<Entry> entry =
        CreateEntryForManifestAt(manifest_path, package_dir);
    if (!entry)
      continue;

    // Skip over subdirs that contain only manifests, they're artifacts of the
    // build (e.g. for applications that are packaged into others) and are not
    // valid standalone packages.
    base::FilePath package_path = GetPackagePath(package_dir, entry->name());
    if (entry->name() != "mojo:shell" && entry->name() != "mojo:catalog" &&
        !base::PathExists(package_path)) {
      continue;
    }

    original_thread_task_runner->PostTask(
        FROM_HERE,
        base::Bind(read_manifest_callback, base::Passed(&entry)));
  }

  original_thread_task_runner->PostTask(FROM_HERE, read_complete_closure);
}

std::unique_ptr<Entry> ReadManifest(const base::FilePath& package_dir,
                                    const std::string& mojo_name) {
  std::unique_ptr<Entry> entry = CreateEntryForManifestAt(
      GetManifestPath(package_dir, mojo_name), package_dir);
  if (!entry) {
    entry.reset(new Entry(mojo_name));
    entry->set_path(GetPackagePath(
        package_dir.AppendASCII(kMojoApplicationsDirName), mojo_name));
  }
  return entry;
}

void AddEntryToCache(EntryCache* cache, std::unique_ptr<Entry> entry) {
  for (auto child : entry->applications())
    AddEntryToCache(cache, base::WrapUnique(child));
  (*cache)[entry->name()] = std::move(entry);
}

void DoNothing(shell::mojom::ResolveResultPtr) {}

}  // namespace

// A sequenced task runner is used to guarantee requests are serviced in the
// order requested. To do otherwise means we may run callbacks in an
// unpredictable order, leading to flake.
Reader::Reader(base::SequencedWorkerPool* worker_pool,
               ManifestProvider* manifest_provider)
    : Reader(manifest_provider) {
  file_task_runner_ = worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
      base::SequencedWorkerPool::GetSequenceToken(),
      base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

Reader::Reader(base::SingleThreadTaskRunner* task_runner,
               ManifestProvider* manifest_provider)
    : Reader(manifest_provider) {
  file_task_runner_ = task_runner;
}

Reader::~Reader() {}

void Reader::Read(const base::FilePath& package_dir,
                  EntryCache* cache,
                  const base::Closure& read_complete_closure) {
  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ScanDir, package_dir,
                 base::Bind(&Reader::OnReadManifest, weak_factory_.GetWeakPtr(),
                            cache, base::Bind(&DoNothing)),
                 base::ThreadTaskRunnerHandle::Get(),
                 read_complete_closure));
}

void Reader::CreateEntryForName(
    const std::string& mojo_name,
    EntryCache* cache,
    const CreateEntryForNameCallback& entry_created_callback) {
  std::string manifest_contents;
  if (manifest_provider_ &&
      manifest_provider_->GetApplicationManifest(mojo_name,
                                                 &manifest_contents)) {
    std::unique_ptr<base::Value> manifest_root =
        base::JSONReader::Read(manifest_contents);
    base::PostTaskAndReplyWithResult(
        file_task_runner_.get(), FROM_HERE,
        base::Bind(&ProcessManifest, base::Passed(&manifest_root),
                   system_package_dir_),
        base::Bind(&Reader::OnReadManifest, weak_factory_.GetWeakPtr(), cache,
                   entry_created_callback));
  } else {
    base::PostTaskAndReplyWithResult(
        file_task_runner_.get(), FROM_HERE,
        base::Bind(&ReadManifest, system_package_dir_, mojo_name),
        base::Bind(&Reader::OnReadManifest, weak_factory_.GetWeakPtr(), cache,
                   entry_created_callback));
  }
}

Reader::Reader(ManifestProvider* manifest_provider)
    : manifest_provider_(manifest_provider), weak_factory_(this) {
  PathService::Get(base::DIR_MODULE, &system_package_dir_);
}

void Reader::OnReadManifest(
    EntryCache* cache,
    const CreateEntryForNameCallback& entry_created_callback,
    std::unique_ptr<Entry> entry) {
  if (!entry)
    return;
  shell::mojom::ResolveResultPtr result =
      shell::mojom::ResolveResult::From(*entry);
  AddEntryToCache(cache, std::move(entry));
  entry_created_callback.Run(std::move(result));
}

}  // namespace catalog
