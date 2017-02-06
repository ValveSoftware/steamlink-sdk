// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/file_system_app.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"

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

void FileSystemApp::Initialize(shell::Connector* connector,
                               const shell::Identity& identity,
                               uint32_t id) {
  tracing_.Initialize(connector, identity.name());
}

bool FileSystemApp::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mojom::FileSystem>(this);
  return true;
}

// |InterfaceFactory<Files>| implementation:
void FileSystemApp::Create(shell::Connection* connection,
                           mojo::InterfaceRequest<mojom::FileSystem> request) {
  new FileSystemImpl(connection, std::move(request), GetUserDataDir(),
                     lock_table_);
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
