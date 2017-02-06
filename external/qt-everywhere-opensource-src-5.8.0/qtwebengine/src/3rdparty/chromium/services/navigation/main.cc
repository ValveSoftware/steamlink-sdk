// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "content/public/app/content_main.h"
#include "services/navigation/content_client/main_delegate.h"
#include "services/shell/runner/init.h"

#if defined(OS_WIN)
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

#if defined(OS_WIN)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  int argc = 0;
  char** argv = nullptr;
#else
int main(int argc, const char** argv) {
#endif
  base::CommandLine::Init(argc, argv);
  shell::WaitForDebuggerIfNecessary();

#if !defined(OFFICIAL_BUILD)
#if defined(OS_WIN)
  base::RouteStdioToConsole(false);
#endif
#endif

  base::FilePath log_filename;
  PathService::Get(base::DIR_EXE, &log_filename);
  log_filename = log_filename.AppendASCII("navigation.mojo.log");
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);

  navigation::MainDelegate delegate;
  content::ContentMainParams params(&delegate);
#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = { 0 };
  content::InitializeSandboxInfo(&sandbox_info);
  params.instance = GetModuleHandle(NULL);
  params.sandbox_info = &sandbox_info;
#endif
  return content::ContentMain(params);
}
