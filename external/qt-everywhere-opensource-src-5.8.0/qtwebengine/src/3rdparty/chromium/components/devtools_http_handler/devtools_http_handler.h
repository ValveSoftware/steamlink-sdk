// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_HTTP_HANDLER_DEVTOOLS_HTTP_HANDLER_H_
#define COMPONENTS_DEVTOOLS_HTTP_HANDLER_DEVTOOLS_HTTP_HANDLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/devtools_discovery/devtools_target_descriptor.h"
#include "net/http/http_status_code.h"

class GURL;

namespace base {
class DictionaryValue;
class Thread;
class Value;
}

namespace net {
class IPEndPoint;
class HttpServerRequestInfo;
class ServerSocket;
}

namespace devtools_http_handler {

class DevToolsAgentHostClientImpl;
class DevToolsHttpHandlerDelegate;
class ServerWrapper;

// This class is used for managing DevTools remote debugging server.
// Clients can connect to the specified ip:port and start debugging
// this browser.
class DevToolsHttpHandler {
 public:

  // Factory of net::ServerSocket. This is to separate instantiating dev tools
  // and instantiating server sockets.
  // All methods including destructor are called on a separate thread
  // different from any BrowserThread instance.
  class ServerSocketFactory {
   public:
    virtual ~ServerSocketFactory() {}

    // Returns a new instance of ServerSocket or nullptr if an error occurred.
    virtual std::unique_ptr<net::ServerSocket> CreateForHttpServer();

    // Creates a named socket for reversed tethering implementation (used with
    // remote debugging, primarily for mobile).
    virtual std::unique_ptr<net::ServerSocket> CreateForTethering(
        std::string* out_name);
  };

  // Takes ownership over |socket_factory| and |delegate|.
  // If |frontend_url| is empty, assumes it's bundled, and uses
  // |delegate->GetFrontendResource()|.
  // |delegate| is only accessed on UI thread.
  // If |active_port_output_directory| is non-empty, it is assumed the
  // socket_factory was initialized with an ephemeral port (0). The
  // port selected by the OS will be written to a well-known file in
  // the output directory.
  DevToolsHttpHandler(
      std::unique_ptr<ServerSocketFactory> server_socket_factory,
      const std::string& frontend_url,
      DevToolsHttpHandlerDelegate* delegate,
      const base::FilePath& active_port_output_directory,
      const base::FilePath& debug_frontend_dir,
      const std::string& product_name,
      const std::string& user_agent);
  ~DevToolsHttpHandler();

  // Returns the URL for the file at |path| in frontend.
  GURL GetFrontendURL(const std::string& path);

 private:
  friend class ServerWrapper;
  friend void ServerStartedOnUI(
      base::WeakPtr<DevToolsHttpHandler> handler,
      base::Thread* thread,
      ServerWrapper* server_wrapper,
      DevToolsHttpHandler::ServerSocketFactory* socket_factory,
      std::unique_ptr<net::IPEndPoint> ip_address);

  void OnJsonRequest(int connection_id,
                     const net::HttpServerRequestInfo& info);
  void OnThumbnailRequest(int connection_id, const std::string& target_id);
  void OnDiscoveryPageRequest(int connection_id);
  void OnFrontendResourceRequest(int connection_id, const std::string& path);
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info);
  void OnWebSocketMessage(int connection_id, const std::string& data);
  void OnClose(int connection_id);

  void ServerStarted(base::Thread* thread,
                     ServerWrapper* server_wrapper,
                     ServerSocketFactory* socket_factory,
                     std::unique_ptr<net::IPEndPoint> ip_address);

  devtools_discovery::DevToolsTargetDescriptor* GetDescriptor(
      const std::string& target_id);

  void SendJson(int connection_id,
                net::HttpStatusCode status_code,
                base::Value* value,
                const std::string& message);
  void Send200(int connection_id,
               const std::string& data,
               const std::string& mime_type);
  void Send404(int connection_id);
  void Send500(int connection_id,
               const std::string& message);
  void AcceptWebSocket(int connection_id,
                       const net::HttpServerRequestInfo& request);

  // Returns the front end url without the host at the beginning.
  std::string GetFrontendURLInternal(const std::string& target_id,
                                     const std::string& host);

  base::DictionaryValue* SerializeDescriptor(
      const devtools_discovery::DevToolsTargetDescriptor& descriptor,
      const std::string& host);

  // The thread used by the devtools handler to run server socket.
  base::Thread* thread_;
  std::string frontend_url_;
  std::string product_name_;
  std::string user_agent_;
  ServerWrapper* server_wrapper_;
  std::unique_ptr<net::IPEndPoint> server_ip_address_;
  typedef std::map<int, DevToolsAgentHostClientImpl*> ConnectionToClientMap;
  ConnectionToClientMap connection_to_client_;
  const std::unique_ptr<DevToolsHttpHandlerDelegate> delegate_;
  ServerSocketFactory* socket_factory_;
  using DescriptorMap =
      std::map<std::string, devtools_discovery::DevToolsTargetDescriptor*>;
  DescriptorMap descriptor_map_;
  base::WeakPtrFactory<DevToolsHttpHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsHttpHandler);
};

}  // namespace devtools_http_handler

#endif  // COMPONENTS_DEVTOOLS_HTTP_HANDLER_DEVTOOLS_HTTP_HANDLER_H_
