// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/default_shell_browser_main_delegate.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_tokenizer.h"
#include "build/build_config.h"
#include "extensions/common/switches.h"
#include "extensions/shell/browser/shell_extension_system.h"

#if defined(USE_AURA)
#include "extensions/shell/browser/shell_desktop_controller_aura.h"
#endif

#if defined(OS_MACOSX)
#include "extensions/shell/browser/shell_desktop_controller_mac.h"
#endif

namespace extensions {

DefaultShellBrowserMainDelegate::DefaultShellBrowserMainDelegate() {
}

DefaultShellBrowserMainDelegate::~DefaultShellBrowserMainDelegate() {
}

void DefaultShellBrowserMainDelegate::Start(
    content::BrowserContext* browser_context) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kLoadApps)) {
    ShellExtensionSystem* extension_system = static_cast<ShellExtensionSystem*>(
        ExtensionSystem::Get(browser_context));
    extension_system->Init();

    base::CommandLine::StringType path_list =
        command_line->GetSwitchValueNative(switches::kLoadApps);

    base::StringTokenizerT<base::CommandLine::StringType,
                           base::CommandLine::StringType::const_iterator>
        tokenizer(path_list, FILE_PATH_LITERAL(","));

    std::string launch_id;
    while (tokenizer.GetNext()) {
      base::FilePath app_absolute_dir =
          base::MakeAbsoluteFilePath(base::FilePath(tokenizer.token()));

      const Extension* extension = extension_system->LoadApp(app_absolute_dir);
      if (!extension)
        continue;
      if (launch_id.empty())
        launch_id = extension->id();
    }

    if (!launch_id.empty())
      extension_system->LaunchApp(launch_id);
    else
      LOG(ERROR) << "Could not load any apps.";
  } else {
    LOG(ERROR) << "--" << switches::kLoadApps
               << " unset; boredom is in your future";
  }
}

void DefaultShellBrowserMainDelegate::Shutdown() {
}

DesktopController* DefaultShellBrowserMainDelegate::CreateDesktopController() {
#if defined(USE_AURA)
  return new ShellDesktopControllerAura();
#elif defined(OS_MACOSX)
  return new ShellDesktopControllerMac();
#else
  return NULL;
#endif
}

}  // namespace extensions
