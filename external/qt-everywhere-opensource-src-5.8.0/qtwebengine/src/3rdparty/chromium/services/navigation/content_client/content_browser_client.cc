// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/navigation/content_client/content_browser_client.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "content/public/common/mojo_application_info.h"
#include "content/public/common/mojo_shell_connection.h"
#include "content/shell/browser/shell_browser_context.h"
#include "services/navigation/content_client/browser_main_parts.h"
#include "services/navigation/navigation.h"

namespace navigation {

ContentBrowserClient::ContentBrowserClient() {}
ContentBrowserClient::~ContentBrowserClient() {}

content::BrowserMainParts* ContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  browser_main_parts_ = new BrowserMainParts(parameters);
  return browser_main_parts_;
}

std::string ContentBrowserClient::GetShellUserIdForBrowserContext(
    content::BrowserContext* browser_context) {
  // Unlike Chrome, where there are different browser contexts for each process,
  // each with their own userid, here there is only one and we should reuse the
  // same userid as our own process to avoid having to create multiple shell
  // connections.
  return content::MojoShellConnection::GetForProcess()->GetIdentity().user_id();
}

void ContentBrowserClient::RegisterInProcessMojoApplications(
    StaticMojoApplicationMap* apps) {
  content::MojoShellConnection::GetForProcess()->AddEmbeddedShellClient(
      base::WrapUnique(new Navigation));
}

}  // namespace navigation
