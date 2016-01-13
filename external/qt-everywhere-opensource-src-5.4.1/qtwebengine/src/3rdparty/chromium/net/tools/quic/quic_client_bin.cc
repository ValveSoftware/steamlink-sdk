// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A binary wrapper for QuicClient.  Connects to --hostname via --address
// on --port and requests URLs specified on the command line.
// Pass --secure to check the certificates using proof verifier.
// Pass --initial_stream_flow_control_window to specify the size of the initial
// stream flow control receive window to advertise to server.
// Pass --initial_session_flow_control_window to specify the size of the initial
// session flow control receive window to advertise to server.
//
// For example:
//  quic_client --address=127.0.0.1 --port=6122 --hostname=www.google.com
//      http://www.google.com/index.html http://www.google.com/favicon.ico

#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/ip_endpoint.h"
#include "net/base/privacy_mode.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_server_id.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_client.h"

// The port the quic client will connect to.
int32 FLAGS_port = 6121;
std::string FLAGS_address = "127.0.0.1";
// The hostname the quic client will connect to.
std::string FLAGS_hostname = "localhost";
// Size of the initial stream flow control receive window to advertise to
// server.
int32 FLAGS_initial_stream_flow_control_window = 100 * net::kMaxPacketSize;
// Size of the initial session flow control receive window to advertise to
// server.
int32 FLAGS_initial_session_flow_control_window = 200 * net::kMaxPacketSize;
// Check the certificates using proof verifier.
bool FLAGS_secure = false;

int main(int argc, char *argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* line = base::CommandLine::ForCurrentProcess();

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(settings));

  if (line->HasSwitch("h") || line->HasSwitch("help")) {
    const char* help_str =
        "Usage: quic_client [options]\n"
        "\n"
        "Options:\n"
        "-h, --help                  show this help message and exit\n"
        "--port=<port>               specify the port to connect to\n"
        "--address=<address>         specify the IP address to connect to\n"
        "--host=<host>               specify the SNI hostname to use\n"
        "--secure                    check certificates\n";
    std::cout << help_str;
    exit(0);
  }
  if (line->HasSwitch("port")) {
    int port;
    if (base::StringToInt(line->GetSwitchValueASCII("port"), &port)) {
      FLAGS_port = port;
    }
  }
  if (line->HasSwitch("address")) {
    FLAGS_address = line->GetSwitchValueASCII("address");
  }
  if (line->HasSwitch("hostname")) {
    FLAGS_hostname = line->GetSwitchValueASCII("hostname");
  }
  if (line->HasSwitch("secure")) {
    FLAGS_secure = true;
  }
  VLOG(1) << "server port: " << FLAGS_port
          << " address: " << FLAGS_address
          << " hostname: " << FLAGS_hostname
          << " secure: " << FLAGS_secure;

  base::AtExitManager exit_manager;

  net::IPAddressNumber addr;
  CHECK(net::ParseIPLiteralToNumber(FLAGS_address, &addr));

  net::QuicConfig config;
  config.SetDefaults();
  config.SetInitialFlowControlWindowToSend(
      FLAGS_initial_session_flow_control_window);
  config.SetInitialStreamFlowControlWindowToSend(
      FLAGS_initial_stream_flow_control_window);
  config.SetInitialSessionFlowControlWindowToSend(
      FLAGS_initial_session_flow_control_window);

  // TODO(rjshade): Set version on command line.
  net::EpollServer epoll_server;
  net::tools::QuicClient client(
      net::IPEndPoint(addr, FLAGS_port),
      net::QuicServerId(FLAGS_hostname, FLAGS_port, FLAGS_secure,
                        net::PRIVACY_MODE_DISABLED),
      net::QuicSupportedVersions(), true, config, &epoll_server);

  client.Initialize();

  if (!client.Connect()) return 1;

  client.SendRequestsAndWaitForResponse(line->GetArgs());
  return 0;
}
