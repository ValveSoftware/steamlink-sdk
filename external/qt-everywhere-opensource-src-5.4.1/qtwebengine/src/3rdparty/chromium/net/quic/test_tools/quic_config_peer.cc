// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_config_peer.h"

#include "net/quic/quic_config.h"

namespace net {
namespace test {

// static
void QuicConfigPeer::SetReceivedInitialWindow(QuicConfig* config,
                                              size_t initial_window) {
  config->initial_congestion_window_.SetReceivedValue(initial_window);
}

// static
void QuicConfigPeer::SetReceivedLossDetection(QuicConfig* config,
                                              QuicTag loss_detection) {
  config->loss_detection_.SetReceivedValue(loss_detection);
}

// static
void QuicConfigPeer::SetReceivedInitialFlowControlWindow(QuicConfig* config,
                                                         uint32 window_bytes) {
  config->initial_flow_control_window_bytes_.SetReceivedValue(window_bytes);
}

// static
void QuicConfigPeer::SetReceivedInitialStreamFlowControlWindow(
    QuicConfig* config, uint32 window_bytes) {
  config->initial_stream_flow_control_window_bytes_.SetReceivedValue(
      window_bytes);
}

// static
void QuicConfigPeer::SetReceivedInitialSessionFlowControlWindow(
    QuicConfig* config, uint32 window_bytes) {
  config->initial_session_flow_control_window_bytes_.SetReceivedValue(
      window_bytes);
}

}  // namespace test
}  // namespace net
