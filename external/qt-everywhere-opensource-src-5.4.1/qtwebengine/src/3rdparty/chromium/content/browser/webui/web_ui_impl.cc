// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_impl.h"

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
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"

namespace content {

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

    base::JSONWriter::Write(arg_list[i], &json);
    parameters += base::UTF8ToUTF16(json);
  }
  return base::ASCIIToUTF16(function_name) +
      base::char16('(') + parameters + base::char16(')') + base::char16(';');
}

WebUIImpl::WebUIImpl(WebContents* contents)
    : link_transition_type_(PAGE_TRANSITION_LINK),
      bindings_(BINDINGS_POLICY_WEB_UI),
      web_contents_(contents) {
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

  // Do not attempt to set the toolkit property if WebUI is not enabled, e.g.,
  // the bookmarks manager page.
  if (!(bindings_ & BINDINGS_POLICY_WEB_UI))
    return;

#if defined(TOOLKIT_VIEWS)
  render_view_host->SetWebUIProperty("toolkit", "views");
#endif  // defined(TOOLKIT_VIEWS)
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

PageTransition WebUIImpl::GetLinkTransitionType() const {
  return link_transition_type_;
}

void WebUIImpl::SetLinkTransitionType(PageTransition type) {
  link_transition_type_ = type;
}

int WebUIImpl::GetBindings() const {
  return bindings_;
}

void WebUIImpl::SetBindings(int bindings) {
  bindings_ = bindings;
}

void WebUIImpl::OverrideJavaScriptFrame(const std::string& frame_name) {
  frame_name_ = frame_name;
}

WebUIController* WebUIImpl::GetController() const {
  return controller_.get();
}

void WebUIImpl::SetController(WebUIController* controller) {
  controller_.reset(controller);
}

void WebUIImpl::CallJavascriptFunction(const std::string& function_name) {
  DCHECK(base::IsStringASCII(function_name));
  base::string16 javascript = base::ASCIIToUTF16(function_name + "();");
  ExecuteJavascript(javascript);
}

void WebUIImpl::CallJavascriptFunction(const std::string& function_name,
                                       const base::Value& arg) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::CallJavascriptFunction(
    const std::string& function_name,
    const base::Value& arg1, const base::Value& arg2) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::CallJavascriptFunction(
    const std::string& function_name,
    const base::Value& arg1, const base::Value& arg2, const base::Value& arg3) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  args.push_back(&arg3);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::CallJavascriptFunction(
    const std::string& function_name,
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

void WebUIImpl::CallJavascriptFunction(
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

// WebUIImpl, protected: -------------------------------------------------------

void WebUIImpl::AddMessageHandler(WebUIMessageHandler* handler) {
  DCHECK(!handler->web_ui());
  handler->set_web_ui(this);
  handler->RegisterMessages();
  handlers_.push_back(handler);
}

void WebUIImpl::ExecuteJavascript(const base::string16& javascript) {
  RenderFrameHost* target_frame = TargetFrame();
  if (target_frame)
    target_frame->ExecuteJavaScript(javascript);
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

}  // namespace content
