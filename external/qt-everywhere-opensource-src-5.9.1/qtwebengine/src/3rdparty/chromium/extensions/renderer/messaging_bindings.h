// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_MESSAGING_BINDINGS_H_
#define EXTENSIONS_RENDERER_MESSAGING_BINDINGS_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "extensions/renderer/object_backed_native_handler.h"

struct ExtensionMsg_ExternalConnectionInfo;
struct ExtensionMsg_TabConnectionInfo;

namespace content {
class RenderFrame;
}

namespace extensions {
class ExtensionPort;
struct Message;
class ScriptContextSet;

// Manually implements JavaScript bindings for extension messaging.
class MessagingBindings : public ObjectBackedNativeHandler {
 public:
  explicit MessagingBindings(ScriptContext* script_context);
  ~MessagingBindings() override;

  // Checks whether the port exists in the given frame. If it does not, a reply
  // is sent back to the browser.
  static void ValidateMessagePort(const ScriptContextSet& context_set,
                                  int port_id,
                                  content::RenderFrame* render_frame);

  // Dispatches the onConnect content script messaging event to some contexts
  // in |context_set|. If |restrict_to_render_frame| is specified, only contexts
  // in that render frame will receive the message.
  static void DispatchOnConnect(const ScriptContextSet& context_set,
                                int target_port_id,
                                const std::string& channel_name,
                                const ExtensionMsg_TabConnectionInfo& source,
                                const ExtensionMsg_ExternalConnectionInfo& info,
                                const std::string& tls_channel_id,
                                content::RenderFrame* restrict_to_render_frame);

  // Delivers a message sent using content script messaging to some of the
  // contexts in |bindings_context_set|. If |restrict_to_render_frame| is
  // specified, only contexts in that render view will receive the message.
  static void DeliverMessage(const ScriptContextSet& context_set,
                             int target_port_id,
                             const Message& message,
                             content::RenderFrame* restrict_to_render_frame);

  // Dispatches the onDisconnect event in response to the channel being closed.
  static void DispatchOnDisconnect(
      const ScriptContextSet& context_set,
      int port_id,
      const std::string& error_message,
      content::RenderFrame* restrict_to_render_frame);

  // Returns an existing port with the given |global_id|, or null.
  ExtensionPort* GetPortWithGlobalId(int global_id);

  // Creates a new port with the given |global_id|. MessagingBindings owns the
  // returned port.
  ExtensionPort* CreateNewPortWithGlobalId(int global_id);

  // Removes the port with the given |local_id|.
  void RemovePortWithLocalId(int local_id);

  base::WeakPtr<MessagingBindings> GetWeakPtr();

 private:
  using PortMap = std::map<int, std::unique_ptr<ExtensionPort>>;

  // JS Exposed Function: Sends a message along the given channel.
  void PostMessage(const v8::FunctionCallbackInfo<v8::Value>& args);

  // JS Exposed Function: Close a port, optionally forcefully (i.e. close the
  // whole channel instead of just the given port).
  void CloseChannel(const v8::FunctionCallbackInfo<v8::Value>& args);

  // JS Exposed Function: Binds |callback| to be invoked *sometime after*
  // |object| is garbage collected. We don't call the method re-entrantly so as
  // to avoid executing JS in some bizarro undefined mid-GC state, nor do we
  // then call into the script context if it's been invalidated.
  void BindToGC(const v8::FunctionCallbackInfo<v8::Value>& args);

  // JS Exposed Function: Opens a new channel to an extension.
  void OpenChannelToExtension(const v8::FunctionCallbackInfo<v8::Value>& args);

  // JS Exposed Function: Opens a new channel to a native application.
  void OpenChannelToNativeApp(const v8::FunctionCallbackInfo<v8::Value>& args);

  // JS Exposed Function: Opens a new channel to a tab.
  void OpenChannelToTab(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Helper function to close a port. See CloseChannel() for |force_close|
  // documentation.
  void ClosePort(int port_id, bool force_close);

  // Sets the global id for the port with |local_id|.
  void SetGlobalPortId(int local_id, int global_id);

  int GetNextLocalId();

  // Active ports, mapped by local port id.
  PortMap ports_;

  // Ports which are disconnected, but haven't been fully initialized. Once
  // initialized and any pending messages are sent, these ports are removed.
  PortMap disconnected_ports_;

  // The next available local id for a port.
  size_t next_local_id_ = 0;

  // The number of ports created in the 'beforeunload' event handler.
  size_t ports_created_in_before_unload_ = 0;

  // The number of ports created in the 'unload' event handler.
  size_t ports_created_in_unload_ = 0;

  // The number of ports created during during any time that isn't in the unload
  // or beforeunload handlers.
  int ports_created_normal_ = 0;

  base::WeakPtrFactory<MessagingBindings> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MessagingBindings);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_MESSAGING_BINDINGS_H_
