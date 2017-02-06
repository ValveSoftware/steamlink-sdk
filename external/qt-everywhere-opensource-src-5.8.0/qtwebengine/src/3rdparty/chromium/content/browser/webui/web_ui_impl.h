// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_

#include <map>
#include <memory>
#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui.h"
#include "ipc/ipc_listener.h"

namespace content {
class RenderFrameHost;
class RenderViewHost;

class CONTENT_EXPORT WebUIImpl : public WebUI,
                                 public IPC::Listener,
                                 public base::SupportsWeakPtr<WebUIImpl> {
 public:
  WebUIImpl(WebContents* contents, const std::string& frame_name);
  ~WebUIImpl() override;

  // Called when a RenderView is created for a WebUI (reload after a renderer
  // crash) or when a WebUI is created for an RenderView (i.e. navigating from
  // chrome://downloads to chrome://bookmarks) or when both are new (i.e.
  // opening a new tab).
  void RenderViewCreated(RenderViewHost* render_view_host);

  // Called when a RenderView is reused for the same WebUI type (i.e. reload).
  void RenderViewReused(RenderViewHost* render_view_host, bool was_main_frame);

  // Called when the owning RenderFrameHost has started swapping out.
  void RenderFrameHostSwappingOut();

  // WebUI implementation:
  WebContents* GetWebContents() const override;
  WebUIController* GetController() const override;
  void SetController(WebUIController* controller) override;
  float GetDeviceScaleFactor() const override;
  const base::string16& GetOverriddenTitle() const override;
  void OverrideTitle(const base::string16& title) override;
  ui::PageTransition GetLinkTransitionType() const override;
  void SetLinkTransitionType(ui::PageTransition type) override;
  int GetBindings() const override;
  void SetBindings(int bindings) override;
  bool HasRenderFrame() override;
  void AddMessageHandler(WebUIMessageHandler* handler) override;
  typedef base::Callback<void(const base::ListValue*)> MessageCallback;
  void RegisterMessageCallback(const std::string& message,
                               const MessageCallback& callback) override;
  void ProcessWebUIMessage(const GURL& source_url,
                           const std::string& message,
                           const base::ListValue& args) override;
  bool CanCallJavascript() override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name) override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                    const base::Value& arg) override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                    const base::Value& arg1,
                                    const base::Value& arg2) override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                    const base::Value& arg1,
                                    const base::Value& arg2,
                                    const base::Value& arg3) override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                    const base::Value& arg1,
                                    const base::Value& arg2,
                                    const base::Value& arg3,
                                    const base::Value& arg4) override;
  void CallJavascriptFunctionUnsafe(
      const std::string& function_name,
      const std::vector<const base::Value*>& args) override;
  ScopedVector<WebUIMessageHandler>* GetHandlersForTesting() override;

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  class MainFrameNavigationObserver;

  // IPC message handling.
  void OnWebUISend(const GURL& source_url,
                   const std::string& message,
                   const base::ListValue& args);

  // Execute a string of raw JavaScript on the page.
  void ExecuteJavascript(const base::string16& javascript);

  // Finds the frame in which to execute JavaScript based on |frame_name_|. If
  // |frame_name_| is empty, the main frame is returned. May return NULL if no
  // frame of the specified name exists in the page.
  RenderFrameHost* TargetFrame();

  // A helper function for TargetFrame; adds a frame to the specified set if its
  // name matches |frame_name_|.
  void AddToSetIfFrameNameMatches(std::set<RenderFrameHost*>* frame_set,
                                  RenderFrameHost* host);

  // Called internally and by the owned MainFrameNavigationObserver.
  void DisallowJavascriptOnAllHandlers();

  // A map of message name -> message handling callback.
  typedef std::map<std::string, MessageCallback> MessageCallbackMap;
  MessageCallbackMap message_callbacks_;

  // Options that may be overridden by individual Web UI implementations. The
  // bool options default to false. See the public getters for more information.
  base::string16 overridden_title_;  // Defaults to empty string.
  ui::PageTransition link_transition_type_;  // Defaults to LINK.
  int bindings_;  // The bindings from BindingsPolicy that should be enabled for
                  // this page.

  // The WebUIMessageHandlers we own.
  ScopedVector<WebUIMessageHandler> handlers_;

  // Non-owning pointer to the WebContents this WebUI is associated with.
  WebContents* web_contents_;

  // Notifies this WebUI about notifications in the main frame.
  std::unique_ptr<MainFrameNavigationObserver> web_contents_observer_;

  // The name of the frame this WebUI is embedded in. If empty, the main frame
  // is used.
  const std::string frame_name_;

  std::unique_ptr<WebUIController> controller_;

  DISALLOW_COPY_AND_ASSIGN(WebUIImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_
