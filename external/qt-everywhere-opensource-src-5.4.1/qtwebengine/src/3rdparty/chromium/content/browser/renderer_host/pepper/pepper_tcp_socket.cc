// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_tcp_socket.h"

#include <string.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/host_resolver.h"
#include "net/dns/single_request_host_resolver.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "ppapi/host/error_conversion.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"
#include "ppapi/shared_impl/socket_option_data.h"
#include "ppapi/shared_impl/tcp_socket_shared.h"

using ppapi::host::NetErrorToPepperError;
using ppapi::NetAddressPrivateImpl;

namespace content {

PepperTCPSocket::PepperTCPSocket(PepperMessageFilter* manager,
                                 int32 routing_id,
                                 uint32 plugin_dispatcher_id,
                                 uint32 socket_id,
                                 bool private_api)
    : manager_(manager),
      routing_id_(routing_id),
      plugin_dispatcher_id_(plugin_dispatcher_id),
      socket_id_(socket_id),
      private_api_(private_api),
      connection_state_(BEFORE_CONNECT),
      end_of_file_reached_(false) {
  DCHECK(manager);
}

PepperTCPSocket::PepperTCPSocket(PepperMessageFilter* manager,
                                 int32 routing_id,
                                 uint32 plugin_dispatcher_id,
                                 uint32 socket_id,
                                 net::StreamSocket* socket,
                                 bool private_api)
    : manager_(manager),
      routing_id_(routing_id),
      plugin_dispatcher_id_(plugin_dispatcher_id),
      socket_id_(socket_id),
      private_api_(private_api),
      connection_state_(CONNECTED),
      end_of_file_reached_(false),
      socket_(socket) {
  DCHECK(manager);
}

PepperTCPSocket::~PepperTCPSocket() {
  // Make sure no further callbacks from socket_.
  if (socket_)
    socket_->Disconnect();
}

void PepperTCPSocket::Connect(const std::string& host, uint16_t port) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (connection_state_ != BEFORE_CONNECT) {
    SendConnectACKError(PP_ERROR_FAILED);
    return;
  }

  connection_state_ = CONNECT_IN_PROGRESS;
  net::HostResolver::RequestInfo request_info(net::HostPortPair(host, port));
  resolver_.reset(
      new net::SingleRequestHostResolver(manager_->GetHostResolver()));
  int net_result = resolver_->Resolve(
      request_info,
      net::DEFAULT_PRIORITY,
      &address_list_,
      base::Bind(&PepperTCPSocket::OnResolveCompleted, base::Unretained(this)),
      net::BoundNetLog());
  if (net_result != net::ERR_IO_PENDING)
    OnResolveCompleted(net_result);
}

void PepperTCPSocket::ConnectWithNetAddress(
    const PP_NetAddress_Private& net_addr) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (connection_state_ != BEFORE_CONNECT) {
    SendConnectACKError(PP_ERROR_FAILED);
    return;
  }

  net::IPAddressNumber address;
  int port;
  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(
          net_addr, &address, &port)) {
    SendConnectACKError(PP_ERROR_ADDRESS_INVALID);
    return;
  }

  // Copy the single IPEndPoint to address_list_.
  address_list_.clear();
  address_list_.push_back(net::IPEndPoint(address, port));
  connection_state_ = CONNECT_IN_PROGRESS;
  StartConnect(address_list_);
}

void PepperTCPSocket::SSLHandshake(
    const std::string& server_name,
    uint16_t server_port,
    const std::vector<std::vector<char> >& trusted_certs,
    const std::vector<std::vector<char> >& untrusted_certs) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Allow to do SSL handshake only if currently the socket has been connected
  // and there isn't pending read or write.
  // IsConnected() includes the state that SSL handshake has been finished and
  // therefore isn't suitable here.
  if (connection_state_ != CONNECTED || read_buffer_.get() ||
      write_buffer_base_.get() || write_buffer_.get()) {
    SendSSLHandshakeACK(false);
    return;
  }

  connection_state_ = SSL_HANDSHAKE_IN_PROGRESS;
  // TODO(raymes,rsleevi): Use trusted/untrusted certificates when connecting.

  scoped_ptr<net::ClientSocketHandle> handle(new net::ClientSocketHandle());
  handle->SetSocket(socket_.Pass());
  net::ClientSocketFactory* factory =
      net::ClientSocketFactory::GetDefaultFactory();
  net::HostPortPair host_port_pair(server_name, server_port);
  net::SSLClientSocketContext ssl_context;
  ssl_context.cert_verifier = manager_->GetCertVerifier();
  ssl_context.transport_security_state = manager_->GetTransportSecurityState();
  socket_ = factory->CreateSSLClientSocket(
      handle.Pass(), host_port_pair, manager_->ssl_config(), ssl_context);
  if (!socket_) {
    LOG(WARNING) << "Failed to create an SSL client socket.";
    OnSSLHandshakeCompleted(net::ERR_UNEXPECTED);
    return;
  }

  int net_result = socket_->Connect(base::Bind(
      &PepperTCPSocket::OnSSLHandshakeCompleted, base::Unretained(this)));
  if (net_result != net::ERR_IO_PENDING)
    OnSSLHandshakeCompleted(net_result);
}

void PepperTCPSocket::Read(int32 bytes_to_read) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsConnected() || end_of_file_reached_) {
    SendReadACKError(PP_ERROR_FAILED);
    return;
  }

  if (read_buffer_.get()) {
    SendReadACKError(PP_ERROR_INPROGRESS);
    return;
  }

  if (bytes_to_read <= 0 ||
      bytes_to_read > ppapi::TCPSocketShared::kMaxReadSize) {
    SendReadACKError(PP_ERROR_BADARGUMENT);
    return;
  }

  read_buffer_ = new net::IOBuffer(bytes_to_read);
  int net_result = socket_->Read(
      read_buffer_.get(),
      bytes_to_read,
      base::Bind(&PepperTCPSocket::OnReadCompleted, base::Unretained(this)));
  if (net_result != net::ERR_IO_PENDING)
    OnReadCompleted(net_result);
}

void PepperTCPSocket::Write(const std::string& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsConnected()) {
    SendWriteACKError(PP_ERROR_FAILED);
    return;
  }

  if (write_buffer_base_.get() || write_buffer_.get()) {
    SendWriteACKError(PP_ERROR_INPROGRESS);
    return;
  }

  size_t data_size = data.size();
  if (data_size == 0 ||
      data_size > static_cast<size_t>(ppapi::TCPSocketShared::kMaxWriteSize)) {
    SendWriteACKError(PP_ERROR_BADARGUMENT);
    return;
  }

  write_buffer_base_ = new net::IOBuffer(data_size);
  memcpy(write_buffer_base_->data(), data.data(), data_size);
  write_buffer_ =
      new net::DrainableIOBuffer(write_buffer_base_.get(), data_size);
  DoWrite();
}

void PepperTCPSocket::SetOption(PP_TCPSocket_Option name,
                                const ppapi::SocketOptionData& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsConnected() || IsSsl()) {
    SendSetOptionACK(PP_ERROR_FAILED);
    return;
  }

  net::TCPClientSocket* tcp_socket =
      static_cast<net::TCPClientSocket*>(socket_.get());
  DCHECK(tcp_socket);

  switch (name) {
    case PP_TCPSOCKET_OPTION_NO_DELAY: {
      bool boolean_value = false;
      if (!value.GetBool(&boolean_value)) {
        SendSetOptionACK(PP_ERROR_BADARGUMENT);
        return;
      }

      SendSetOptionACK(tcp_socket->SetNoDelay(boolean_value) ? PP_OK
                                                             : PP_ERROR_FAILED);
      return;
    }
    case PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE:
    case PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE: {
      int32_t integer_value = 0;
      if (!value.GetInt32(&integer_value) || integer_value <= 0) {
        SendSetOptionACK(PP_ERROR_BADARGUMENT);
        return;
      }

      int net_result = net::OK;
      if (name == PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE) {
        if (integer_value > ppapi::TCPSocketShared::kMaxSendBufferSize) {
          SendSetOptionACK(PP_ERROR_BADARGUMENT);
          return;
        }
        net_result = tcp_socket->SetSendBufferSize(integer_value);
      } else {
        if (integer_value > ppapi::TCPSocketShared::kMaxReceiveBufferSize) {
          SendSetOptionACK(PP_ERROR_BADARGUMENT);
          return;
        }
        net_result = tcp_socket->SetReceiveBufferSize(integer_value);
      }
      // TODO(wtc): Add error mapping.
      SendSetOptionACK((net_result == net::OK) ? PP_OK : PP_ERROR_FAILED);
      return;
    }
    default: {
      NOTREACHED();
      SendSetOptionACK(PP_ERROR_BADARGUMENT);
      return;
    }
  }
}

void PepperTCPSocket::StartConnect(const net::AddressList& addresses) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS);

  socket_.reset(
      new net::TCPClientSocket(addresses, NULL, net::NetLog::Source()));
  int net_result = socket_->Connect(
      base::Bind(&PepperTCPSocket::OnConnectCompleted, base::Unretained(this)));
  if (net_result != net::ERR_IO_PENDING)
    OnConnectCompleted(net_result);
}

void PepperTCPSocket::SendConnectACKError(int32_t error) {
  manager_->Send(new PpapiMsg_PPBTCPSocket_ConnectACK(
      routing_id_,
      plugin_dispatcher_id_,
      socket_id_,
      error,
      NetAddressPrivateImpl::kInvalidNetAddress,
      NetAddressPrivateImpl::kInvalidNetAddress));
}

// static
bool PepperTCPSocket::GetCertificateFields(
    const net::X509Certificate& cert,
    ppapi::PPB_X509Certificate_Fields* fields) {
  const net::CertPrincipal& issuer = cert.issuer();
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_COMMON_NAME,
                   new base::StringValue(issuer.common_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_LOCALITY_NAME,
                   new base::StringValue(issuer.locality_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_STATE_OR_PROVINCE_NAME,
                   new base::StringValue(issuer.state_or_province_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_COUNTRY_NAME,
                   new base::StringValue(issuer.country_name));
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_ISSUER_ORGANIZATION_NAME,
      new base::StringValue(JoinString(issuer.organization_names, '\n')));
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_ISSUER_ORGANIZATION_UNIT_NAME,
      new base::StringValue(JoinString(issuer.organization_unit_names, '\n')));

  const net::CertPrincipal& subject = cert.subject();
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_COMMON_NAME,
                   new base::StringValue(subject.common_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_LOCALITY_NAME,
                   new base::StringValue(subject.locality_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_STATE_OR_PROVINCE_NAME,
                   new base::StringValue(subject.state_or_province_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_COUNTRY_NAME,
                   new base::StringValue(subject.country_name));
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_SUBJECT_ORGANIZATION_NAME,
      new base::StringValue(JoinString(subject.organization_names, '\n')));
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_SUBJECT_ORGANIZATION_UNIT_NAME,
      new base::StringValue(JoinString(subject.organization_unit_names, '\n')));

  const std::string& serial_number = cert.serial_number();
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SERIAL_NUMBER,
                   base::BinaryValue::CreateWithCopiedBuffer(
                       serial_number.data(), serial_number.length()));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_VALIDITY_NOT_BEFORE,
                   new base::FundamentalValue(cert.valid_start().ToDoubleT()));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_VALIDITY_NOT_AFTER,
                   new base::FundamentalValue(cert.valid_expiry().ToDoubleT()));
  std::string der;
  net::X509Certificate::GetDEREncoded(cert.os_cert_handle(), &der);
  fields->SetField(
      PP_X509CERTIFICATE_PRIVATE_RAW,
      base::BinaryValue::CreateWithCopiedBuffer(der.data(), der.length()));
  return true;
}

// static
bool PepperTCPSocket::GetCertificateFields(
    const char* der,
    uint32_t length,
    ppapi::PPB_X509Certificate_Fields* fields) {
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromBytes(der, length);
  if (!cert.get())
    return false;
  return GetCertificateFields(*cert.get(), fields);
}

void PepperTCPSocket::SendReadACKError(int32_t error) {
  manager_->Send(new PpapiMsg_PPBTCPSocket_ReadACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, error, std::string()));
}

void PepperTCPSocket::SendWriteACKError(int32_t error) {
  DCHECK_GT(0, error);
  manager_->Send(new PpapiMsg_PPBTCPSocket_WriteACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, error));
}

void PepperTCPSocket::SendSSLHandshakeACK(bool succeeded) {
  ppapi::PPB_X509Certificate_Fields certificate_fields;
  if (succeeded) {
    // Our socket is guaranteed to be an SSL socket if we get here.
    net::SSLClientSocket* ssl_socket =
        static_cast<net::SSLClientSocket*>(socket_.get());
    net::SSLInfo ssl_info;
    ssl_socket->GetSSLInfo(&ssl_info);
    if (ssl_info.cert.get())
      GetCertificateFields(*ssl_info.cert.get(), &certificate_fields);
  }
  manager_->Send(
      new PpapiMsg_PPBTCPSocket_SSLHandshakeACK(routing_id_,
                                                plugin_dispatcher_id_,
                                                socket_id_,
                                                succeeded,
                                                certificate_fields));
}

void PepperTCPSocket::SendSetOptionACK(int32_t result) {
  manager_->Send(new PpapiMsg_PPBTCPSocket_SetOptionACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, result));
}

void PepperTCPSocket::OnResolveCompleted(int net_result) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS);

  if (net_result != net::OK) {
    SendConnectACKError(NetErrorToPepperError(net_result));
    connection_state_ = BEFORE_CONNECT;
    return;
  }

  StartConnect(address_list_);
}

void PepperTCPSocket::OnConnectCompleted(int net_result) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS && socket_.get());

  int32_t pp_result = NetErrorToPepperError(net_result);
  do {
    if (pp_result != PP_OK)
      break;

    net::IPEndPoint ip_end_point_local;
    net::IPEndPoint ip_end_point_remote;
    pp_result =
        NetErrorToPepperError(socket_->GetLocalAddress(&ip_end_point_local));
    if (pp_result != PP_OK)
      break;
    pp_result =
        NetErrorToPepperError(socket_->GetPeerAddress(&ip_end_point_remote));
    if (pp_result != PP_OK)
      break;

    PP_NetAddress_Private local_addr =
        NetAddressPrivateImpl::kInvalidNetAddress;
    PP_NetAddress_Private remote_addr =
        NetAddressPrivateImpl::kInvalidNetAddress;
    if (!NetAddressPrivateImpl::IPEndPointToNetAddress(
            ip_end_point_local.address(),
            ip_end_point_local.port(),
            &local_addr) ||
        !NetAddressPrivateImpl::IPEndPointToNetAddress(
            ip_end_point_remote.address(),
            ip_end_point_remote.port(),
            &remote_addr)) {
      pp_result = PP_ERROR_ADDRESS_INVALID;
      break;
    }

    manager_->Send(new PpapiMsg_PPBTCPSocket_ConnectACK(routing_id_,
                                                        plugin_dispatcher_id_,
                                                        socket_id_,
                                                        PP_OK,
                                                        local_addr,
                                                        remote_addr));
    connection_state_ = CONNECTED;
    return;
  } while (false);

  SendConnectACKError(pp_result);
  connection_state_ = BEFORE_CONNECT;
}

void PepperTCPSocket::OnSSLHandshakeCompleted(int net_result) {
  DCHECK(connection_state_ == SSL_HANDSHAKE_IN_PROGRESS);

  bool succeeded = net_result == net::OK;
  SendSSLHandshakeACK(succeeded);
  connection_state_ = succeeded ? SSL_CONNECTED : SSL_HANDSHAKE_FAILED;
}

void PepperTCPSocket::OnReadCompleted(int net_result) {
  DCHECK(read_buffer_.get());

  if (net_result > 0) {
    manager_->Send(new PpapiMsg_PPBTCPSocket_ReadACK(
        routing_id_,
        plugin_dispatcher_id_,
        socket_id_,
        PP_OK,
        std::string(read_buffer_->data(), net_result)));
  } else if (net_result == 0) {
    end_of_file_reached_ = true;
    manager_->Send(new PpapiMsg_PPBTCPSocket_ReadACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, PP_OK, std::string()));
  } else {
    SendReadACKError(NetErrorToPepperError(net_result));
  }
  read_buffer_ = NULL;
}

void PepperTCPSocket::OnWriteCompleted(int net_result) {
  DCHECK(write_buffer_base_.get());
  DCHECK(write_buffer_.get());

  // Note: For partial writes of 0 bytes, don't continue writing to avoid a
  // likely infinite loop.
  if (net_result > 0) {
    write_buffer_->DidConsume(net_result);
    if (write_buffer_->BytesRemaining() > 0) {
      DoWrite();
      return;
    }
  }

  if (net_result >= 0) {
    manager_->Send(
        new PpapiMsg_PPBTCPSocket_WriteACK(routing_id_,
                                           plugin_dispatcher_id_,
                                           socket_id_,
                                           write_buffer_->BytesConsumed()));
  } else {
    SendWriteACKError(NetErrorToPepperError(net_result));
  }

  write_buffer_ = NULL;
  write_buffer_base_ = NULL;
}

bool PepperTCPSocket::IsConnected() const {
  return connection_state_ == CONNECTED || connection_state_ == SSL_CONNECTED;
}

bool PepperTCPSocket::IsSsl() const {
  return connection_state_ == SSL_HANDSHAKE_IN_PROGRESS ||
         connection_state_ == SSL_CONNECTED ||
         connection_state_ == SSL_HANDSHAKE_FAILED;
}

void PepperTCPSocket::DoWrite() {
  DCHECK(write_buffer_base_.get());
  DCHECK(write_buffer_.get());
  DCHECK_GT(write_buffer_->BytesRemaining(), 0);

  int net_result = socket_->Write(
      write_buffer_.get(),
      write_buffer_->BytesRemaining(),
      base::Bind(&PepperTCPSocket::OnWriteCompleted, base::Unretained(this)));
  if (net_result != net::ERR_IO_PENDING)
    OnWriteCompleted(net_result);
}

}  // namespace content
