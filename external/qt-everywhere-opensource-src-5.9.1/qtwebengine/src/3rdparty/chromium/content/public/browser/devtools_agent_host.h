// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class ServerSocket;
}

namespace content {

class BrowserContext;
class DevToolsExternalAgentProxyDelegate;
class DevToolsSocketFactory;
class RenderFrameHost;
class WebContents;

// Describes interface for managing devtools agents from browser process.
class CONTENT_EXPORT DevToolsAgentHost
    : public base::RefCounted<DevToolsAgentHost> {
 public:
  static char kTypePage[];
  static char kTypeFrame[];
  static char kTypeSharedWorker[];
  static char kTypeServiceWorker[];
  static char kTypeExternal[];
  static char kTypeBrowser[];
  static char kTypeOther[];

  // Latest DevTools protocol version supported.
  static std::string GetProtocolVersion();

  // Returns whether particular version of DevTools protocol is supported.
  static bool IsSupportedProtocolVersion(const std::string& version);

  // Returns DevToolsAgentHost with a given |id| or nullptr of it doesn't exist.
  static scoped_refptr<DevToolsAgentHost> GetForId(const std::string& id);

  // Returns DevToolsAgentHost that can be used for inspecting |web_contents|.
  // A new DevToolsAgentHost will be created if it does not exist.
  static scoped_refptr<DevToolsAgentHost> GetOrCreateFor(
      WebContents* web_contents);

  // Returns DevToolsAgentHost that can be used for inspecting |frame_host|.
  // A new DevToolsAgentHost will be created if it does not exist.
  // For main frame cases, prefer using the above method which takes WebContents
  // instead.
  // TODO(dgozman): this is a temporary measure until we can inspect
  // cross-process subframes within a single agent.
  static scoped_refptr<DevToolsAgentHost> GetOrCreateFor(
      RenderFrameHost* frame_host);

  // Returns true iff an instance of DevToolsAgentHost for the |web_contents|
  // does exist.
  static bool HasFor(WebContents* web_contents);

  // Returns DevToolsAgentHost that can be used for inspecting shared worker
  // with given worker process host id and routing id.
  static scoped_refptr<DevToolsAgentHost> GetForWorker(int worker_process_id,
                                                       int worker_route_id);

  // Creates DevToolsAgentHost that communicates to the target by means of
  // provided |delegate|. |delegate| ownership is passed to the created agent
  // host.
  static scoped_refptr<DevToolsAgentHost> Forward(
      const std::string& id,
      std::unique_ptr<DevToolsExternalAgentProxyDelegate> delegate);

  using CreateServerSocketCallback =
      base::Callback<std::unique_ptr<net::ServerSocket>(std::string*)>;

  // Creates DevToolsAgentHost for the browser, which works with browser-wide
  // debugging protocol.
  static scoped_refptr<DevToolsAgentHost> CreateForBrowser(
      scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
      const CreateServerSocketCallback& socket_callback);

  static bool IsDebuggerAttached(WebContents* web_contents);

  using List = std::vector<scoped_refptr<DevToolsAgentHost>>;

  // Returns all DevToolsAgentHosts content is aware of.
  static List GetOrCreateAll();

  using DiscoveryCallback = base::Callback<void(List)>;

  // Returns all possible DevToolsAgentHosts embedder is aware of.
  static void DiscoverAllHosts(const DiscoveryCallback& callback);

  // Starts remote debugging.
  // Takes ownership over |socket_factory|.
  // If |frontend_url| is empty, assumes it's bundled.
  // If |active_port_output_directory| is non-empty, it is assumed the
  // socket_factory was initialized with an ephemeral port (0). The
  // port selected by the OS will be written to a well-known file in
  // the output directory.
  static void StartRemoteDebuggingServer(
      std::unique_ptr<DevToolsSocketFactory> server_socket_factory,
      const std::string& frontend_url,
      const base::FilePath& active_port_output_directory,
      const base::FilePath& debug_frontend_dir,
      const std::string& product_name,
      const std::string& user_agent);

  // Stops remote debugging.
  static void StopRemoteDebuggingServer();

  // Observer is notified about changes in DevToolsAgentHosts.
  static void AddObserver(DevToolsAgentHostObserver*);
  static void RemoveObserver(DevToolsAgentHostObserver*);

  // Attaches |client| to this agent host to start debugging.
  // Returns true iff attach succeeded.
  virtual bool AttachClient(DevToolsAgentHostClient* client) = 0;

  // Attaches |client| to this agent host to start debugging. Disconnects
  // any existing clients.
  virtual void ForceAttachClient(DevToolsAgentHostClient* client) = 0;

  // Already attached client detaches from this agent host to stop debugging it.
  // Returns true iff detach succeeded.
  virtual bool DetachClient(DevToolsAgentHostClient* client) = 0;

  // Returns true if there is a client attached.
  virtual bool IsAttached() = 0;

  // Sends |message| from |client| to the agent.
  // Returns true if the message is dispatched and handled.
  virtual bool DispatchProtocolMessage(DevToolsAgentHostClient* client,
                                       const std::string& message) = 0;

  // Starts inspecting element at position (|x|, |y|).
  virtual void InspectElement(DevToolsAgentHostClient* client,
                              int x,
                              int y) = 0;

  // Returns the unique id of the agent.
  virtual std::string GetId() = 0;

  // Returns the id of the parent host, or empty string if no parent.
  virtual std::string GetParentId() = 0;

  // Returns web contents instance for this host if any.
  virtual WebContents* GetWebContents() = 0;

  // Returns related browser context instance if available.
  virtual BrowserContext* GetBrowserContext() = 0;

  // Temporarily detaches WebContents from this host. Must be followed by
  // a call to ConnectWebContents (may leak the host instance otherwise).
  virtual void DisconnectWebContents() = 0;

  // Attaches render view host to this host.
  virtual void ConnectWebContents(WebContents* web_contents) = 0;

  // Returns agent host type.
  virtual std::string GetType() = 0;

  // Returns agent host title.
  virtual std::string GetTitle() = 0;

  // Returns the host description.
  virtual std::string GetDescription() = 0;

  // Returns url associated with agent host.
  virtual GURL GetURL() = 0;

  // Returns the favicon url for this host.
  virtual GURL GetFaviconURL() = 0;

  // Returns the frontend url for this host.
  virtual std::string GetFrontendURL() = 0;

  // Activates agent host. Returns false if the operation failed.
  virtual bool Activate() = 0;

  // Reloads the host.
  virtual void Reload() = 0;

  // Closes agent host. Returns false if the operation failed.
  virtual bool Close() = 0;

  // Returns the time when the host was last active.
  virtual base::TimeTicks GetLastActivityTime() = 0;

  // Terminates all debugging sessions and detaches all clients.
  static void DetachAllClients();

 protected:
  friend class base::RefCounted<DevToolsAgentHost>;
  virtual ~DevToolsAgentHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_
