// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_test_base.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "mojo/shell/context.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {
namespace test {

ShellTestBase::ShellTestBase() {
}

ShellTestBase::~ShellTestBase() {
}

void ShellTestBase::InitMojo() {
  DCHECK(!message_loop_);
  DCHECK(!shell_context_);
  message_loop_.reset(new base::MessageLoop());
  shell_context_.reset(new Context());
}

void ShellTestBase::LaunchServiceInProcess(
    const GURL& service_url,
    const std::string& service_name,
    ScopedMessagePipeHandle client_handle) {
  DCHECK(message_loop_);
  DCHECK(shell_context_);

  base::FilePath base_dir;
  CHECK(PathService::Get(base::DIR_EXE, &base_dir));
  // On android, the library is bundled with the app.
#if defined(OS_ANDROID)
  base::FilePath service_dir;
  CHECK(PathService::Get(base::DIR_MODULE, &service_dir));
  // On Mac and Windows, libraries are dumped beside the executables.
#elif defined(OS_MACOSX) || defined(OS_WIN)
  base::FilePath service_dir(base_dir);
#else
  // On Linux, they're under lib/.
  base::FilePath service_dir(base_dir.AppendASCII("lib"));
#endif
  shell_context_->mojo_url_resolver()->set_origin(
      net::FilePathToFileURL(service_dir).spec());

  shell_context_->service_manager()->ConnectToService(
      service_url, service_name, client_handle.Pass(), GURL());
}

}  // namespace test
}  // namespace shell
}  // namespace mojo
