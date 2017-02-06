// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/browser_handler.h"

namespace content {
namespace devtools {
namespace browser {

namespace {
const char kTargetTypeWebContents[] = "web_contents";
const char kTargetTypeFrame[] = "frame";
const char kTargetTypeSharedWorker[] = "shared_worker";
const char kTargetTypeServiceWorker[] = "service_worker";
const char kTargetTypeServiceOther[] = "other";
}

using Response = DevToolsProtocolClient::Response;

BrowserHandler::BrowserHandler() {
}

BrowserHandler::~BrowserHandler() {
}

void BrowserHandler::SetClient(std::unique_ptr<Client> client) {
  client_.swap(client);
}

static std::string GetTypeString(DevToolsAgentHost* agent_host) {
  switch (agent_host->GetType()) {
    case DevToolsAgentHost::TYPE_WEB_CONTENTS:
      return kTargetTypeWebContents;
    case DevToolsAgentHost::TYPE_FRAME:
      return kTargetTypeFrame;
    case DevToolsAgentHost::TYPE_SHARED_WORKER:
      return kTargetTypeSharedWorker;
    case DevToolsAgentHost::TYPE_SERVICE_WORKER:
      return kTargetTypeServiceWorker;
    default:
      return kTargetTypeServiceOther;
  }
}

Response BrowserHandler::GetTargets(TargetInfos* infos) {
  DevToolsAgentHost::List agents = DevToolsAgentHost::GetOrCreateAll();
  for (DevToolsAgentHost::List::iterator it = agents.begin();
       it != agents.end(); ++it) {
    DevToolsAgentHost* agent_host = (*it).get();
    scoped_refptr<devtools::browser::TargetInfo> info =
        devtools::browser::TargetInfo::Create()->
            set_target_id(agent_host->GetId())->
            set_type(GetTypeString(agent_host))->
            set_title(agent_host->GetTitle())->
            set_url(agent_host->GetURL().spec());
  }
  return Response::OK();
}

Response BrowserHandler::Attach(const std::string& targetId) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(targetId);
  if (!agent_host)
    return Response::ServerError("No target with given id found");
  bool success = agent_host->AttachClient(this);
  return success ? Response::OK() :
                   Response::ServerError("Target is already being debugged");
}

Response BrowserHandler::Detach(const std::string& targetId) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(targetId);
  if (!agent_host)
    return Response::ServerError("No target with given id found");
  bool success = agent_host->DetachClient(this);
  return success ? Response::OK() :
                   Response::ServerError("Target is not being debugged");
}

Response BrowserHandler::SendMessage(const std::string& targetId,
                                     const std::string& message) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(targetId);
  if (!agent_host)
    return Response::ServerError("No target with given id found");
  agent_host->DispatchProtocolMessage(this, message);
  return Response::OK();
}

void BrowserHandler::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host, const std::string& message) {
  client_->DispatchMessage(DispatchMessageParams::Create()->
      set_target_id(agent_host->GetId())->
      set_message(message));
}

void BrowserHandler::AgentHostClosed(DevToolsAgentHost* agent_host,
                                     bool replaced_with_another_client) {
}

}  // namespace browser
}  // namespace devtools
}  // namespace content
