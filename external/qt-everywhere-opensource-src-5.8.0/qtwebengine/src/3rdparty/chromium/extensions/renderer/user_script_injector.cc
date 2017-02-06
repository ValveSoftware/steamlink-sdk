// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_script_injector.h"

#include <tuple>
#include <vector>

#include "base/lazy_instance.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/guest_view/extensions_guest_view_messages.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/injection_host.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/scripts_run_info.h"
#include "grit/extensions_renderer_resources.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace extensions {

namespace {

struct RoutingInfoKey {
  int routing_id;
  int script_id;

  RoutingInfoKey(int routing_id, int script_id)
      : routing_id(routing_id), script_id(script_id) {}

  bool operator<(const RoutingInfoKey& other) const {
    return std::tie(routing_id, script_id) <
           std::tie(other.routing_id, other.script_id);
  }
};

using RoutingInfoMap = std::map<RoutingInfoKey, bool>;

// These two strings are injected before and after the Greasemonkey API and
// user script to wrap it in an anonymous scope.
const char kUserScriptHead[] = "(function (unsafeWindow) {\n";
const char kUserScriptTail[] = "\n})(window);";

// A map records whether a given |script_id| from a webview-added user script
// is allowed to inject on the render of given |routing_id|.
// Once a script is added, the decision of whether or not allowed to inject
// won't be changed.
// After removed by the webview, the user scipt will also be removed
// from the render. Therefore, there won't be any query from the same
// |script_id| and |routing_id| pair.
base::LazyInstance<RoutingInfoMap> g_routing_info_map =
    LAZY_INSTANCE_INITIALIZER;

// Greasemonkey API source that is injected with the scripts.
struct GreasemonkeyApiJsString {
  GreasemonkeyApiJsString();
  blink::WebScriptSource GetSource() const;

 private:
  std::string source_;
};

// The below constructor, monstrous as it is, just makes a WebScriptSource from
// the GreasemonkeyApiJs resource.
GreasemonkeyApiJsString::GreasemonkeyApiJsString()
    : source_(ResourceBundle::GetSharedInstance()
                  .GetRawDataResource(IDR_GREASEMONKEY_API_JS)
                  .as_string()) {
}

blink::WebScriptSource GreasemonkeyApiJsString::GetSource() const {
  return blink::WebScriptSource(blink::WebString::fromUTF8(source_));
}

base::LazyInstance<GreasemonkeyApiJsString> g_greasemonkey_api =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

UserScriptInjector::UserScriptInjector(const UserScript* script,
                                       UserScriptSet* script_list,
                                       bool is_declarative)
    : script_(script),
      script_id_(script_->id()),
      host_id_(script_->host_id()),
      is_declarative_(is_declarative),
      user_script_set_observer_(this) {
  user_script_set_observer_.Add(script_list);
}

UserScriptInjector::~UserScriptInjector() {
}

void UserScriptInjector::OnUserScriptsUpdated(
    const std::set<HostID>& changed_hosts,
    const std::vector<UserScript*>& scripts) {
  // If the host causing this injection changed, then this injection
  // will be removed, and there's no guarantee the backing script still exists.
  if (changed_hosts.count(host_id_) > 0) {
    script_ = nullptr;
    return;
  }

  for (std::vector<UserScript*>::const_iterator iter = scripts.begin();
       iter != scripts.end();
       ++iter) {
    // We need to compare to |script_id_| (and not to script_->id()) because the
    // old |script_| may be deleted by now.
    if ((*iter)->id() == script_id_) {
      script_ = *iter;
      break;
    }
  }
}

UserScript::InjectionType UserScriptInjector::script_type() const {
  return UserScript::CONTENT_SCRIPT;
}

bool UserScriptInjector::ShouldExecuteInMainWorld() const {
  return false;
}

bool UserScriptInjector::IsUserGesture() const {
  return false;
}

bool UserScriptInjector::ExpectsResults() const {
  return false;
}

bool UserScriptInjector::ShouldInjectJs(
    UserScript::RunLocation run_location) const {
  return script_ && script_->run_location() == run_location &&
         !script_->js_scripts().empty();
}

bool UserScriptInjector::ShouldInjectCss(
    UserScript::RunLocation run_location) const {
  return script_ && run_location == UserScript::DOCUMENT_START &&
         !script_->css_scripts().empty();
}

PermissionsData::AccessType UserScriptInjector::CanExecuteOnFrame(
    const InjectionHost* injection_host,
    blink::WebLocalFrame* web_frame,
    int tab_id) const {
  // There is no harm in allowing the injection when the script is gone,
  // because there is nothing to inject.
  if (!script_)
    return PermissionsData::ACCESS_ALLOWED;

  if (script_->consumer_instance_type() ==
          UserScript::ConsumerInstanceType::WEBVIEW) {
    int routing_id = content::RenderView::FromWebView(web_frame->top()->view())
                        ->GetRoutingID();

    RoutingInfoKey key(routing_id, script_->id());

    RoutingInfoMap& map = g_routing_info_map.Get();
    auto iter = map.find(key);

    bool allowed = false;
    if (iter != map.end()) {
      allowed = iter->second;
    } else {
      // Send a SYNC IPC message to the browser to check if this is allowed.
      // This is not ideal, but is mitigated by the fact that this is only done
      // for webviews, and then only once per host.
      // TODO(hanxi): Find a more efficient way to do this.
      content::RenderThread::Get()->Send(
          new ExtensionsGuestViewHostMsg_CanExecuteContentScriptSync(
              routing_id, script_->id(), &allowed));
      map.insert(std::pair<RoutingInfoKey, bool>(key, allowed));
    }

    return allowed ? PermissionsData::ACCESS_ALLOWED
                   : PermissionsData::ACCESS_DENIED;
  }

  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      web_frame, web_frame->document().url(), script_->match_about_blank());

  return injection_host->CanExecuteOnFrame(
      effective_document_url,
      content::RenderFrame::FromWebFrame(web_frame),
      tab_id,
      is_declarative_);
}

std::vector<blink::WebScriptSource> UserScriptInjector::GetJsSources(
    UserScript::RunLocation run_location) const {
  std::vector<blink::WebScriptSource> sources;
  if (!script_)
    return sources;

  DCHECK_EQ(script_->run_location(), run_location);

  const UserScript::FileList& js_scripts = script_->js_scripts();

  for (UserScript::FileList::const_iterator iter = js_scripts.begin();
       iter != js_scripts.end();
       ++iter) {
    std::string content = iter->GetContent().as_string();

    // We add this dumb function wrapper for user scripts to emulate what
    // Greasemonkey does.
    if (script_->emulate_greasemonkey()) {
      content.insert(0, kUserScriptHead);
      content += kUserScriptTail;
    }
    sources.push_back(blink::WebScriptSource(
        blink::WebString::fromUTF8(content), iter->url()));
  }

  // Emulate Greasemonkey API for scripts that were converted to extension
  // user scripts.
  if (script_->emulate_greasemonkey())
    sources.insert(sources.begin(), g_greasemonkey_api.Get().GetSource());

  return sources;
}

std::vector<std::string> UserScriptInjector::GetCssSources(
    UserScript::RunLocation run_location) const {
  DCHECK_EQ(UserScript::DOCUMENT_START, run_location);

  std::vector<std::string> sources;
  if (!script_)
    return sources;

  const UserScript::FileList& css_scripts = script_->css_scripts();
  for (UserScript::FileList::const_iterator iter = css_scripts.begin();
       iter != css_scripts.end();
       ++iter) {
    sources.push_back(iter->GetContent().as_string());
  }
  return sources;
}

void UserScriptInjector::GetRunInfo(
    ScriptsRunInfo* scripts_run_info,
    UserScript::RunLocation run_location) const {
  if (!script_)
    return;

  if (ShouldInjectJs(run_location)) {
    const UserScript::FileList& js_scripts = script_->js_scripts();
    scripts_run_info->num_js += js_scripts.size();
    for (UserScript::FileList::const_iterator iter = js_scripts.begin();
         iter != js_scripts.end();
         ++iter) {
      scripts_run_info->executing_scripts[host_id_.id()].insert(
          iter->url().path());
    }
  }

  if (ShouldInjectCss(run_location))
    scripts_run_info->num_css += script_->css_scripts().size();
}

void UserScriptInjector::OnInjectionComplete(
    std::unique_ptr<base::Value> execution_result,
    UserScript::RunLocation run_location,
    content::RenderFrame* render_frame) {}

void UserScriptInjector::OnWillNotInject(InjectFailureReason reason,
                                         content::RenderFrame* render_frame) {
}

}  // namespace extensions
