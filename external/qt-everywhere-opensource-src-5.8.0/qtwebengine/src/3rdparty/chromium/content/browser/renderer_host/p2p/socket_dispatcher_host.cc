// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_dispatcher_host.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/p2p/socket_host.h"
#include "content/common/p2p_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/base/sys_addrinfo.h"
#include "net/dns/single_request_host_resolver.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_factory.h"
#include "net/udp/datagram_client_socket.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserMessageFilter;
using content::BrowserThread;

namespace content {

namespace {

// Used by GetDefaultLocalAddress as the target to connect to for getting the
// default local address. They are public DNS servers on the internet.
const uint8_t kPublicIPv4Host[] = {8, 8, 8, 8};
const uint8_t kPublicIPv6Host[] = {
    0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0, 0, 0, 0, 0, 0, 0, 0, 0x88, 0x88};
const int kPublicPort = 53;  // DNS port.

}  // namespace

const size_t kMaximumPacketSize = 32768;

class P2PSocketDispatcherHost::DnsRequest {
 public:
  typedef base::Callback<void(const net::IPAddressList&)> DoneCallback;

  DnsRequest(int32_t request_id, net::HostResolver* host_resolver)
      : request_id_(request_id), resolver_(host_resolver) {}

  void Resolve(const std::string& host_name,
               const DoneCallback& done_callback) {
    DCHECK(!done_callback.is_null());

    host_name_ = host_name;
    done_callback_ = done_callback;

    // Return an error if it's an empty string.
    if (host_name_.empty()) {
      net::IPAddressList address_list;
      done_callback_.Run(address_list);
      return;
    }

    // Add period at the end to make sure that we only resolve
    // fully-qualified names.
    if (host_name_.at(host_name_.size() - 1) != '.')
      host_name_ = host_name_ + '.';

    net::HostResolver::RequestInfo info(net::HostPortPair(host_name_, 0));
    int result = resolver_.Resolve(
        info,
        net::DEFAULT_PRIORITY,
        &addresses_,
        base::Bind(&P2PSocketDispatcherHost::DnsRequest::OnDone,
                   base::Unretained(this)),
        net::BoundNetLog());
    if (result != net::ERR_IO_PENDING)
      OnDone(result);
  }

  int32_t request_id() { return request_id_; }

 private:
  void OnDone(int result) {
    net::IPAddressList list;
    if (result != net::OK) {
      LOG(ERROR) << "Failed to resolve address for " << host_name_
                 << ", errorcode: " << result;
      done_callback_.Run(list);
      return;
    }

    DCHECK(!addresses_.empty());
    for (net::AddressList::iterator iter = addresses_.begin();
         iter != addresses_.end(); ++iter) {
      list.push_back(iter->address());
    }
    done_callback_.Run(list);
  }

  int32_t request_id_;
  net::AddressList addresses_;

  std::string host_name_;
  net::SingleRequestHostResolver resolver_;

  DoneCallback done_callback_;
};

P2PSocketDispatcherHost::P2PSocketDispatcherHost(
    content::ResourceContext* resource_context,
    net::URLRequestContextGetter* url_context)
    : BrowserMessageFilter(P2PMsgStart),
      resource_context_(resource_context),
      url_context_(url_context),
      monitoring_networks_(false),
      dump_incoming_rtp_packet_(false),
      dump_outgoing_rtp_packet_(false) {
}

void P2PSocketDispatcherHost::OnChannelClosing() {
  // Since the IPC sender is gone, close pending connections.
  STLDeleteContainerPairSecondPointers(sockets_.begin(), sockets_.end());
  sockets_.clear();

  STLDeleteContainerPointers(dns_requests_.begin(), dns_requests_.end());
  dns_requests_.clear();

  if (monitoring_networks_) {
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
    monitoring_networks_ = false;
  }
}

void P2PSocketDispatcherHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool P2PSocketDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(P2PSocketDispatcherHost, message)
    IPC_MESSAGE_HANDLER(P2PHostMsg_StartNetworkNotifications,
                        OnStartNetworkNotifications)
    IPC_MESSAGE_HANDLER(P2PHostMsg_StopNetworkNotifications,
                        OnStopNetworkNotifications)
    IPC_MESSAGE_HANDLER(P2PHostMsg_GetHostAddress, OnGetHostAddress)
    IPC_MESSAGE_HANDLER(P2PHostMsg_CreateSocket, OnCreateSocket)
    IPC_MESSAGE_HANDLER(P2PHostMsg_AcceptIncomingTcpConnection,
                        OnAcceptIncomingTcpConnection)
    IPC_MESSAGE_HANDLER(P2PHostMsg_Send, OnSend)
    IPC_MESSAGE_HANDLER(P2PHostMsg_SetOption, OnSetOption)
    IPC_MESSAGE_HANDLER(P2PHostMsg_DestroySocket, OnDestroySocket)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void P2PSocketDispatcherHost::OnIPAddressChanged() {
  // Notify the renderer about changes to list of network interfaces.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE, base::Bind(
          &P2PSocketDispatcherHost::DoGetNetworkList, this));
}

void P2PSocketDispatcherHost::StartRtpDump(
    bool incoming,
    bool outgoing,
    const RenderProcessHost::WebRtcRtpPacketCallback& packet_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if ((!dump_incoming_rtp_packet_ && incoming) ||
      (!dump_outgoing_rtp_packet_ && outgoing)) {
    if (incoming)
      dump_incoming_rtp_packet_ = true;

    if (outgoing)
      dump_outgoing_rtp_packet_ = true;

    packet_callback_ = packet_callback;
    for (SocketsMap::iterator it = sockets_.begin(); it != sockets_.end(); ++it)
      it->second->StartRtpDump(incoming, outgoing, packet_callback);
  }
}

void P2PSocketDispatcherHost::StopRtpDumpOnUIThread(bool incoming,
                                                    bool outgoing) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&P2PSocketDispatcherHost::StopRtpDumpOnIOThread,
                 this,
                 incoming,
                 outgoing));
}

P2PSocketDispatcherHost::~P2PSocketDispatcherHost() {
  DCHECK(sockets_.empty());
  DCHECK(dns_requests_.empty());

  if (monitoring_networks_)
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

P2PSocketHost* P2PSocketDispatcherHost::LookupSocket(int socket_id) {
  SocketsMap::iterator it = sockets_.find(socket_id);
  return (it == sockets_.end()) ? NULL : it->second;
}

void P2PSocketDispatcherHost::OnStartNetworkNotifications() {
  if (!monitoring_networks_) {
    net::NetworkChangeNotifier::AddIPAddressObserver(this);
    monitoring_networks_ = true;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE, base::Bind(
          &P2PSocketDispatcherHost::DoGetNetworkList, this));
}

void P2PSocketDispatcherHost::OnStopNetworkNotifications() {
  if (monitoring_networks_) {
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
    monitoring_networks_ = false;
  }
}

void P2PSocketDispatcherHost::OnGetHostAddress(const std::string& host_name,
                                               int32_t request_id) {
  DnsRequest* request = new DnsRequest(request_id,
                                       resource_context_->GetHostResolver());
  dns_requests_.insert(request);
  request->Resolve(host_name, base::Bind(
      &P2PSocketDispatcherHost::OnAddressResolved,
      base::Unretained(this), request));
}

void P2PSocketDispatcherHost::OnCreateSocket(
    P2PSocketType type, int socket_id,
    const net::IPEndPoint& local_address,
    const P2PHostAndIPEndPoint& remote_address) {
  if (LookupSocket(socket_id)) {
    LOG(ERROR) << "Received P2PHostMsg_CreateSocket for socket "
        "that already exists.";
    return;
  }

  std::unique_ptr<P2PSocketHost> socket(P2PSocketHost::Create(
      this, socket_id, type, url_context_.get(), &throttler_));

  if (!socket) {
    Send(new P2PMsg_OnError(socket_id));
    return;
  }

  if (socket->Init(local_address, remote_address)) {
    sockets_[socket_id] = socket.release();

    if (dump_incoming_rtp_packet_ || dump_outgoing_rtp_packet_) {
      sockets_[socket_id]->StartRtpDump(dump_incoming_rtp_packet_,
                                        dump_outgoing_rtp_packet_,
                                        packet_callback_);
    }
  }
}

void P2PSocketDispatcherHost::OnAcceptIncomingTcpConnection(
    int listen_socket_id, const net::IPEndPoint& remote_address,
    int connected_socket_id) {
  P2PSocketHost* socket = LookupSocket(listen_socket_id);
  if (!socket) {
    LOG(ERROR) << "Received P2PHostMsg_AcceptIncomingTcpConnection "
        "for invalid listen_socket_id.";
    return;
  }
  if (LookupSocket(connected_socket_id) != NULL) {
    LOG(ERROR) << "Received P2PHostMsg_AcceptIncomingTcpConnection "
        "for duplicated connected_socket_id.";
    return;
  }

  P2PSocketHost* accepted_connection =
      socket->AcceptIncomingTcpConnection(remote_address, connected_socket_id);
  if (accepted_connection) {
    sockets_[connected_socket_id] = accepted_connection;
  }
}

void P2PSocketDispatcherHost::OnSend(int socket_id,
                                     const net::IPEndPoint& socket_address,
                                     const std::vector<char>& data,
                                     const rtc::PacketOptions& options,
                                     uint64_t packet_id) {
  P2PSocketHost* socket = LookupSocket(socket_id);
  if (!socket) {
    LOG(ERROR) << "Received P2PHostMsg_Send for invalid socket_id.";
    return;
  }

  if (data.size() > kMaximumPacketSize) {
    LOG(ERROR) << "Received P2PHostMsg_Send with a packet that is too big: "
               << data.size();
    Send(new P2PMsg_OnError(socket_id));
    delete socket;
    sockets_.erase(socket_id);
    return;
  }

  socket->Send(socket_address, data, options, packet_id);
}

void P2PSocketDispatcherHost::OnSetOption(int socket_id,
                                          P2PSocketOption option,
                                          int value) {
  P2PSocketHost* socket = LookupSocket(socket_id);
  if (!socket) {
    LOG(ERROR) << "Received P2PHostMsg_SetOption for invalid socket_id.";
    return;
  }

  socket->SetOption(option, value);
}

void P2PSocketDispatcherHost::OnDestroySocket(int socket_id) {
  SocketsMap::iterator it = sockets_.find(socket_id);
  if (it != sockets_.end()) {
    delete it->second;
    sockets_.erase(it);
  } else {
    LOG(ERROR) << "Received P2PHostMsg_DestroySocket for invalid socket_id.";
  }
}

void P2PSocketDispatcherHost::DoGetNetworkList() {
  net::NetworkInterfaceList list;
  if (!net::GetNetworkList(&list, net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
    LOG(ERROR) << "GetNetworkList failed.";
    return;
  }
  default_ipv4_local_address_ = GetDefaultLocalAddress(AF_INET);
  default_ipv6_local_address_ = GetDefaultLocalAddress(AF_INET6);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&P2PSocketDispatcherHost::SendNetworkList, this, list,
                 default_ipv4_local_address_, default_ipv6_local_address_));
}

void P2PSocketDispatcherHost::SendNetworkList(
    const net::NetworkInterfaceList& list,
    const net::IPAddress& default_ipv4_local_address,
    const net::IPAddress& default_ipv6_local_address) {
  Send(new P2PMsg_NetworkListChanged(list, default_ipv4_local_address,
                                     default_ipv6_local_address));
}

net::IPAddress P2PSocketDispatcherHost::GetDefaultLocalAddress(int family) {
  DCHECK(family == AF_INET || family == AF_INET6);

  // Creation and connection of a UDP socket might be janky.
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  std::unique_ptr<net::DatagramClientSocket> socket(
      net::ClientSocketFactory::GetDefaultFactory()->CreateDatagramClientSocket(
          net::DatagramSocket::DEFAULT_BIND, net::RandIntCallback(), NULL,
          net::NetLog::Source()));

  net::IPAddress ip_address;
  if (family == AF_INET) {
    ip_address = net::IPAddress(kPublicIPv4Host);
  } else {
    ip_address = net::IPAddress(kPublicIPv6Host);
  }

  if (socket->Connect(net::IPEndPoint(ip_address, kPublicPort)) != net::OK) {
    return net::IPAddress();
  }

  net::IPEndPoint local_address;
  if (socket->GetLocalAddress(&local_address) != net::OK)
    return net::IPAddress();

  return local_address.address();
}

void P2PSocketDispatcherHost::OnAddressResolved(
    DnsRequest* request,
    const net::IPAddressList& addresses) {
  Send(new P2PMsg_GetHostAddressResult(request->request_id(), addresses));

  dns_requests_.erase(request);
  delete request;
}

void P2PSocketDispatcherHost::StopRtpDumpOnIOThread(bool incoming,
                                                    bool outgoing) {
  if ((dump_incoming_rtp_packet_ && incoming) ||
      (dump_outgoing_rtp_packet_ && outgoing)) {
    if (incoming)
      dump_incoming_rtp_packet_ = false;

    if (outgoing)
      dump_outgoing_rtp_packet_ = false;

    if (!dump_incoming_rtp_packet_ && !dump_outgoing_rtp_packet_)
      packet_callback_.Reset();

    for (SocketsMap::iterator it = sockets_.begin(); it != sockets_.end(); ++it)
      it->second->StopRtpDump(incoming, outgoing);
  }
}

}  // namespace content
