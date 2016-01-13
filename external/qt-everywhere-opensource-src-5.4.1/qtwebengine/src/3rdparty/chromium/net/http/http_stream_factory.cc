// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_network_session.h"
#include "url/gurl.h"

namespace net {

// WARNING: If you modify or add any static flags, you must keep them in sync
// with |ResetStaticSettingsToInit|. This is critical for unit test isolation.

// static
bool HttpStreamFactory::spdy_enabled_ = true;

HttpStreamFactory::~HttpStreamFactory() {}

// static
void HttpStreamFactory::ResetStaticSettingsToInit() {
  spdy_enabled_ = true;
}

void HttpStreamFactory::ProcessAlternateProtocol(
    const base::WeakPtr<HttpServerProperties>& http_server_properties,
    const std::string& alternate_protocol_str,
    const HostPortPair& http_host_port_pair,
    const HttpNetworkSession& session) {
  std::vector<std::string> port_protocol_vector;
  base::SplitString(alternate_protocol_str, ':', &port_protocol_vector);
  if (port_protocol_vector.size() != 2) {
    DVLOG(1) << kAlternateProtocolHeader
             << " header has too many tokens: "
             << alternate_protocol_str;
    return;
  }

  int port;
  if (!base::StringToInt(port_protocol_vector[0], &port) ||
      port <= 0 || port >= 1 << 16) {
    DVLOG(1) << kAlternateProtocolHeader
             << " header has unrecognizable port: "
             << port_protocol_vector[0];
    return;
  }

  AlternateProtocol protocol =
      AlternateProtocolFromString(port_protocol_vector[1]);
  if (IsAlternateProtocolValid(protocol) &&
      !session.IsProtocolEnabled(protocol)) {
    protocol = ALTERNATE_PROTOCOL_BROKEN;
  }

  if (protocol == ALTERNATE_PROTOCOL_BROKEN) {
    DVLOG(1) << kAlternateProtocolHeader
             << " header has unrecognized protocol: "
             << port_protocol_vector[1];
    return;
  }

  HostPortPair host_port(http_host_port_pair);
  const HostMappingRules* mapping_rules = GetHostMappingRules();
  if (mapping_rules)
    mapping_rules->RewriteHost(&host_port);

  if (http_server_properties->HasAlternateProtocol(host_port)) {
    const PortAlternateProtocolPair existing_alternate =
        http_server_properties->GetAlternateProtocol(host_port);
    // If we think the alternate protocol is broken, don't change it.
    if (existing_alternate.protocol == ALTERNATE_PROTOCOL_BROKEN)
      return;
  }

  http_server_properties->SetAlternateProtocol(host_port, port, protocol);
}

GURL HttpStreamFactory::ApplyHostMappingRules(const GURL& url,
                                              HostPortPair* endpoint) {
  const HostMappingRules* mapping_rules = GetHostMappingRules();
  if (mapping_rules && mapping_rules->RewriteHost(endpoint)) {
    url::Replacements<char> replacements;
    const std::string port_str = base::IntToString(endpoint->port());
    replacements.SetPort(port_str.c_str(), url::Component(0, port_str.size()));
    replacements.SetHost(endpoint->host().c_str(),
                         url::Component(0, endpoint->host().size()));
    return url.ReplaceComponents(replacements);
  }
  return url;
}

HttpStreamFactory::HttpStreamFactory() {}

}  // namespace net
