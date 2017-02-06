// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_DISPATCHER_H_
#define EXTENSIONS_RENDERER_DISPATCHER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "content/public/renderer/render_thread_observer.h"
#include "extensions/common/event_filter.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/resource_bundle_source_map.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/user_script_set_manager.h"
#include "extensions/renderer/v8_schema_registry.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "v8/include/v8.h"

class ChromeRenderViewTest;
class GURL;
class ModuleSystem;
class URLPattern;
struct ExtensionMsg_ExternalConnectionInfo;
struct ExtensionMsg_Loaded_Params;
struct ExtensionMsg_TabConnectionInfo;
struct ExtensionMsg_UpdatePermissions_Params;

namespace blink {
class WebFrame;
class WebLocalFrame;
class WebSecurityOrigin;
}

namespace base {
class ListValue;
}

namespace content {
class RenderThread;
}

namespace extensions {
class ContentWatcher;
class DispatcherDelegate;
class FilteredEventRouter;
class ManifestPermissionSet;
class RequestSender;
class ScriptContext;
class ScriptInjectionManager;
struct Message;

// Dispatches extension control messages sent to the renderer and stores
// renderer extension related state.
class Dispatcher : public content::RenderThreadObserver,
                   public UserScriptSetManager::Observer {
 public:
  explicit Dispatcher(DispatcherDelegate* delegate);
  ~Dispatcher() override;

  const ScriptContextSet& script_context_set() const {
    return *script_context_set_;
  }

  V8SchemaRegistry* v8_schema_registry() { return v8_schema_registry_.get(); }

  ContentWatcher* content_watcher() { return content_watcher_.get(); }

  RequestSender* request_sender() { return request_sender_.get(); }

  const std::string& webview_partition_id() { return webview_partition_id_; }

  bool activity_logging_enabled() const { return activity_logging_enabled_; }

  void OnRenderFrameCreated(content::RenderFrame* render_frame);

  bool IsExtensionActive(const std::string& extension_id) const;

  void DidCreateScriptContext(blink::WebLocalFrame* frame,
                              const v8::Local<v8::Context>& context,
                              int extension_group,
                              int world_id);

  // Runs on a different thread and should only use thread safe member
  // variables.
  void DidInitializeServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> v8_context,
      int embedded_worker_id,
      const GURL& url);

  void WillReleaseScriptContext(blink::WebLocalFrame* frame,
                                const v8::Local<v8::Context>& context,
                                int world_id);

  // Runs on a different thread and should not use any member variables.
  static void WillDestroyServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> v8_context,
      int embedded_worker_id,
      const GURL& url);

  // This method is not allowed to run JavaScript code in the frame.
  void DidCreateDocumentElement(blink::WebLocalFrame* frame);

  // These methods may run (untrusted) JavaScript code in the frame, and
  // cause |render_frame| to become invalid.
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame);
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame);

  void OnExtensionResponse(int request_id,
                           bool success,
                           const base::ListValue& response,
                           const std::string& error);

  // Dispatches the event named |event_name| to all render views.
  void DispatchEvent(const std::string& extension_id,
                     const std::string& event_name) const;

  // Shared implementation of the various MessageInvoke IPCs.
  void InvokeModuleSystemMethod(content::RenderFrame* render_frame,
                                const std::string& extension_id,
                                const std::string& module_name,
                                const std::string& function_name,
                                const base::ListValue& args,
                                bool user_gesture);

  // Returns a list of (module name, resource id) pairs for the JS modules to
  // add to the source map.
  static std::vector<std::pair<std::string, int> > GetJsResources();
  static void RegisterNativeHandlers(ModuleSystem* module_system,
                                     ScriptContext* context,
                                     Dispatcher* dispatcher,
                                     RequestSender* request_sender,
                                     V8SchemaRegistry* v8_schema_registry);

  bool WasWebRequestUsedBySomeExtensions() const { return webrequest_used_; }

 private:
  // The RendererPermissionsPolicyDelegateTest.CannotScriptWebstore test needs
  // to call the OnActivateExtension IPCs.
  friend class ::ChromeRenderViewTest;
  FRIEND_TEST_ALL_PREFIXES(RendererPermissionsPolicyDelegateTest,
                           CannotScriptWebstore);

  // RenderThreadObserver implementation:
  bool OnControlMessageReceived(const IPC::Message& message) override;
  void IdleNotification() override;
  void OnRenderProcessShutdown() override;

  void OnActivateExtension(const std::string& extension_id);
  void OnCancelSuspend(const std::string& extension_id);
  void OnDeliverMessage(int target_port_id,
                        int source_tab_id,
                        const Message& message);
  void OnDispatchOnConnect(int target_port_id,
                           const std::string& channel_name,
                           const ExtensionMsg_TabConnectionInfo& source,
                           const ExtensionMsg_ExternalConnectionInfo& info,
                           const std::string& tls_channel_id);
  void OnDispatchOnDisconnect(int port_id, const std::string& error_message);
  void OnLoaded(
      const std::vector<ExtensionMsg_Loaded_Params>& loaded_extensions);
  void OnMessageInvoke(const std::string& extension_id,
                       const std::string& module_name,
                       const std::string& function_name,
                       const base::ListValue& args,
                       bool user_gesture);
  void OnSetChannel(int channel);
  void OnSetScriptingWhitelist(
      const ExtensionsClient::ScriptingWhitelist& extension_ids);
  void OnSetSystemFont(const std::string& font_family,
                       const std::string& font_size);
  void OnSetWebViewPartitionID(const std::string& partition_id);
  void OnShouldSuspend(const std::string& extension_id, uint64_t sequence_id);
  void OnSuspend(const std::string& extension_id);
  void OnTransferBlobs(const std::vector<std::string>& blob_uuids);
  void OnUnloaded(const std::string& id);
  void OnUpdatePermissions(const ExtensionMsg_UpdatePermissions_Params& params);
  void OnUpdateTabSpecificPermissions(const GURL& visible_url,
                                      const std::string& extension_id,
                                      const URLPatternSet& new_hosts,
                                      bool update_origin_whitelist,
                                      int tab_id);
  void OnClearTabSpecificPermissions(
      const std::vector<std::string>& extension_ids,
      bool update_origin_whitelist,
      int tab_id);
  void OnUsingWebRequestAPI(bool webrequest_used);
  void OnSetActivityLoggingEnabled(bool enabled);

  // UserScriptSetManager::Observer implementation.
  void OnUserScriptsUpdated(const std::set<HostID>& changed_hosts,
                            const std::vector<UserScript*>& scripts) override;

  void UpdateActiveExtensions();

  // Sets up the host permissions for |extension|.
  void InitOriginPermissions(const Extension* extension);

  // Updates the host permissions for the extension url to include only those in
  // |new_patterns|, and remove from |old_patterns| that are no longer allowed.
  void UpdateOriginPermissions(const GURL& extension_url,
                               const URLPatternSet& old_patterns,
                               const URLPatternSet& new_patterns);

  // Enable custom element whitelist in Apps.
  void EnableCustomElementWhiteList();

  // Adds or removes bindings for every context belonging to |extension_id|, or
  // or all contexts if |extension_id| is empty.
  void UpdateBindings(const std::string& extension_id);

  void UpdateBindingsForContext(ScriptContext* context);

  void RegisterBinding(const std::string& api_name, ScriptContext* context);

  void RegisterNativeHandlers(ModuleSystem* module_system,
                              ScriptContext* context,
                              RequestSender* request_sender,
                              V8SchemaRegistry* v8_schema_registry);

  // Determines if a ScriptContext can connect to any externally_connectable-
  // enabled extension.
  bool IsRuntimeAvailableToContext(ScriptContext* context);

  // Updates a web page context with any content capabilities granted by active
  // extensions.
  void UpdateContentCapabilities(ScriptContext* context);

  // Inserts static source code into |source_map_|.
  void PopulateSourceMap();

  // Returns whether the current renderer hosts a platform app.
  bool IsWithinPlatformApp();

  // Gets |field| from |object| or creates it as an empty object if it doesn't
  // exist.
  static v8::Local<v8::Object> GetOrCreateObject(
      const v8::Local<v8::Object>& object,
      const std::string& field,
      v8::Isolate* isolate);

  static v8::Local<v8::Object> GetOrCreateBindObjectIfAvailable(
      const std::string& api_name,
      std::string* bind_name,
      ScriptContext* context);

  // Requires the GuestView modules in the module system of the ScriptContext
  // |context|.
  void RequireGuestViewModules(ScriptContext* context);

  // The delegate for this dispatcher. Not owned, but must extend beyond the
  // Dispatcher's own lifetime.
  DispatcherDelegate* delegate_;

  // True if the IdleNotification timer should be set.
  bool set_idle_notifications_;

  // The IDs of extensions that failed to load, mapped to the error message
  // generated on failure.
  std::map<std::string, std::string> extension_load_errors_;

  // All the bindings contexts that are currently loaded for this renderer.
  // There is zero or one for each v8 context.
  std::unique_ptr<ScriptContextSet> script_context_set_;

  std::unique_ptr<ContentWatcher> content_watcher_;

  std::unique_ptr<UserScriptSetManager> user_script_set_manager_;

  std::unique_ptr<ScriptInjectionManager> script_injection_manager_;

  // Same as above, but on a longer timer and will run even if the process is
  // not idle, to ensure that IdleHandle gets called eventually.
  std::unique_ptr<base::RepeatingTimer> forced_idle_timer_;

  // The extensions and apps that are active in this process.
  ExtensionIdSet active_extension_ids_;

  ResourceBundleSourceMap source_map_;

  // Cache for the v8 representation of extension API schemas.
  std::unique_ptr<V8SchemaRegistry> v8_schema_registry_;

  // Sends API requests to the extension host.
  std::unique_ptr<RequestSender> request_sender_;

  // The platforms system font family and size;
  std::string system_font_family_;
  std::string system_font_size_;

  // It is important for this to come after the ScriptInjectionManager, so that
  // the observer is destroyed before the UserScriptSet.
  ScopedObserver<UserScriptSetManager, UserScriptSetManager::Observer>
      user_script_set_manager_observer_;

  // Status of webrequest usage.
  bool webrequest_used_;

  // Whether or not extension activity is enabled.
  bool activity_logging_enabled_;

  // The WebView partition ID associated with this process's storage partition,
  // if this renderer is a WebView guest render process. Otherwise, this will be
  // empty.
  std::string webview_partition_id_;

  DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_DISPATCHER_H_
