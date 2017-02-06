// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_FRAME_HELPER_H_
#define EXTENSIONS_RENDERER_EXTENSION_FRAME_HELPER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/common/console_message_level.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "extensions/common/view_type.h"

struct ExtensionMsg_ExternalConnectionInfo;
struct ExtensionMsg_TabConnectionInfo;

namespace base {
class ListValue;
}

namespace extensions {

class Dispatcher;
struct Message;
class ScriptContext;

// RenderFrame-level plumbing for extension features.
class ExtensionFrameHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<ExtensionFrameHelper> {
 public:
  ExtensionFrameHelper(content::RenderFrame* render_frame,
                       Dispatcher* extension_dispatcher);
  ~ExtensionFrameHelper() override;

  // Returns a list of extension RenderFrames that match the given filter
  // criteria. A |browser_window_id| of extension_misc::kUnknownWindowId
  // specifies "all", as does a |view_type| of VIEW_TYPE_INVALID.
  static std::vector<content::RenderFrame*> GetExtensionFrames(
      const std::string& extension_id,
      int browser_window_id,
      ViewType view_type);

  // Returns the main frame of the extension's background page, or null if there
  // isn't one in this process.
  static content::RenderFrame* GetBackgroundPageFrame(
      const std::string& extension_id);

  // Returns true if the given |context| is for any frame in the extension's
  // event page.
  // TODO(devlin): This isn't really used properly, and should probably be
  // deleted.
  static bool IsContextForEventPage(const ScriptContext* context);

  ViewType view_type() const { return view_type_; }
  int tab_id() const { return tab_id_; }
  int browser_window_id() const { return browser_window_id_; }
  bool did_create_current_document_element() const {
    return did_create_current_document_element_;
  }

  // Called when the document element has been inserted in this frame. This
  // method may invoke untrusted JavaScript code that invalidate the frame and
  // this ExtensionFrameHelper.
  void RunScriptsAtDocumentStart();

  // Called after the DOMContentLoaded event has fired.
  void RunScriptsAtDocumentEnd();

  // Schedule a callback, to be run at the next RunScriptsAtDocumentStart
  // notification. Only call this when you are certain that there will be such a
  // notification, e.g. from RenderFrameObserver::DidCreateDocumentElement.
  // Otherwise the callback is never invoked, or invoked for a document that you
  // were not expecting.
  void ScheduleAtDocumentStart(const base::Closure& callback);

  // Schedule a callback, to be run at the next RunScriptsAtDocumentEnd call.
  void ScheduleAtDocumentEnd(const base::Closure& callback);

 private:
  // RenderFrameObserver implementation.
  void DidCreateDocumentElement() override;
  void DidCreateNewDocument() override;
  void DidMatchCSS(
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors)
          override;
  void DidCreateScriptContext(v8::Local<v8::Context>,
                              int extension_group,
                              int world_id) override;
  void WillReleaseScriptContext(v8::Local<v8::Context>, int world_id) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;

  // IPC handlers.
  void OnExtensionValidateMessagePort(int port_id);
  void OnExtensionDispatchOnConnect(
      int target_port_id,
      const std::string& channel_name,
      const ExtensionMsg_TabConnectionInfo& source,
      const ExtensionMsg_ExternalConnectionInfo& info,
      const std::string& tls_channel_id);
  void OnExtensionDeliverMessage(int target_port_id,
                                 int source_tab_id,
                                 const Message& message);
  void OnExtensionDispatchOnDisconnect(int port_id,
                                       const std::string& error_message);
  void OnExtensionSetTabId(int tab_id);
  void OnUpdateBrowserWindowId(int browser_window_id);
  void OnNotifyRendererViewType(ViewType view_type);
  void OnExtensionResponse(int request_id,
                           bool success,
                           const base::ListValue& response,
                           const std::string& error);
  void OnExtensionMessageInvoke(const std::string& extension_id,
                                const std::string& module_name,
                                const std::string& function_name,
                                const base::ListValue& args,
                                bool user_gesture);

  // Type of view associated with the RenderFrame.
  ViewType view_type_;

  // The id of the tab the render frame is attached to.
  int tab_id_;

  // The id of the browser window the render frame is attached to.
  int browser_window_id_;

  Dispatcher* extension_dispatcher_;

  // Whether or not the current document element has been created.
  bool did_create_current_document_element_;

  // Callbacks to be run at the next RunScriptsAtDocumentStart notification.
  std::vector<base::Closure> document_element_created_callbacks_;

  // Callbacks to be run at the next RunScriptsAtDocumentEnd notification.
  std::vector<base::Closure> document_load_finished_callbacks_;

  base::WeakPtrFactory<ExtensionFrameHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionFrameHelper);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_FRAME_HELPER_H_
