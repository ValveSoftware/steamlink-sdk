// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_WEBSOCKET_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_WEBSOCKET_DISPATCHER_HOST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "content/common/content_export.h"
#include "content/common/websocket.h"
#include "content/public/browser/browser_message_filter.h"

namespace net {
class URLRequestContext;
}  // namespace net

namespace content {

struct WebSocketHandshakeRequest;
struct WebSocketHandshakeResponse;
class WebSocketHost;

// Creates a WebSocketHost object for each WebSocket channel, and dispatches
// WebSocketMsg_* messages sent from renderer to the appropriate WebSocketHost.
class CONTENT_EXPORT WebSocketDispatcherHost : public BrowserMessageFilter {
 public:
  typedef base::Callback<net::URLRequestContext*()> GetRequestContextCallback;

  // Given a routing_id, WebSocketHostFactory returns a new instance of
  // WebSocketHost or its subclass.
  typedef base::Callback<WebSocketHost*(int)> WebSocketHostFactory;  // NOLINT

  // Return value for methods that may delete the WebSocketHost. This enum is
  // binary-compatible with net::WebSocketEventInterface::ChannelState, to make
  // conversion cheap. By using a separate enum including net/ header files can
  // be avoided.
  enum WebSocketHostState {
    WEBSOCKET_HOST_ALIVE,
    WEBSOCKET_HOST_DELETED
  };

  WebSocketDispatcherHost(
      int process_id,
      const GetRequestContextCallback& get_context_callback);

  // For testing. Specify a factory method that creates mock version of
  // WebSocketHost.
  WebSocketDispatcherHost(int process_id,
                          const GetRequestContextCallback& get_context_callback,
                          const WebSocketHostFactory& websocket_host_factory);

  // BrowserMessageFilter:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // The following methods are used by WebSocketHost::EventInterface to send
  // IPCs from the browser to the renderer or child process. Any of them may
  // return WEBSOCKET_HOST_DELETED and delete the WebSocketHost on failure,
  // leading to the WebSocketChannel and EventInterface also being deleted.

  // Sends a WebSocketMsg_AddChannelResponse IPC, and then deletes and
  // unregisters the WebSocketHost if |fail| is true.
  WebSocketHostState SendAddChannelResponse(
      int routing_id,
      bool fail,
      const std::string& selected_protocol,
      const std::string& extensions) WARN_UNUSED_RESULT;

  // Sends a WebSocketMsg_SendFrame IPC.
  WebSocketHostState SendFrame(int routing_id,
                               bool fin,
                               WebSocketMessageType type,
                               const std::vector<char>& data);

  // Sends a WebSocketMsg_FlowControl IPC.
  WebSocketHostState SendFlowControl(int routing_id,
                                     int64 quota) WARN_UNUSED_RESULT;

  // Sends a WebSocketMsg_NotifyClosing IPC
  WebSocketHostState NotifyClosingHandshake(int routing_id) WARN_UNUSED_RESULT;

  // Sends a WebSocketMsg_NotifyStartOpeningHandshake IPC.
  WebSocketHostState NotifyStartOpeningHandshake(
      int routing_id,
      const WebSocketHandshakeRequest& request) WARN_UNUSED_RESULT;

  // Sends a WebSocketMsg_NotifyFinishOpeningHandshake IPC.
  WebSocketHostState NotifyFinishOpeningHandshake(
      int routing_id,
      const WebSocketHandshakeResponse& response) WARN_UNUSED_RESULT;

  // Sends a WebSocketMsg_NotifyFailure IPC and deletes and unregisters the
  // channel.
  WebSocketHostState NotifyFailure(
      int routing_id,
      const std::string& message) WARN_UNUSED_RESULT;

  // Sends a WebSocketMsg_DropChannel IPC and deletes and unregisters the
  // channel.
  WebSocketHostState DoDropChannel(
      int routing_id,
      bool was_clean,
      uint16 code,
      const std::string& reason) WARN_UNUSED_RESULT;

  // Returns whether the associated renderer process can read raw cookies.
  bool CanReadRawCookies() const;

  int render_process_id() const { return process_id_; }

 private:
  typedef base::hash_map<int, WebSocketHost*> WebSocketHostTable;

  virtual ~WebSocketDispatcherHost();

  WebSocketHost* CreateWebSocketHost(int routing_id);

  // Looks up a WebSocketHost object by |routing_id|. Returns the object if one
  // is found, or NULL otherwise.
  WebSocketHost* GetHost(int routing_id) const;

  // Sends the passed in IPC::Message via the BrowserMessageFilter::Send()
  // method. If sending the IPC fails, assumes that this connection is no
  // longer useable, calls DeleteWebSocketHost(), and returns
  // WEBSOCKET_HOST_DELETED. The behaviour is the same for all message types.
  WebSocketHostState SendOrDrop(IPC::Message* message) WARN_UNUSED_RESULT;

  // Deletes the WebSocketHost object associated with the given |routing_id| and
  // removes it from the |hosts_| table.
  void DeleteWebSocketHost(int routing_id);

  // Table of WebSocketHost objects, owned by this object, indexed by
  // routing_id.
  WebSocketHostTable hosts_;

  // The the process ID of the associated renderer process.
  const int process_id_;

  // A callback which returns the appropriate net::URLRequestContext for us to
  // use.
  GetRequestContextCallback get_context_callback_;

  WebSocketHostFactory websocket_host_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_WEBSOCKET_DISPATCHER_HOST_H_
