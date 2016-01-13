// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_HTTP_HANDLER_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_HTTP_HANDLER_IMPL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "net/http/http_status_code.h"
#include "net/server/http_server.h"

namespace base {
class DictionaryValue;
class ListValue;
class Thread;
class Value;
}

namespace net {
class StreamListenSocketFactory;
class URLRequestContextGetter;
}

namespace content {

class DevToolsBrowserTarget;
class DevToolsClientHost;

class DevToolsHttpHandlerImpl
    : public DevToolsHttpHandler,
      public base::RefCountedThreadSafe<DevToolsHttpHandlerImpl>,
      public net::HttpServer::Delegate {
 private:
  friend class base::RefCountedThreadSafe<DevToolsHttpHandlerImpl>;
  friend class DevToolsHttpHandler;

  // Takes ownership over |socket_factory|.
  DevToolsHttpHandlerImpl(const net::StreamListenSocketFactory* socket_factory,
                          const std::string& frontend_url,
                          DevToolsHttpHandlerDelegate* delegate,
                          const base::FilePath& active_port_output_directory);
  virtual ~DevToolsHttpHandlerImpl();
  void Start();

  // DevToolsHttpHandler implementation.
  virtual void Stop() OVERRIDE;
  virtual GURL GetFrontendURL() OVERRIDE;

  // net::HttpServer::Delegate implementation.
  virtual void OnHttpRequest(int connection_id,
                             const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketRequest(
      int connection_id,
      const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketMessage(int connection_id,
                                  const std::string& data) OVERRIDE;
  virtual void OnClose(int connection_id) OVERRIDE;

  void OnJsonRequestUI(int connection_id,
                       const net::HttpServerRequestInfo& info);
  void OnThumbnailRequestUI(int connection_id, const GURL& page_url);
  void OnDiscoveryPageRequestUI(int connection_id);

  void OnWebSocketRequestUI(int connection_id,
                            const net::HttpServerRequestInfo& info);
  void OnWebSocketMessageUI(int connection_id, const std::string& data);
  void OnCloseUI(int connection_id);

  void ResetHandlerThread();
  void ResetHandlerThreadAndRelease();

  void OnTargetListReceived(
      int connection_id,
      const std::string& host,
      const DevToolsHttpHandlerDelegate::TargetList& targets);

  DevToolsTarget* GetTarget(const std::string& id);

  void Init();
  void Teardown();

  void StartHandlerThread();
  void StopHandlerThread();

  void WriteActivePortToUserProfile();

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
  std::string GetFrontendURLInternal(const std::string target_id,
                                     const std::string& host);

  base::DictionaryValue* SerializeTarget(const DevToolsTarget& target,
                                         const std::string& host);

  // The thread used by the devtools handler to run server socket.
  scoped_ptr<base::Thread> thread_;

  std::string frontend_url_;
  scoped_ptr<const net::StreamListenSocketFactory> socket_factory_;
  scoped_refptr<net::HttpServer> server_;
  typedef std::map<int, DevToolsClientHost*> ConnectionToClientHostMap;
  ConnectionToClientHostMap connection_to_client_host_ui_;
  scoped_ptr<DevToolsHttpHandlerDelegate> delegate_;
  base::FilePath active_port_output_directory_;
  typedef std::map<std::string, DevToolsTarget*> TargetMap;
  TargetMap target_map_;
  typedef std::map<int, scoped_refptr<DevToolsBrowserTarget> > BrowserTargets;
  BrowserTargets browser_targets_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsHttpHandlerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_HTTP_HANDLER_IMPL_H_
