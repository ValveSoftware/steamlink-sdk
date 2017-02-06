// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/engine_authentication_handler.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/timer/timer.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/logging.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/protocol_version.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_transport.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_error_observer.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"

namespace blimp {

namespace {
// Expect Client to send the StartConnection within ten seconds of becoming
// connected.
const int kAuthTimeoutDurationInSeconds = 10;

// Authenticates one connection. It deletes itself when
//   * the connection is authenticated and passed to |connection_handler|.
//   * the connection gets into an error state.
//   * the auth message does not arrive within a reasonable time.
class Authenticator : public ConnectionErrorObserver,
                      public BlimpMessageProcessor {
 public:
  explicit Authenticator(std::unique_ptr<BlimpConnection> connection,
                         base::WeakPtr<ConnectionHandler> connection_handler,
                         const std::string& client_token);
  ~Authenticator() override;

 private:
  // Processes authentication result and deletes |this|.
  void OnConnectionAuthenticated(bool authenticated);

  // Handles timeout waiting for auth message, and deletes |this|.
  void OnAuthenticationTimeout();

  // ConnectionErrorObserver implementation.
  void OnConnectionError(int error) override;

  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

  // The connection to be authenticated.
  std::unique_ptr<BlimpConnection> connection_;

  // Handler to pass successfully authenticated connections to.
  base::WeakPtr<ConnectionHandler> connection_handler_;

  // Used to authenticate incoming connection.
  const std::string client_token_;

  // A timer to fail authentication on timeout.
  base::OneShotTimer timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(Authenticator);
};

Authenticator::Authenticator(
    std::unique_ptr<BlimpConnection> connection,
    base::WeakPtr<ConnectionHandler> connection_handler,
    const std::string& client_token)
    : connection_(std::move(connection)),
      connection_handler_(connection_handler),
      client_token_(client_token) {
  DVLOG(1) << "Authenticator object created.";

  // Observe for errors that might occur during the authentication phase.
  connection_->AddConnectionErrorObserver(this);
  connection_->SetIncomingMessageProcessor(this);
  timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kAuthTimeoutDurationInSeconds),
      this, &Authenticator::OnAuthenticationTimeout);
}

Authenticator::~Authenticator() {}

void Authenticator::OnConnectionAuthenticated(bool authenticated) {
  DVLOG(1) << "OnConnectionAuthenticated result=" << authenticated;

  if (authenticated && connection_handler_) {
    // Authentication is successful. Stop observing connection errors.
    connection_->RemoveConnectionErrorObserver(this);
    connection_handler_->HandleConnection(std::move(connection_));
  }

  delete this;
}

void Authenticator::OnAuthenticationTimeout() {
  DVLOG(1) << "Connection authentication timeout";
  OnConnectionAuthenticated(false);
}

void Authenticator::OnConnectionError(int error) {
  DVLOG(1) << "Connection error before authenticated "
           << net::ErrorToString(error);
  OnConnectionAuthenticated(false);
}

void Authenticator::ProcessMessage(std::unique_ptr<BlimpMessage> message,
                                   const net::CompletionCallback& callback) {
  base::ScopedClosureRunner run_callback(base::Bind(callback, net::OK));

  if (!message->has_protocol_control() ||
      !message->protocol_control().has_start_connection()) {
    DVLOG(1) << "Expected PROTOCOL_CONTROL->START_CONNECTION, got " << *message;
    OnConnectionAuthenticated(false);
    return;
  }

  const StartConnectionMessage& start_connection =
      message->protocol_control().start_connection();

  // Verify that the protocol version is supported.
  if (start_connection.protocol_version() != kProtocolVersion) {
    DVLOG(1) << "Protocol version mismatch: "
             << start_connection.protocol_version() << " vs "
             << kProtocolVersion;

    // Inform the client of the mismatch before disconnecting it, so it can
    // show the user an appropriate error.
    connection_->GetOutgoingMessageProcessor()->ProcessMessage(
        CreateEndConnectionMessage(EndConnectionMessage::PROTOCOL_MISMATCH),
        net::CompletionCallback());

    OnConnectionAuthenticated(false);
    return;
  }

  // Verify that the authentication token matches.
  bool token_match = client_token_ == start_connection.client_token();
  DVLOG(1) << "Authentication challenge received: "
           << start_connection.client_token() << ", and token "
           << (token_match ? " matches" : " does not match");
  OnConnectionAuthenticated(token_match);
}

}  // namespace

EngineAuthenticationHandler::EngineAuthenticationHandler(
    ConnectionHandler* connection_handler,
    const std::string& client_token)
    : connection_handler_weak_factory_(connection_handler),
      client_token_(client_token) {
  DCHECK(!client_token_.empty());
}

EngineAuthenticationHandler::~EngineAuthenticationHandler() {}

void EngineAuthenticationHandler::HandleConnection(
    std::unique_ptr<BlimpConnection> connection) {
  // Authenticator manages its own lifetime.
  new Authenticator(std::move(connection),
                    connection_handler_weak_factory_.GetWeakPtr(),
                    client_token_);
}

}  // namespace blimp
