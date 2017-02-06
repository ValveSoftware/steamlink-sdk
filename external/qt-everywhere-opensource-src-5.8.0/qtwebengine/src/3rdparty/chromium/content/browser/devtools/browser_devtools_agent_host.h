// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_BROWSER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_BROWSER_DEVTOOLS_AGENT_HOST_H_

#include "content/browser/devtools/devtools_agent_host_impl.h"

namespace content {

class DevToolsProtocolHandler;

namespace devtools {
namespace browser { class BrowserHandler; }
namespace io { class IOHandler; }
namespace memory { class MemoryHandler; }
namespace system_info { class SystemInfoHandler; }
namespace tethering { class TetheringHandler; }
namespace tracing { class TracingHandler; }
}  // namespace devtools

class BrowserDevToolsAgentHost : public DevToolsAgentHostImpl {
 private:
  friend class DevToolsAgentHost;
  BrowserDevToolsAgentHost(
      scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
      const CreateServerSocketCallback& socket_callback);
  ~BrowserDevToolsAgentHost() override;

  // DevToolsAgentHostImpl implementation.
  void Attach() override;
  void Detach() override;

  // DevToolsAgentHost implementation.
  Type GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  bool Close() override;
  bool DispatchProtocolMessage(const std::string& message) override;

  std::unique_ptr<devtools::browser::BrowserHandler> browser_handler_;
  std::unique_ptr<devtools::io::IOHandler> io_handler_;
  std::unique_ptr<devtools::memory::MemoryHandler> memory_handler_;
  std::unique_ptr<devtools::system_info::SystemInfoHandler>
      system_info_handler_;
  std::unique_ptr<devtools::tethering::TetheringHandler> tethering_handler_;
  std::unique_ptr<devtools::tracing::TracingHandler> tracing_handler_;
  std::unique_ptr<DevToolsProtocolHandler> protocol_handler_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_BROWSER_DEVTOOLS_AGENT_HOST_H_
