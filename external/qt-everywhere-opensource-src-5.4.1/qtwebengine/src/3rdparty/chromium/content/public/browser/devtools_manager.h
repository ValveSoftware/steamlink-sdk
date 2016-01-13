// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"

namespace IPC {
class Message;
}

namespace content {

class DevToolsAgentHost;
class DevToolsClientHost;

// DevToolsManager connects a devtools client to inspected agent and routes
// devtools messages between the inspected instance represented by the agent
// and devtools front-end represented by the client. If inspected agent
// gets terminated DevToolsManager will notify corresponding client and
// remove it from the map.
class CONTENT_EXPORT DevToolsManager {
 public:
  static DevToolsManager* GetInstance();

  virtual ~DevToolsManager() {}

  // Routes devtools message from |from| client to corresponding
  // DevToolsAgentHost.
  virtual bool DispatchOnInspectorBackend(DevToolsClientHost* from,
                                          const std::string& message) = 0;

  // Disconnects all client hostst.
  virtual void CloseAllClientHosts() = 0;

  // Returns agent that has |client_host| attached to it if there is one.
  virtual DevToolsAgentHost* GetDevToolsAgentHostFor(
      DevToolsClientHost* client_host) = 0;

  // Registers new DevToolsClientHost for inspected |agent_host|. If there is
  // another DevToolsClientHost registered for the |agent_host| at the moment
  // it is disconnected.
  virtual void RegisterDevToolsClientHostFor(
      DevToolsAgentHost* agent_host,
      DevToolsClientHost* client_host) = 0;

  // This method will remove all references from the manager to the
  // DevToolsClientHost and unregister all listeners related to the
  // DevToolsClientHost. Called by closing client.
  virtual void ClientHostClosing(DevToolsClientHost* client_host) = 0;

  typedef base::Callback<void(DevToolsAgentHost*, bool attached)> Callback;

  virtual void AddAgentStateCallback(const Callback& callback) = 0;
  virtual void RemoveAgentStateCallback(const Callback& callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_H_
