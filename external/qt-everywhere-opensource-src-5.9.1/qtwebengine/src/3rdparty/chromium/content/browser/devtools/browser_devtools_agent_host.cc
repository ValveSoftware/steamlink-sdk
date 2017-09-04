// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/browser_devtools_agent_host.h"

#include "base/bind.h"
#include "base/guid.h"
#include "content/browser/devtools/devtools_protocol_handler.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/io_handler.h"
#include "content/browser/devtools/protocol/memory_handler.h"
#include "content/browser/devtools/protocol/system_info_handler.h"
#include "content/browser/devtools/protocol/tethering_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/browser/frame_host/frame_tree_node.h"

namespace content {

scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::CreateForBrowser(
    scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
    const CreateServerSocketCallback& socket_callback) {
  return new BrowserDevToolsAgentHost(tethering_task_runner, socket_callback);
}

BrowserDevToolsAgentHost::BrowserDevToolsAgentHost(
    scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
    const CreateServerSocketCallback& socket_callback)
    : DevToolsAgentHostImpl(base::GenerateGUID()),
      io_handler_(new devtools::io::IOHandler(GetIOContext())),
      memory_handler_(new devtools::memory::MemoryHandler()),
      system_info_handler_(new devtools::system_info::SystemInfoHandler()),
      tethering_handler_(
          new devtools::tethering::TetheringHandler(socket_callback,
                                                    tethering_task_runner)),
      tracing_handler_(new devtools::tracing::TracingHandler(
          devtools::tracing::TracingHandler::Browser,
          FrameTreeNode::kFrameTreeNodeInvalidId,
          GetIOContext())),
      protocol_handler_(new DevToolsProtocolHandler(this)) {
  DevToolsProtocolDispatcher* dispatcher = protocol_handler_->dispatcher();
  dispatcher->SetIOHandler(io_handler_.get());
  dispatcher->SetMemoryHandler(memory_handler_.get());
  dispatcher->SetSystemInfoHandler(system_info_handler_.get());
  dispatcher->SetTetheringHandler(tethering_handler_.get());
  dispatcher->SetTracingHandler(tracing_handler_.get());
  NotifyCreated();
}

BrowserDevToolsAgentHost::~BrowserDevToolsAgentHost() {
}

void BrowserDevToolsAgentHost::Attach() {
}

void BrowserDevToolsAgentHost::Detach() {
}

std::string BrowserDevToolsAgentHost::GetType() {
  return kTypeBrowser;
}

std::string BrowserDevToolsAgentHost::GetTitle() {
  return "";
}

GURL BrowserDevToolsAgentHost::GetURL() {
  return GURL();
}

bool BrowserDevToolsAgentHost::Activate() {
  return false;
}

bool BrowserDevToolsAgentHost::Close() {
  return false;
}

void BrowserDevToolsAgentHost::Reload() {
}

bool BrowserDevToolsAgentHost::DispatchProtocolMessage(
    const std::string& message) {
  protocol_handler_->HandleMessage(session() ? session()->session_id() : 0,
                                   message);
  return true;
}

}  // content
