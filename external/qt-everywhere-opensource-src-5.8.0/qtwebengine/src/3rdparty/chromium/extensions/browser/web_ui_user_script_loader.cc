// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/web_ui_user_script_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/guest_view/web_view/web_ui/web_ui_url_fetcher.h"

namespace {

void SerializeOnFileThread(
    std::unique_ptr<extensions::UserScriptList> user_scripts,
    extensions::UserScriptLoader::LoadScriptsCallback callback) {
  std::unique_ptr<base::SharedMemory> memory =
      extensions::UserScriptLoader::Serialize(*user_scripts);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(callback, base::Passed(&user_scripts), base::Passed(&memory)));
}

}  // namespace

struct WebUIUserScriptLoader::UserScriptRenderInfo {
  int render_process_id;
  int render_view_id;

  UserScriptRenderInfo() : render_process_id(-1), render_view_id(-1) {}

  UserScriptRenderInfo(int render_process_id, int render_view_id)
      : render_process_id(render_process_id), render_view_id(render_view_id) {}
};

WebUIUserScriptLoader::WebUIUserScriptLoader(
    content::BrowserContext* browser_context,
    const HostID& host_id)
    : UserScriptLoader(browser_context, host_id), complete_fetchers_(0) {
  SetReady(true);
}

WebUIUserScriptLoader::~WebUIUserScriptLoader() {
}

void WebUIUserScriptLoader::AddScripts(
    const std::set<extensions::UserScript>& scripts,
    int render_process_id,
    int render_view_id) {
  UserScriptRenderInfo info(render_process_id, render_view_id);
  for (const extensions::UserScript& script : scripts) {
    script_render_info_map_.insert(
        std::pair<int, UserScriptRenderInfo>(script.id(), info));
  }

  extensions::UserScriptLoader::AddScripts(scripts);
}

void WebUIUserScriptLoader::LoadScripts(
    std::unique_ptr<extensions::UserScriptList> user_scripts,
    const std::set<HostID>& changed_hosts,
    const std::set<int>& added_script_ids,
    LoadScriptsCallback callback) {
  user_scripts_cache_.swap(user_scripts);
  scripts_loaded_callback_ = callback;

  // The total number of the tasks is used to trace whether all the fetches
  // are complete. Therefore, we store all the fetcher pointers in |fetchers_|
  // before we get theis number. Once we get the total number, start each
  // fetch tasks.
  DCHECK_EQ(0u, complete_fetchers_);

  for (extensions::UserScript& script : *user_scripts_cache_) {
    if (added_script_ids.count(script.id()) == 0)
      continue;

    auto iter = script_render_info_map_.find(script.id());
    DCHECK(iter != script_render_info_map_.end());
    int render_process_id = iter->second.render_process_id;
    int render_view_id = iter->second.render_view_id;

    content::BrowserContext* browser_context =
        content::RenderProcessHost::FromID(render_process_id)
            ->GetBrowserContext();

    CreateWebUIURLFetchers(&script.js_scripts(), browser_context,
                           render_process_id, render_view_id);
    CreateWebUIURLFetchers(&script.css_scripts(), browser_context,
                           render_process_id, render_view_id);

    script_render_info_map_.erase(script.id());
  }

  // If no fetch is needed, call OnWebUIURLFetchComplete directly.
  if (fetchers_.empty()) {
    OnWebUIURLFetchComplete();
    return;
  }
  for (const auto& fetcher : fetchers_)
    fetcher->Start();
}

void WebUIUserScriptLoader::CreateWebUIURLFetchers(
    extensions::UserScript::FileList* script_files,
    content::BrowserContext* browser_context,
    int render_process_id,
    int render_view_id) {
  for (extensions::UserScript::File& file : *script_files) {
    if (file.GetContent().empty()) {
      // The WebUIUserScriptLoader owns these WebUIURLFetchers. Once the
      // loader is destroyed, all the fetchers will be destroyed. Therefore,
      // we are sure it is safe to use base::Unretained(this) here.
      std::unique_ptr<WebUIURLFetcher> fetcher(new WebUIURLFetcher(
          browser_context, render_process_id, render_view_id, file.url(),
          base::Bind(&WebUIUserScriptLoader::OnSingleWebUIURLFetchComplete,
                     base::Unretained(this), &file)));
      fetchers_.push_back(std::move(fetcher));
    }
  }
}

void WebUIUserScriptLoader::OnSingleWebUIURLFetchComplete(
    extensions::UserScript::File* script_file,
    bool success,
    const std::string& data) {
  if (success) {
    // Remove BOM from |data|.
    if (base::StartsWith(data, base::kUtf8ByteOrderMark,
                         base::CompareCase::SENSITIVE)) {
      script_file->set_content(data.substr(strlen(base::kUtf8ByteOrderMark)));
    } else {
      script_file->set_content(data);
    }
  }

  ++complete_fetchers_;
  if (complete_fetchers_ == fetchers_.size()) {
    complete_fetchers_ = 0;
    OnWebUIURLFetchComplete();
    fetchers_.clear();
  }
}

void WebUIUserScriptLoader::OnWebUIURLFetchComplete() {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&SerializeOnFileThread, base::Passed(&user_scripts_cache_),
                 scripts_loaded_callback_));
  scripts_loaded_callback_.Reset();
}
