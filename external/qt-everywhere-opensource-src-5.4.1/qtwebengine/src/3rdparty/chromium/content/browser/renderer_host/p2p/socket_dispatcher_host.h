// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "content/browser/renderer_host/p2p/socket_host_throttler.h"
#include "content/common/p2p_socket_type.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_change_notifier.h"

namespace net {
class URLRequestContextGetter;
}

namespace talk_base {
struct PacketOptions;
}

namespace content {

class P2PSocketHost;
class ResourceContext;

#if defined(ENABLE_WEBRTC)
class P2PSocketDispatcherHost
    : public content::BrowserMessageFilter,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  P2PSocketDispatcherHost(content::ResourceContext* resource_context,
                          net::URLRequestContextGetter* url_context);

  // content::BrowserMessageFilter overrides.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // net::NetworkChangeNotifier::IPAddressObserver interface.
  virtual void OnIPAddressChanged() OVERRIDE;

  // Starts the RTP packet header dumping. Must be called on the IO thread.
  void StartRtpDump(
      bool incoming,
      bool outgoing,
      const RenderProcessHost::WebRtcRtpPacketCallback& packet_callback);

  // Stops the RTP packet header dumping. Must be Called on the UI thread.
  void StopRtpDumpOnUIThread(bool incoming, bool outgoing);

 protected:
  virtual ~P2PSocketDispatcherHost();

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<P2PSocketDispatcherHost>;

  typedef std::map<int, P2PSocketHost*> SocketsMap;

  class DnsRequest;

  P2PSocketHost* LookupSocket(int socket_id);

  // Handlers for the messages coming from the renderer.
  void OnStartNetworkNotifications();
  void OnStopNetworkNotifications();
  void OnGetHostAddress(const std::string& host_name,
                        int32 request_id);

  void OnCreateSocket(P2PSocketType type,
                      int socket_id,
                      const net::IPEndPoint& local_address,
                      const P2PHostAndIPEndPoint& remote_address);
  void OnAcceptIncomingTcpConnection(int listen_socket_id,
                                     const net::IPEndPoint& remote_address,
                                     int connected_socket_id);
  void OnSend(int socket_id,
              const net::IPEndPoint& socket_address,
              const std::vector<char>& data,
              const talk_base::PacketOptions& options,
              uint64 packet_id);
  void OnSetOption(int socket_id, P2PSocketOption option, int value);
  void OnDestroySocket(int socket_id);

  void DoGetNetworkList();
  void SendNetworkList(const net::NetworkInterfaceList& list);

  void OnAddressResolved(DnsRequest* request,
                         const net::IPAddressList& addresses);

  void StopRtpDumpOnIOThread(bool incoming, bool outgoing);

  content::ResourceContext* resource_context_;
  scoped_refptr<net::URLRequestContextGetter> url_context_;

  SocketsMap sockets_;

  bool monitoring_networks_;

  std::set<DnsRequest*> dns_requests_;
  P2PMessageThrottler throttler_;

  bool dump_incoming_rtp_packet_;
  bool dump_outgoing_rtp_packet_;
  RenderProcessHost::WebRtcRtpPacketCallback packet_callback_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketDispatcherHost);
};
#endif

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
