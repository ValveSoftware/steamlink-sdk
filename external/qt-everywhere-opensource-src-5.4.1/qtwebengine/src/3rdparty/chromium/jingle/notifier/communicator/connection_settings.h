// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_CONNECTION_SETTINGS_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_CONNECTION_SETTINGS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "jingle/notifier/base/server_information.h"
#include "talk/base/socketaddress.h"

namespace buzz {
class XmppClientSettings;
}  // namespace

namespace notifier {

// The port for SSLTCP (just the regular port for SSL).
extern const uint16 kSslTcpPort;

enum SslTcpMode { DO_NOT_USE_SSLTCP, USE_SSLTCP };

struct ConnectionSettings {
 public:
  ConnectionSettings(const talk_base::SocketAddress& server,
                     SslTcpMode ssltcp_mode,
                     SslTcpSupport ssltcp_support);
  ConnectionSettings();
  ~ConnectionSettings();

  bool Equals(const ConnectionSettings& settings) const;

  std::string ToString() const;

  // Fill in the connection-related fields of |client_settings|.
  void FillXmppClientSettings(buzz::XmppClientSettings* client_settings) const;

  talk_base::SocketAddress server;
  SslTcpMode ssltcp_mode;
  SslTcpSupport ssltcp_support;
};

typedef std::vector<ConnectionSettings> ConnectionSettingsList;

// Given a list of servers in priority order, generate a list of
// ConnectionSettings to try in priority order.  If |try_ssltcp_first|
// is set, for each server that supports SSLTCP, the
// ConnectionSettings using SSLTCP will come first.  If it is not set,
// the ConnectionSettings using SSLTCP will come last.
ConnectionSettingsList MakeConnectionSettingsList(
    const ServerList& servers,
    bool try_ssltcp_first);

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_CONNECTION_SETTINGS_H_
