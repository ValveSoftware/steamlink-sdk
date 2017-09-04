// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_PORT_H_
#define EXTENSIONS_RENDERER_EXTENSION_PORT_H_

#include <memory>
#include <vector>

#include "base/macros.h"

namespace content {
class RenderFrame;
}

namespace extensions {

struct Message;
class ScriptContext;

// A class representing information about a specific extension message port that
// handles sending related IPCs to the browser. Since global port IDs are
// assigned asynchronously, this object caches pending messages.
class ExtensionPort {
 public:
  ExtensionPort(ScriptContext* script_context, int local_id);
  ~ExtensionPort();

  // Sets the global id of the port (that is, the id used in the browser
  // process to send/receive messages). Any pending messages will be sent.
  void SetGlobalId(int id);

  // Posts a new message to the port. If the port is not initialized, the
  // message will be queued until it is.
  void PostExtensionMessage(std::unique_ptr<Message> message);

  // Closes the port. If there are pending messages, they will still be sent
  // assuming initialization completes (after which, the port will close).
  void Close(bool close_channel);

  int local_id() const { return local_id_; }
  int global_id() const { return global_id_; }
  bool initialized() const { return global_id_ != -1; }

 private:
  // Helper function to send the post message IPC.
  void PostMessageImpl(content::RenderFrame* render_frame,
                       const Message& message);

  // Sends the message to the browser that this port has been disconnected.
  void SendDisconnected(content::RenderFrame* render_frame);

  // The associated ScriptContext for this port. Since these objects are owned
  // by a NativeHandler, this should always be valid.
  ScriptContext* script_context_ = nullptr;

  // The local id of the port (used within JS bindings).
  int local_id_ = -1;

  // The global id of the port (used by the browser process).
  int global_id_ = -1;

  // The queue of any pending messages to be sent once the port is initialized.
  std::vector<std::unique_ptr<Message>> pending_messages_;

  // Whether or not the port is disconnected.
  bool is_disconnected_ = false;

  // Whether to close the full message channel.
  bool close_channel_ = false;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPort);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_PORT_H_
