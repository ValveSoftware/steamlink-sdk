// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_FLAGS_H_
#define NET_QUIC_QUIC_FLAGS_H_

#include "net/base/net_export.h"

NET_EXPORT_PRIVATE extern bool FLAGS_track_retransmission_history;
NET_EXPORT_PRIVATE extern bool FLAGS_enable_quic_pacing;
NET_EXPORT_PRIVATE extern bool FLAGS_enable_quic_stream_flow_control_2;
NET_EXPORT_PRIVATE extern bool FLAGS_enable_quic_connection_flow_control_2;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_allow_oversized_packets_for_test;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_use_time_loss_detection;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_allow_port_migration;
NET_EXPORT_PRIVATE extern bool FLAGS_use_early_return_when_verifying_chlo;
NET_EXPORT_PRIVATE extern bool FLAGS_send_quic_crypto_reject_reason;

#endif  // NET_QUIC_QUIC_FLAGS_H_
