// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/app/shell_main_delegate.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/browser/default_shell_browser_main_delegate.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/common/shell_content_client.h"
#include "extensions/shell/renderer/shell_content_renderer_client.h"
#include "extensions/shell/utility/shell_content_utility_client.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_paths.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "extensions/shell/app/paths_mac.h"
#endif

#if !defined(DISABLE_NACL)
#include "components/nacl/common/nacl_switches.h"
#if defined(OS_LINUX)
#include "components/nacl/common/nacl_paths.h"
#endif  // OS_LINUX
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
#include "components/nacl/zygote/nacl_fork_delegate_linux.h"
#endif  // OS_POSIX && !OS_MACOSX && !OS_ANDROID
#endif  // !DISABLE_NACL

namespace {

void InitLogging() {
  base::FilePath log_filename;
  PathService::Get(base::DIR_EXE, &log_filename);
  log_filename = log_filename.AppendASCII("app_shell.log");
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  logging::SetLogItems(true, true, true, true);
}

// Returns the path to the extensions_shell_and_test.pak file.
base::FilePath GetResourcesPakFilePath() {
#if defined(OS_MACOSX)
  return base::mac::PathForFrameworkBundleResource(
      CFSTR("extensions_shell_and_test.pak"));
#else
  base::FilePath extensions_shell_and_test_pak_path;
  PathService::Get(base::DIR_MODULE, &extensions_shell_and_test_pak_path);
  extensions_shell_and_test_pak_path =
      extensions_shell_and_test_pak_path.AppendASCII(
          "extensions_shell_and_test.pak");
  return extensions_shell_and_test_pak_path;
#endif  // OS_MACOSX
}

}  // namespace

namespace extensions {

ShellMainDelegate::ShellMainDelegate() {
}

ShellMainDelegate::~ShellMainDelegate() {
}

bool ShellMainDelegate::BasicStartupComplete(int* exit_code) {
  InitLogging();
  content_client_.reset(CreateContentClient());
  SetContentClient(content_client_.get());

#if defined(OS_MACOSX)
  OverrideChildProcessFilePath();
  // This must happen before InitializeResourceBundle.
  OverrideFrameworkBundlePath();
#endif

#if defined(OS_CHROMEOS)
  chromeos::RegisterPathProvider();
#endif
#if !defined(DISABLE_NACL) && defined(OS_LINUX)
  nacl::RegisterPathProvider();
#endif
  extensions::RegisterPathProvider();
  return false;
}

void ShellMainDelegate::PreSandboxStartup() {
  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  if (ProcessNeedsResourceBundle(process_type))
    InitializeResourceBundle();
}

content::ContentBrowserClient* ShellMainDelegate::CreateContentBrowserClient() {
  browser_client_.reset(CreateShellContentBrowserClient());
  return browser_client_.get();
}

content::ContentRendererClient*
ShellMainDelegate::CreateContentRendererClient() {
  renderer_client_.reset(CreateShellContentRendererClient());
  return renderer_client_.get();
}

content::ContentUtilityClient* ShellMainDelegate::CreateContentUtilityClient() {
  utility_client_.reset(CreateShellContentUtilityClient());
  return utility_client_.get();
}

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
void ShellMainDelegate::ZygoteStarting(
    ScopedVector<content::ZygoteForkDelegate>* delegates) {
#if !defined(DISABLE_NACL)
  nacl::AddNaClZygoteForkDelegates(delegates);
#endif  // DISABLE_NACL
}
#endif  // OS_POSIX && !OS_MACOSX && !OS_ANDROID

content::ContentClient* ShellMainDelegate::CreateContentClient() {
  return new ShellContentClient();
}

content::ContentBrowserClient*
ShellMainDelegate::CreateShellContentBrowserClient() {
  return new ShellContentBrowserClient(new DefaultShellBrowserMainDelegate());
}

content::ContentRendererClient*
ShellMainDelegate::CreateShellContentRendererClient() {
  return new ShellContentRendererClient();
}

content::ContentUtilityClient*
ShellMainDelegate::CreateShellContentUtilityClient() {
  return new ShellContentUtilityClient();
}

void ShellMainDelegate::InitializeResourceBundle() {
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      GetResourcesPakFilePath());
}

// static
bool ShellMainDelegate::ProcessNeedsResourceBundle(
    const std::string& process_type) {
  // The browser process has no process type flag, but needs resources.
  // On Linux the zygote process opens the resources for the renderers.
  return process_type.empty() ||
         process_type == switches::kZygoteProcess ||
         process_type == switches::kRendererProcess ||
#if !defined(DISABLE_NACL)
         process_type == switches::kNaClLoaderProcess ||
#endif
#if defined(OS_MACOSX)
         process_type == switches::kGpuProcess ||
#endif
         process_type == switches::kUtilityProcess;
}

}  // namespace extensions
