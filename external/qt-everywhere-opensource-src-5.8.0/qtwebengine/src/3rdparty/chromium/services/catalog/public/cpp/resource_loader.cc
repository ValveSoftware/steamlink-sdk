// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/public/cpp/resource_loader.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"

namespace catalog {

namespace {

base::File GetFileFromHandle(mojo::ScopedHandle handle) {
  CHECK(handle.is_valid());
  base::PlatformFile platform_file;
  CHECK_EQ(mojo::UnwrapPlatformFile(std::move(handle), &platform_file),
           MOJO_RESULT_OK);
  return base::File(platform_file);
}

}  // namespace

ResourceLoader::ResourceLoader() {}
ResourceLoader::~ResourceLoader() {}

bool ResourceLoader::OpenFiles(filesystem::mojom::DirectoryPtr directory,
                               const std::set<std::string>& paths) {
  mojo::Array<filesystem::mojom::FileOpenDetailsPtr> details(
      mojo::Array<filesystem::mojom::FileOpenDetailsPtr>::New(paths.size()));
  size_t i = 0;
  for (const auto& path : paths) {
    filesystem::mojom::FileOpenDetailsPtr open_details(
        filesystem::mojom::FileOpenDetails::New());
    open_details->path = path;
    open_details->open_flags =
        filesystem::mojom::kFlagOpen | filesystem::mojom::kFlagRead;
    details[i++] = std::move(open_details);
  }

  mojo::Array<filesystem::mojom::FileOpenResultPtr> results(
      mojo::Array<filesystem::mojom::FileOpenResultPtr>::New(paths.size()));
  if (!directory->OpenFileHandles(std::move(details), &results))
    return false;

  for (const auto& result : results) {
    resource_map_[result->path].reset(
        new base::File(GetFileFromHandle(std::move(result->file_handle))));
  }
  return true;
}

base::File ResourceLoader::TakeFile(const std::string& path) {
  std::unique_ptr<base::File> file_wrapper(std::move(resource_map_[path]));
  resource_map_.erase(path);
  return std::move(*file_wrapper);
}

}  // namespace catalog
