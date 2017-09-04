// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper functions to configure HttpNetworkSession::Params based on field
// trials, policy, and command line.

#ifndef COMPONENTS_NETWORK_SESSION_CONFIGURATOR_NETWORK_SESSION_CONFIGURATOR_H_
#define COMPONENTS_NETWORK_SESSION_CONFIGURATOR_NETWORK_SESSION_CONFIGURATOR_H_

#include <string>

#include "net/http/http_network_session.h"

namespace network_session_configurator {

// Parse serialized QUIC version string.
// Return QUIC_VERSION_UNSUPPORTED on failure.
net::QuicVersion ParseQuicVersion(const std::string& quic_version);

// Configure |params| based on field trials
// and forcing (policy or command line) arguments.
void ParseFieldTrials(bool is_quic_force_disabled,
                      bool is_quic_force_enabled,
                      const std::string& quic_user_agent_id,
                      net::HttpNetworkSession::Params* params);

}  // namespace network_session_configurator

#endif  // COMPONENTS_NETWORK_SESSION_CONFIGURATOR_NETWORK_SESSION_CONFIGURATOR_H_
