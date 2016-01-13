// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_TARGET_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_TARGET_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/devtools_protocol.h"

namespace base {
class DictionaryValue;
class MessageLoopProxy;
class Value;
}  // namespace base

namespace net {
class HttpServer;
}  // namespace net

namespace content {

// This class handles the "Browser" target for remote debugging.
class DevToolsBrowserTarget
    : public base::RefCountedThreadSafe<DevToolsBrowserTarget> {
 public:
  DevToolsBrowserTarget(net::HttpServer* server, int connection_id);

  int connection_id() const { return connection_id_; }

  // Takes ownership of |handler|.
  void RegisterDomainHandler(const std::string& domain,
                             DevToolsProtocol::Handler* handler,
                             bool handle_on_ui_thread);

  void HandleMessage(const std::string& data);

  void Detach();

 private:
  friend class base::RefCountedThreadSafe<DevToolsBrowserTarget>;
  ~DevToolsBrowserTarget();

  void HandleCommandOnUIThread(
      DevToolsProtocol::Handler*,
      scoped_refptr<DevToolsProtocol::Command> command);

  void DeleteHandlersOnUIThread(
      std::vector<DevToolsProtocol::Handler*> handlers);

  void Respond(const std::string& message);
  void RespondFromUIThread(const std::string& message);

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  net::HttpServer* http_server_;
  const int connection_id_;

  typedef std::map<std::string, DevToolsProtocol::Handler*> DomainHandlerMap;
  DomainHandlerMap handlers_;
  std::set<std::string> handle_on_ui_thread_;
  base::WeakPtrFactory<DevToolsBrowserTarget> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsBrowserTarget);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_TARGET_H_
