// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper functions to configure HttpNetworkSession::Params based on field
// trials, policy, and command line.

#ifndef COMPONENTS_NETWORK_SESSION_CONFIGURATOR_NETWORK_SESSION_CONFIGURATOR_H_
#define COMPONENTS_NETWORK_SESSION_CONFIGURATOR_NETWORK_SESSION_CONFIGURATOR_H_

#include <string>

#include "base/command_line.h"
#include "net/http/http_network_session.h"

namespace network_session_configurator {

// Configure |params| based on field trials and policy arguments.
void ParseFieldTrials(bool is_spdy_allowed_by_policy,
                      bool is_quic_allowed_by_policy,
                      const std::string& quic_user_agent_id,
                      net::HttpNetworkSession::Params* params);

// Configure |params| based on field trials, policy arguments,
// and command line.
void ParseFieldTrialsAndCommandLine(bool is_spdy_allowed_by_policy,
                                    bool is_quic_allowed_by_policy,
                                    const std::string& quic_user_agent_id,
                                    net::HttpNetworkSession::Params* params);

}  // namespace network_session_configurator

#endif  // COMPONENTS_NETWORK_SESSION_CONFIGURATOR_NETWORK_SESSION_CONFIGURATOR_H_
