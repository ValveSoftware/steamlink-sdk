// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/file_system_app.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#elif defined(OS_ANDROID)
#include "base/base_paths_android.h"
#include "base/path_service.h"
#elif defined(OS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#elif defined(OS_MACOSX)
#include "base/base_paths_mac.h"
#include "base/path_service.h"
#endif

namespace filesystem {

namespace {

const char kUserDataDir[] = "user-data-dir";

}  // namespace filesystem

FileSystemApp::FileSystemApp() : lock_table_(new LockTable) {}

FileSystemApp::~FileSystemApp() {}

void FileSystemApp::OnStart() {
  tracing_.Initialize(context()->connector(), context()->identity().name());
}

bool FileSystemApp::OnConnect(const service_manager::ServiceInfo& remote_info,
                              service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::FileSystem>(this);
  return true;
}

// |InterfaceFactory<Files>| implementation:
void FileSystemApp::Create(const service_manager::Identity& remote_identity,
                           mojom::FileSystemRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<FileSystemImpl>(
                              remote_identity, GetUserDataDir(), lock_table_),
                          std::move(request));
}

//static
base::FilePath FileSystemApp::GetUserDataDir() {
  base::FilePath path;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUserDataDir)) {
    path = command_line->GetSwitchValuePath(kUserDataDir);
  } else {
#if defined(OS_WIN)
    CHECK(PathService::Get(base::DIR_LOCAL_APP_DATA, &path));
#elif defined(OS_MACOSX)
    CHECK(PathService::Get(base::DIR_APP_DATA, &path));
#elif defined(OS_ANDROID)
    CHECK(PathService::Get(base::DIR_ANDROID_APP_DATA, &path));
#elif defined(OS_LINUX)
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    path = base::nix::GetXDGDirectory(env.get(),
                                      base::nix::kXdgConfigHomeEnvVar,
                                      base::nix::kDotConfigDir);
#else
    NOTIMPLEMENTED();
#endif
    path = path.Append(FILE_PATH_LITERAL("filesystem"));
  }

  if (!base::PathExists(path))
    base::CreateDirectory(path);

  return path;
}

}  // namespace filesystem
