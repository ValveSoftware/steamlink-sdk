// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TETHERING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TETHERING_HANDLER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"

namespace net {
class ServerSocket;
}

namespace content {
namespace devtools {
namespace tethering {

// This class implements reversed tethering handler.
class TetheringHandler {
 public:
  using Response = DevToolsProtocolClient::Response;
  using CreateServerSocketCallback =
      base::Callback<std::unique_ptr<net::ServerSocket>(std::string*)>;

  TetheringHandler(const CreateServerSocketCallback& socket_callback,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~TetheringHandler();

  void SetClient(std::unique_ptr<Client> client);

  Response Bind(DevToolsCommandId command_id, int port);
  Response Unbind(DevToolsCommandId command_id, int port);

 private:
  class TetheringImpl;

  void Accepted(uint16_t port, const std::string& name);
  bool Activate();

  void SendBindSuccess(DevToolsCommandId command_id);
  void SendUnbindSuccess(DevToolsCommandId command_id);
  void SendInternalError(DevToolsCommandId command_id,
                         const std::string& message);

  std::unique_ptr<Client> client_;
  CreateServerSocketCallback socket_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool is_active_;
  base::WeakPtrFactory<TetheringHandler> weak_factory_;

  static TetheringImpl* impl_;

  DISALLOW_COPY_AND_ASSIGN(TetheringHandler);
};

}  // namespace tethering
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TETHERING_HANDLER_H_
