// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_socket.h"

#include <stdlib.h>
#include <string.h>

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "extensions/browser/api/cast_channel/cast_auth_util.h"
#include "extensions/browser/api/cast_channel/cast_framer.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/browser/api/cast_channel/cast_transport.h"
#include "extensions/browser/api/cast_channel/logger.h"
#include "extensions/browser/api/cast_channel/logger_util.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_info.h"

// Helper for logging data with remote host IP and authentication state.
// Assumes |ip_endpoint_| of type net::IPEndPoint and |channel_auth_| of enum
// type ChannelAuthType are available in the current scope.
#define CONNECTION_INFO() \
  "[" << ip_endpoint_.ToString() << ", auth=" << channel_auth_ << "] "
#define VLOG_WITH_CONNECTION(level) VLOG(level) << CONNECTION_INFO()
#define LOG_WITH_CONNECTION(level) LOG(level) << CONNECTION_INFO()

namespace extensions {
static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<api::cast_channel::CastSocket>>> g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<
    ApiResourceManager<api::cast_channel::CastSocket>>*
ApiResourceManager<api::cast_channel::CastSocket>::GetFactoryInstance() {
  return g_factory.Pointer();
}

namespace api {
namespace cast_channel {
namespace {

bool IsTerminalState(proto::ConnectionState state) {
  return state == proto::CONN_STATE_FINISHED ||
         state == proto::CONN_STATE_ERROR || state == proto::CONN_STATE_TIMEOUT;
}

// Cert verifier which blindly accepts all certificates, regardless of validity.
class FakeCertVerifier : public net::CertVerifier {
 public:
  FakeCertVerifier() {}
  ~FakeCertVerifier() override {}

  int Verify(const RequestParams& params,
             net::CRLSet*,
             net::CertVerifyResult* verify_result,
             const net::CompletionCallback&,
             std::unique_ptr<Request>*,
             const net::BoundNetLog&) override {
    verify_result->Reset();
    verify_result->verified_cert = params.certificate();
    return net::OK;
  }
};

}  // namespace

CastSocket::CastSocket(const std::string& owner_extension_id)
    : ApiResource(owner_extension_id) {
}

bool CastSocket::IsPersistent() const {
  return true;
}

CastSocketImpl::CastSocketImpl(const std::string& owner_extension_id,
                               const net::IPEndPoint& ip_endpoint,
                               ChannelAuthType channel_auth,
                               net::NetLog* net_log,
                               const base::TimeDelta& timeout,
                               bool keep_alive,
                               const scoped_refptr<Logger>& logger,
                               uint64_t device_capabilities)
    : CastSocket(owner_extension_id),
      owner_extension_id_(owner_extension_id),
      channel_id_(0),
      ip_endpoint_(ip_endpoint),
      channel_auth_(channel_auth),
      net_log_(net_log),
      keep_alive_(keep_alive),
      logger_(logger),
      connect_timeout_(timeout),
      connect_timeout_timer_(new base::OneShotTimer),
      is_canceled_(false),
      device_capabilities_(device_capabilities),
      audio_only_(false),
      connect_state_(proto::CONN_STATE_START_CONNECT),
      error_state_(CHANNEL_ERROR_NONE),
      ready_state_(READY_STATE_NONE),
      auth_delegate_(nullptr) {
  DCHECK(net_log_);
  DCHECK(channel_auth_ == CHANNEL_AUTH_TYPE_SSL ||
         channel_auth_ == CHANNEL_AUTH_TYPE_SSL_VERIFIED);
  net_log_source_.type = net::NetLog::SOURCE_SOCKET;
  net_log_source_.id = net_log_->NextID();
}

CastSocketImpl::~CastSocketImpl() {
  // Ensure that resources are freed but do not run pending callbacks to avoid
  // any re-entrancy.
  CloseInternal();
}

ReadyState CastSocketImpl::ready_state() const {
  return ready_state_;
}

ChannelError CastSocketImpl::error_state() const {
  return error_state_;
}

const net::IPEndPoint& CastSocketImpl::ip_endpoint() const {
  return ip_endpoint_;
}

int CastSocketImpl::id() const {
  return channel_id_;
}

void CastSocketImpl::set_id(int id) {
  channel_id_ = id;
}

ChannelAuthType CastSocketImpl::channel_auth() const {
  return channel_auth_;
}

bool CastSocketImpl::keep_alive() const {
  return keep_alive_;
}

bool CastSocketImpl::audio_only() const {
  return audio_only_;
}

std::unique_ptr<net::TCPClientSocket> CastSocketImpl::CreateTcpSocket() {
  net::AddressList addresses(ip_endpoint_);
  return std::unique_ptr<net::TCPClientSocket>(
      new net::TCPClientSocket(addresses, nullptr, net_log_, net_log_source_));
  // Options cannot be set on the TCPClientSocket yet, because the
  // underlying platform socket will not be created until Bind()
  // or Connect() is called.
}

std::unique_ptr<net::SSLClientSocket> CastSocketImpl::CreateSslSocket(
    std::unique_ptr<net::StreamSocket> socket) {
  net::SSLConfig ssl_config;
  cert_verifier_ = base::WrapUnique(new FakeCertVerifier);
  transport_security_state_.reset(new net::TransportSecurityState);
  cert_transparency_verifier_.reset(new net::MultiLogCTVerifier());
  ct_policy_enforcer_.reset(new net::CTPolicyEnforcer());

  // Note that |context| fields remain owned by CastSocketImpl.
  net::SSLClientSocketContext context;
  context.cert_verifier = cert_verifier_.get();
  context.transport_security_state = transport_security_state_.get();
  context.cert_transparency_verifier = cert_transparency_verifier_.get();
  context.ct_policy_enforcer = ct_policy_enforcer_.get();

  std::unique_ptr<net::ClientSocketHandle> connection(
      new net::ClientSocketHandle);
  connection->SetSocket(std::move(socket));
  net::HostPortPair host_and_port = net::HostPortPair::FromIPEndPoint(
      ip_endpoint_);

  return net::ClientSocketFactory::GetDefaultFactory()->CreateSSLClientSocket(
      std::move(connection), host_and_port, ssl_config, context);
}

scoped_refptr<net::X509Certificate> CastSocketImpl::ExtractPeerCert() {
  net::SSLInfo ssl_info;
  if (!socket_->GetSSLInfo(&ssl_info) || !ssl_info.cert.get()) {
    return nullptr;
  }

  logger_->LogSocketEvent(channel_id_, proto::SSL_INFO_OBTAINED);

  return ssl_info.cert;
}

bool CastSocketImpl::VerifyChannelPolicy(const AuthResult& result) {
  audio_only_ = (result.channel_policies & AuthResult::POLICY_AUDIO_ONLY) != 0;
  if (audio_only_ &&
      (device_capabilities_ & CastDeviceCapability::VIDEO_OUT) != 0) {
    LOG_WITH_CONNECTION(ERROR)
        << "Audio only channel policy enforced for video out capable device";
    logger_->LogSocketEventWithDetails(
        channel_id_, proto::CHANNEL_POLICY_ENFORCED, std::string());
    return false;
  }
  return true;
}

bool CastSocketImpl::VerifyChallengeReply() {
  DCHECK(peer_cert_);
  AuthResult result =
      AuthenticateChallengeReply(*challenge_reply_, *peer_cert_);
  logger_->LogSocketChallengeReplyEvent(channel_id_, result);
  if (result.success()) {
    VLOG(1) << result.error_message;
    if (!VerifyChannelPolicy(result)) {
      return false;
    }
  }
  return result.success();
}

void CastSocketImpl::SetTransportForTesting(
    std::unique_ptr<CastTransport> transport) {
  transport_ = std::move(transport);
}

void CastSocketImpl::Connect(std::unique_ptr<CastTransport::Delegate> delegate,
                             base::Callback<void(ChannelError)> callback) {
  DCHECK(CalledOnValidThread());
  VLOG_WITH_CONNECTION(1) << "Connect readyState = " << ready_state_;
  DCHECK_EQ(proto::CONN_STATE_START_CONNECT, connect_state_);

  delegate_ = std::move(delegate);

  if (ready_state_ != READY_STATE_NONE) {
    logger_->LogSocketEventWithDetails(
        channel_id_, proto::CONNECT_FAILED, "ReadyState not NONE");
    callback.Run(CHANNEL_ERROR_CONNECT_ERROR);
    return;
  }

  connect_callback_ = callback;
  SetReadyState(READY_STATE_CONNECTING);
  SetConnectState(proto::CONN_STATE_TCP_CONNECT);

  // Set up connection timeout.
  if (connect_timeout_.InMicroseconds() > 0) {
    DCHECK(connect_timeout_callback_.IsCancelled());
    connect_timeout_callback_.Reset(
        base::Bind(&CastSocketImpl::OnConnectTimeout, base::Unretained(this)));
    GetTimer()->Start(FROM_HERE,
                      connect_timeout_,
                      connect_timeout_callback_.callback());
  }

  DoConnectLoop(net::OK);
}

CastTransport* CastSocketImpl::transport() const {
  return transport_.get();
}

void CastSocketImpl::OnConnectTimeout() {
  DCHECK(CalledOnValidThread());
  // Stop all pending connection setup tasks and report back to the client.
  is_canceled_ = true;
  logger_->LogSocketEvent(channel_id_, proto::CONNECT_TIMED_OUT);
  VLOG_WITH_CONNECTION(1) << "Timeout while establishing a connection.";
  SetErrorState(CHANNEL_ERROR_CONNECT_TIMEOUT);
  DoConnectCallback();
}

void CastSocketImpl::PostTaskToStartConnectLoop(int result) {
  DCHECK(CalledOnValidThread());
  DCHECK(connect_loop_callback_.IsCancelled());
  connect_loop_callback_.Reset(base::Bind(&CastSocketImpl::DoConnectLoop,
                                          base::Unretained(this), result));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, connect_loop_callback_.callback());
}

// This method performs the state machine transitions for connection flow.
// There are two entry points to this method:
// 1. Connect method: this starts the flow
// 2. Callback from network operations that finish asynchronously.
void CastSocketImpl::DoConnectLoop(int result) {
  connect_loop_callback_.Cancel();
  if (is_canceled_) {
    LOG_WITH_CONNECTION(ERROR) << "CANCELLED - Aborting DoConnectLoop.";
    return;
  }

  // Network operations can either finish synchronously or asynchronously.
  // This method executes the state machine transitions in a loop so that
  // correct state transitions happen even when network operations finish
  // synchronously.
  int rv = result;
  do {
    proto::ConnectionState state = connect_state_;
    connect_state_ = proto::CONN_STATE_UNKNOWN;
    switch (state) {
      case proto::CONN_STATE_TCP_CONNECT:
        rv = DoTcpConnect();
        break;
      case proto::CONN_STATE_TCP_CONNECT_COMPLETE:
        rv = DoTcpConnectComplete(rv);
        break;
      case proto::CONN_STATE_SSL_CONNECT:
        DCHECK_EQ(net::OK, rv);
        rv = DoSslConnect();
        break;
      case proto::CONN_STATE_SSL_CONNECT_COMPLETE:
        rv = DoSslConnectComplete(rv);
        break;
      case proto::CONN_STATE_AUTH_CHALLENGE_SEND:
        rv = DoAuthChallengeSend();
        break;
      case proto::CONN_STATE_AUTH_CHALLENGE_SEND_COMPLETE:
        rv = DoAuthChallengeSendComplete(rv);
        break;
      case proto::CONN_STATE_AUTH_CHALLENGE_REPLY_COMPLETE:
        rv = DoAuthChallengeReplyComplete(rv);
        DCHECK(IsTerminalState(connect_state_));
        break;
      default:
        NOTREACHED() << "Unknown state in connect flow: " << state;
        SetConnectState(proto::CONN_STATE_FINISHED);
        SetErrorState(CHANNEL_ERROR_UNKNOWN);
        DoConnectCallback();
        return;
    }
  } while (rv != net::ERR_IO_PENDING && !IsTerminalState(connect_state_));
  // Exit the state machine if an asynchronous network operation is pending
  // or if the state machine is in the terminal "finished" state.

  if (IsTerminalState(connect_state_)) {
    DCHECK_NE(rv, net::ERR_IO_PENDING);
    logger_->LogSocketConnectState(channel_id_, connect_state_);
    GetTimer()->Stop();
    DoConnectCallback();
  } else {
    DCHECK_EQ(rv, net::ERR_IO_PENDING);
  }
}

int CastSocketImpl::DoTcpConnect() {
  DCHECK(connect_loop_callback_.IsCancelled());
  VLOG_WITH_CONNECTION(1) << "DoTcpConnect";
  SetConnectState(proto::CONN_STATE_TCP_CONNECT_COMPLETE);
  tcp_socket_ = CreateTcpSocket();

  int rv = tcp_socket_->Connect(
      base::Bind(&CastSocketImpl::DoConnectLoop, base::Unretained(this)));
  logger_->LogSocketEventWithRv(channel_id_, proto::TCP_SOCKET_CONNECT, rv);
  return rv;
}

int CastSocketImpl::DoTcpConnectComplete(int connect_result) {
  VLOG_WITH_CONNECTION(1) << "DoTcpConnectComplete: " << connect_result;
  logger_->LogSocketEventWithRv(channel_id_, proto::TCP_SOCKET_CONNECT_COMPLETE,
                                connect_result);
  if (connect_result == net::OK) {
    SetConnectState(proto::CONN_STATE_SSL_CONNECT);
  } else if (connect_result == net::ERR_CONNECTION_TIMED_OUT) {
    SetConnectState(proto::CONN_STATE_FINISHED);
    SetErrorState(CHANNEL_ERROR_CONNECT_TIMEOUT);
  } else {
    SetConnectState(proto::CONN_STATE_FINISHED);
    SetErrorState(CHANNEL_ERROR_CONNECT_ERROR);
  }
  return connect_result;
}

int CastSocketImpl::DoSslConnect() {
  DCHECK(connect_loop_callback_.IsCancelled());
  VLOG_WITH_CONNECTION(1) << "DoSslConnect";
  SetConnectState(proto::CONN_STATE_SSL_CONNECT_COMPLETE);
  socket_ = CreateSslSocket(std::move(tcp_socket_));

  int rv = socket_->Connect(
      base::Bind(&CastSocketImpl::DoConnectLoop, base::Unretained(this)));
  logger_->LogSocketEventWithRv(channel_id_, proto::SSL_SOCKET_CONNECT, rv);
  return rv;
}

int CastSocketImpl::DoSslConnectComplete(int result) {
  logger_->LogSocketEventWithRv(channel_id_, proto::SSL_SOCKET_CONNECT_COMPLETE,
                                result);
  VLOG_WITH_CONNECTION(1) << "DoSslConnectComplete: " << result;
  if (result == net::OK) {
    peer_cert_ = ExtractPeerCert();

    if (!peer_cert_) {
      LOG_WITH_CONNECTION(WARNING) << "Could not extract peer cert.";
      SetConnectState(proto::CONN_STATE_FINISHED);
      SetErrorState(CHANNEL_ERROR_AUTHENTICATION_ERROR);
      return net::ERR_CERT_INVALID;
    }

    // SSL connection succeeded.
    if (!transport_.get()) {
      // Create a channel transport if one wasn't already set (e.g. by test
      // code).
      transport_.reset(new CastTransportImpl(this->socket_.get(), channel_id_,
                                             ip_endpoint_, channel_auth_,
                                             logger_));
    }
    auth_delegate_ = new AuthTransportDelegate(this);
    transport_->SetReadDelegate(base::WrapUnique(auth_delegate_));
    if (channel_auth_ == CHANNEL_AUTH_TYPE_SSL_VERIFIED) {
      // Additionally verify the connection with a handshake.
      SetConnectState(proto::CONN_STATE_AUTH_CHALLENGE_SEND);
    } else {
      SetConnectState(proto::CONN_STATE_FINISHED);
      transport_->Start();
    }
  } else if (result == net::ERR_CONNECTION_TIMED_OUT) {
    SetConnectState(proto::CONN_STATE_FINISHED);
    SetErrorState(CHANNEL_ERROR_CONNECT_TIMEOUT);
  } else {
    SetConnectState(proto::CONN_STATE_FINISHED);
    SetErrorState(CHANNEL_ERROR_AUTHENTICATION_ERROR);
  }
  return result;
}

int CastSocketImpl::DoAuthChallengeSend() {
  VLOG_WITH_CONNECTION(1) << "DoAuthChallengeSend";
  SetConnectState(proto::CONN_STATE_AUTH_CHALLENGE_SEND_COMPLETE);

  CastMessage challenge_message;
  CreateAuthChallengeMessage(&challenge_message);
  VLOG_WITH_CONNECTION(1) << "Sending challenge: "
                          << CastMessageToString(challenge_message);

  transport_->SendMessage(
      challenge_message,
      base::Bind(&CastSocketImpl::DoConnectLoop, base::Unretained(this)));

  // Always return IO_PENDING since the result is always asynchronous.
  return net::ERR_IO_PENDING;
}

int CastSocketImpl::DoAuthChallengeSendComplete(int result) {
  VLOG_WITH_CONNECTION(1) << "DoAuthChallengeSendComplete: " << result;
  if (result < 0) {
    SetConnectState(proto::CONN_STATE_ERROR);
    SetErrorState(CHANNEL_ERROR_SOCKET_ERROR);
    logger_->LogSocketEventWithRv(channel_id_,
                                  proto::SEND_AUTH_CHALLENGE_FAILED, result);
    return result;
  }
  transport_->Start();
  SetConnectState(proto::CONN_STATE_AUTH_CHALLENGE_REPLY_COMPLETE);
  return net::ERR_IO_PENDING;
}

CastSocketImpl::AuthTransportDelegate::AuthTransportDelegate(
    CastSocketImpl* socket)
    : socket_(socket), error_state_(CHANNEL_ERROR_NONE) {
  DCHECK(socket);
}

ChannelError CastSocketImpl::AuthTransportDelegate::error_state() const {
  return error_state_;
}

LastErrors CastSocketImpl::AuthTransportDelegate::last_errors() const {
  return last_errors_;
}

void CastSocketImpl::AuthTransportDelegate::OnError(ChannelError error_state) {
  error_state_ = error_state;
  socket_->PostTaskToStartConnectLoop(net::ERR_CONNECTION_FAILED);
}

void CastSocketImpl::AuthTransportDelegate::OnMessage(
    const CastMessage& message) {
  if (!IsAuthMessage(message)) {
    error_state_ = CHANNEL_ERROR_TRANSPORT_ERROR;
    socket_->logger_->LogSocketEvent(socket_->channel_id_,
                                     proto::AUTH_CHALLENGE_REPLY_INVALID);
    socket_->PostTaskToStartConnectLoop(net::ERR_INVALID_RESPONSE);
  } else {
    socket_->challenge_reply_.reset(new CastMessage(message));
    socket_->logger_->LogSocketEvent(socket_->channel_id_,
                                     proto::RECEIVED_CHALLENGE_REPLY);
    socket_->PostTaskToStartConnectLoop(net::OK);
  }
}

void CastSocketImpl::AuthTransportDelegate::Start() {
}

int CastSocketImpl::DoAuthChallengeReplyComplete(int result) {
  VLOG_WITH_CONNECTION(1) << "DoAuthChallengeReplyComplete: " << result;

  if (auth_delegate_->error_state() != CHANNEL_ERROR_NONE) {
    SetErrorState(auth_delegate_->error_state());
    SetConnectState(proto::CONN_STATE_ERROR);
    return net::ERR_CONNECTION_FAILED;
  }
  auth_delegate_ = nullptr;

  if (result < 0) {
    SetConnectState(proto::CONN_STATE_ERROR);
    return result;
  }

  if (!VerifyChallengeReply()) {
    SetErrorState(CHANNEL_ERROR_AUTHENTICATION_ERROR);
    SetConnectState(proto::CONN_STATE_ERROR);
    return net::ERR_CONNECTION_FAILED;
  }
  VLOG_WITH_CONNECTION(1) << "Auth challenge verification succeeded";

  SetConnectState(proto::CONN_STATE_FINISHED);
  return net::OK;
}

void CastSocketImpl::DoConnectCallback() {
  VLOG(1) << "DoConnectCallback (error_state = " << error_state_ << ")";
  if (connect_callback_.is_null()) {
    DLOG(FATAL) << "Connection callback invoked multiple times.";
    return;
  }

  if (error_state_ == CHANNEL_ERROR_NONE) {
    SetReadyState(READY_STATE_OPEN);
    transport_->SetReadDelegate(std::move(delegate_));
  } else {
    CloseInternal();
  }

  base::ResetAndReturn(&connect_callback_).Run(error_state_);
}

void CastSocketImpl::Close(const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  CloseInternal();
  // Run this callback last.  It may delete the socket.
  callback.Run(net::OK);
}

void CastSocketImpl::CloseInternal() {
  // TODO(mfoltz): Enforce this when CastChannelAPITest is rewritten to create
  // and free sockets on the same thread.  crbug.com/398242
  DCHECK(CalledOnValidThread());
  if (ready_state_ == READY_STATE_CLOSED) {
    return;
  }

  VLOG_WITH_CONNECTION(1) << "Close ReadyState = " << ready_state_;
  transport_.reset();
  tcp_socket_.reset();
  socket_.reset();
  transport_security_state_.reset();
  if (GetTimer()) {
    GetTimer()->Stop();
  }

  // Cancel callbacks that we queued ourselves to re-enter the connect or read
  // loops.
  connect_loop_callback_.Cancel();
  send_auth_challenge_callback_.Cancel();
  connect_timeout_callback_.Cancel();
  SetReadyState(READY_STATE_CLOSED);
  logger_->LogSocketEvent(channel_id_, proto::SOCKET_CLOSED);
}

bool CastSocketImpl::CalledOnValidThread() const {
  return thread_checker_.CalledOnValidThread();
}

base::Timer* CastSocketImpl::GetTimer() {
  return connect_timeout_timer_.get();
}

void CastSocketImpl::SetConnectState(proto::ConnectionState connect_state) {
  if (connect_state_ != connect_state) {
    connect_state_ = connect_state;
    logger_->LogSocketConnectState(channel_id_, connect_state_);
  }
}

void CastSocketImpl::SetReadyState(ReadyState ready_state) {
  if (ready_state_ != ready_state) {
    ready_state_ = ready_state;
    logger_->LogSocketReadyState(channel_id_, ReadyStateToProto(ready_state_));
  }
}

void CastSocketImpl::SetErrorState(ChannelError error_state) {
  VLOG_WITH_CONNECTION(1) << "SetErrorState " << error_state;
  DCHECK_EQ(CHANNEL_ERROR_NONE, error_state_);
  error_state_ = error_state;
  logger_->LogSocketErrorState(channel_id_, ErrorStateToProto(error_state_));
  delegate_->OnError(error_state_);
}
}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
#undef VLOG_WITH_CONNECTION
