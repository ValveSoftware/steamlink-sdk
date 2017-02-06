// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_content_main_delegate.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "blimp/engine/app/blimp_content_browser_client.h"
#include "blimp/engine/app/blimp_engine_crash_reporter_client.h"
#include "blimp/engine/renderer/blimp_content_renderer_client.h"
// TODO(marcinjb): Reenable gncheck for breakpad_linux.h after
// http://crbug.com/466890 is resolved.
#include "components/crash/content/app/breakpad_linux.h"  // nogncheck
#include "components/crash/content/app/crash_reporter_client.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "ui/base/resource/resource_bundle.h"

namespace blimp {
namespace engine {

// Blimp engine crash client. This should be available globally and should be
// long lived.
base::LazyInstance<BlimpEngineCrashReporterClient>
    g_blimp_engine_crash_reporter_client = LAZY_INSTANCE_INITIALIZER;

namespace {
void InitLogging() {
  // TODO(haibinlu): Remove this before release.
  // Enables a few verbose log by default.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch("vmodule")) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "vmodule", "remote_channel_main=1");
  }

  logging::LoggingSettings settings;
  base::FilePath log_filename;
  PathService::Get(base::DIR_EXE, &log_filename);
  log_filename = log_filename.AppendASCII("blimp_engine.log");
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  logging::SetLogItems(true,    // Process ID
                       true,    // Thread ID
                       true,    // Timestamp
                       false);  // Tick count
}
}  // namespace

BlimpContentMainDelegate::BlimpContentMainDelegate() {}

BlimpContentMainDelegate::~BlimpContentMainDelegate() {}

bool BlimpContentMainDelegate::BasicStartupComplete(int* exit_code) {
  InitLogging();
  content::SetContentClient(&content_client_);
  return false;
}

void BlimpContentMainDelegate::PreSandboxStartup() {
  // Enable crash reporting for all processes, and initialize the crash
  // reporter client.
  crash_reporter::SetCrashReporterClient(
      g_blimp_engine_crash_reporter_client.Pointer());
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  cmd->AppendSwitch(::switches::kEnableCrashReporter);
  breakpad::InitCrashReporter(
      cmd->GetSwitchValueASCII(::switches::kProcessType));

  InitializeResourceBundle();
}

void BlimpContentMainDelegate::InitializeResourceBundle() {
  base::FilePath pak_file;
  bool pak_file_valid = PathService::Get(base::DIR_MODULE, &pak_file);
  CHECK(pak_file_valid);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("blimp_engine.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
}

content::ContentBrowserClient*
BlimpContentMainDelegate::CreateContentBrowserClient() {
  DCHECK(!browser_client_);
  browser_client_.reset(new BlimpContentBrowserClient);
  return browser_client_.get();
}

content::ContentRendererClient*
BlimpContentMainDelegate::CreateContentRendererClient() {
  DCHECK(!renderer_client_);
  renderer_client_.reset(new BlimpContentRendererClient);
  return renderer_client_.get();
}

}  // namespace engine
}  // namespace blimp
