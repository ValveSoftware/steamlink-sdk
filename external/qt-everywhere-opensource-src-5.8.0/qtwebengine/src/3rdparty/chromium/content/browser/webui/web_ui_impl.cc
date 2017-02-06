// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_impl.h"

#include <stddef.h>

#include "base/debug/dump_without_crashing.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/view_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"

namespace content {

class WebUIImpl::MainFrameNavigationObserver : public WebContentsObserver {
 public:
  MainFrameNavigationObserver(WebUIImpl* web_ui, WebContents* contents)
      : WebContentsObserver(contents), web_ui_(web_ui) {}
  ~MainFrameNavigationObserver() override {}

 private:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    // Only disallow JavaScript on cross-document navigations in the main frame.
    if (!navigation_handle->IsInMainFrame() ||
        !navigation_handle->HasCommitted() || navigation_handle->IsSamePage()) {
      return;
    }

    web_ui_->DisallowJavascriptOnAllHandlers();
  }

  WebUIImpl* web_ui_;
};

const WebUI::TypeID WebUI::kNoWebUI = NULL;

// static
base::string16 WebUI::GetJavascriptCall(
    const std::string& function_name,
    const std::vector<const base::Value*>& arg_list) {
  base::string16 parameters;
  std::string json;
  for (size_t i = 0; i < arg_list.size(); ++i) {
    if (i > 0)
      parameters += base::char16(',');

    base::JSONWriter::Write(*arg_list[i], &json);
    parameters += base::UTF8ToUTF16(json);
  }
  return base::ASCIIToUTF16(function_name) +
      base::char16('(') + parameters + base::char16(')') + base::char16(';');
}

WebUIImpl::WebUIImpl(WebContents* contents, const std::string& frame_name)
    : link_transition_type_(ui::PAGE_TRANSITION_LINK),
      bindings_(BINDINGS_POLICY_WEB_UI),
      web_contents_(contents),
      web_contents_observer_(new MainFrameNavigationObserver(this, contents)),
      frame_name_(frame_name) {
  DCHECK(contents);
}

WebUIImpl::~WebUIImpl() {
  // Delete the controller first, since it may also be keeping a pointer to some
  // of the handlers and can call them at destruction.
  controller_.reset();
}

// WebUIImpl, public: ----------------------------------------------------------

bool WebUIImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebUIImpl, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_WebUISend, OnWebUISend)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebUIImpl::OnWebUISend(const GURL& source_url,
                            const std::string& message,
                            const base::ListValue& args) {
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->
          HasWebUIBindings(web_contents_->GetRenderProcessHost()->GetID()) ||
      !WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
          web_contents_->GetBrowserContext(), source_url)) {
    NOTREACHED() << "Blocked unauthorized use of WebUIBindings.";
    return;
  }

  ProcessWebUIMessage(source_url, message, args);
}

void WebUIImpl::RenderViewCreated(RenderViewHost* render_view_host) {
  controller_->RenderViewCreated(render_view_host);
}

void WebUIImpl::RenderViewReused(RenderViewHost* render_view_host,
                                 bool was_main_frame) {
  if (was_main_frame) {
    GURL site_url = render_view_host->GetSiteInstance()->GetSiteURL();
    GetContentClient()->browser()->LogWebUIUrl(site_url);
  }
}

void WebUIImpl::RenderFrameHostSwappingOut() {
  DisallowJavascriptOnAllHandlers();
}

WebContents* WebUIImpl::GetWebContents() const {
  return web_contents_;
}

float WebUIImpl::GetDeviceScaleFactor() const {
  return GetScaleFactorForView(web_contents_->GetRenderWidgetHostView());
}

const base::string16& WebUIImpl::GetOverriddenTitle() const {
  return overridden_title_;
}

void WebUIImpl::OverrideTitle(const base::string16& title) {
  overridden_title_ = title;
}

ui::PageTransition WebUIImpl::GetLinkTransitionType() const {
  return link_transition_type_;
}

void WebUIImpl::SetLinkTransitionType(ui::PageTransition type) {
  link_transition_type_ = type;
}

int WebUIImpl::GetBindings() const {
  return bindings_;
}

void WebUIImpl::SetBindings(int bindings) {
  bindings_ = bindings;
}

bool WebUIImpl::HasRenderFrame() {
  return TargetFrame() != nullptr;
}

WebUIController* WebUIImpl::GetController() const {
  return controller_.get();
}

void WebUIImpl::SetController(WebUIController* controller) {
  controller_.reset(controller);
}

bool WebUIImpl::CanCallJavascript() {
  RenderFrameHost* target_frame = TargetFrame();
  return target_frame &&
         (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
              target_frame->GetProcess()->GetID()) ||
          // It's possible to load about:blank in a Web UI renderer.
          // See http://crbug.com/42547
          target_frame->GetLastCommittedURL().spec() == url::kAboutBlankURL);
}

void WebUIImpl::CallJavascriptFunctionUnsafe(const std::string& function_name) {
  DCHECK(base::IsStringASCII(function_name));
  base::string16 javascript = base::ASCIIToUTF16(function_name + "();");
  ExecuteJavascript(javascript);
}

void WebUIImpl::CallJavascriptFunctionUnsafe(const std::string& function_name,
                                             const base::Value& arg) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::CallJavascriptFunctionUnsafe(const std::string& function_name,
                                             const base::Value& arg1,
                                             const base::Value& arg2) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::CallJavascriptFunctionUnsafe(const std::string& function_name,
                                             const base::Value& arg1,
                                             const base::Value& arg2,
                                             const base::Value& arg3) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  args.push_back(&arg3);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::CallJavascriptFunctionUnsafe(const std::string& function_name,
                                             const base::Value& arg1,
                                             const base::Value& arg2,
                                             const base::Value& arg3,
                                             const base::Value& arg4) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  args.push_back(&arg3);
  args.push_back(&arg4);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::CallJavascriptFunctionUnsafe(
    const std::string& function_name,
    const std::vector<const base::Value*>& args) {
  DCHECK(base::IsStringASCII(function_name));
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::RegisterMessageCallback(const std::string &message,
                                        const MessageCallback& callback) {
  message_callbacks_.insert(std::make_pair(message, callback));
}

void WebUIImpl::ProcessWebUIMessage(const GURL& source_url,
                                    const std::string& message,
                                    const base::ListValue& args) {
  if (controller_->OverrideHandleWebUIMessage(source_url, message, args))
    return;

  // Look up the callback for this message.
  MessageCallbackMap::const_iterator callback =
      message_callbacks_.find(message);
  if (callback != message_callbacks_.end()) {
    // Forward this message and content on.
    callback->second.Run(&args);
  } else {
    NOTREACHED() << "Unhandled chrome.send(\"" << message << "\");";
  }
}

ScopedVector<WebUIMessageHandler>* WebUIImpl::GetHandlersForTesting() {
  return &handlers_;
}

// WebUIImpl, protected: -------------------------------------------------------

void WebUIImpl::AddMessageHandler(WebUIMessageHandler* handler) {
  DCHECK(!handler->web_ui());
  handler->set_web_ui(this);
  handler->RegisterMessages();
  handlers_.push_back(handler);
}

void WebUIImpl::ExecuteJavascript(const base::string16& javascript) {
  // Silently ignore the request. Would be nice to clean-up WebUI so we
  // could turn this into a CHECK(). http://crbug.com/516690.
  if (!CanCallJavascript())
    return;

  TargetFrame()->ExecuteJavaScript(javascript);
}

RenderFrameHost* WebUIImpl::TargetFrame() {
  if (frame_name_.empty())
    return web_contents_->GetMainFrame();

  std::set<RenderFrameHost*> frame_set;
  web_contents_->ForEachFrame(base::Bind(&WebUIImpl::AddToSetIfFrameNameMatches,
                                         base::Unretained(this),
                                         &frame_set));

  // It happens that some sub-pages attempt to send JavaScript messages before
  // their frames are loaded.
  DCHECK_GE(1U, frame_set.size());
  if (frame_set.empty())
    return NULL;
  return *frame_set.begin();
}

void WebUIImpl::AddToSetIfFrameNameMatches(
    std::set<RenderFrameHost*>* frame_set,
    RenderFrameHost* host) {
  if (host->GetFrameName() == frame_name_)
    frame_set->insert(host);
}

void WebUIImpl::DisallowJavascriptOnAllHandlers() {
  for (WebUIMessageHandler* handler : handlers_)
    handler->DisallowJavascript();
}

}  // namespace content
