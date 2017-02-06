// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/declarative_user_script_master.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_user_script_loader.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/browser/web_ui_user_script_loader.h"
#include "extensions/common/user_script.h"

namespace extensions {

DeclarativeUserScriptMaster::DeclarativeUserScriptMaster(
    content::BrowserContext* browser_context,
    const HostID& host_id)
    : host_id_(host_id) {
  switch (host_id_.type()) {
    case HostID::EXTENSIONS:
      loader_.reset(new ExtensionUserScriptLoader(
          browser_context, host_id,
          false /* listen_for_extension_system_loaded */));
      break;
    case HostID::WEBUI:
      loader_.reset(new WebUIUserScriptLoader(browser_context, host_id));
      break;
  }
}

DeclarativeUserScriptMaster::~DeclarativeUserScriptMaster() {
}

void DeclarativeUserScriptMaster::AddScript(const UserScript& script) {
  std::set<UserScript> set;
  set.insert(script);
  loader_->AddScripts(set);
}

void DeclarativeUserScriptMaster::AddScripts(
    const std::set<UserScript>& scripts,
    int render_process_id,
    int render_view_id) {
  loader_->AddScripts(scripts, render_process_id, render_view_id);
}

void DeclarativeUserScriptMaster::RemoveScript(const UserScript& script) {
  std::set<UserScript> set;
  set.insert(script);
  loader_->RemoveScripts(set);
}

void DeclarativeUserScriptMaster::RemoveScripts(
    const std::set<UserScript>& scripts) {
  loader_->RemoveScripts(scripts);
}

void DeclarativeUserScriptMaster::ClearScripts() {
  loader_->ClearScripts();
}

}  // namespace extensions
