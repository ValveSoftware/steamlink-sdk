// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_flags.h"

// TODO(rtenneti): Remove this.
// Do not flip this flag until the flakiness of the
// net/tools/quic/end_to_end_test is fixed.
// If true, then QUIC connections will track the retransmission history of a
// packet so that an ack of a previous transmission will ack the data of all
// other transmissions.
bool FLAGS_track_retransmission_history = false;

// Do not remove this flag until the Finch-trials described in b/11706275
// are complete.
// If true, QUIC connections will support the use of a pacing algorithm when
// sending packets, in an attempt to reduce packet loss.  The client must also
// request pacing for the server to enable it.
bool FLAGS_enable_quic_pacing = true;

// Do not remove this flag until b/11792453 is marked as Fixed.
// If true, turns on stream flow control in QUIC.
// If this is disabled, all in flight QUIC connections talking QUIC_VERSION_17
// or higher will timeout. New connections will be fine.
// If disabling this flag, also disable enable_quic_connection_flow_control_2.
bool FLAGS_enable_quic_stream_flow_control_2 = true;

// Do not remove this flag until b/11792453 is marked as Fixed.
// If true, turns on connection level flow control in QUIC.
// If this is disabled, all in flight QUIC connections talking QUIC_VERSION_19
// or higher will timeout. New connections will be fine.
bool FLAGS_enable_quic_connection_flow_control_2 = true;

bool FLAGS_quic_allow_oversized_packets_for_test = false;

// When true, the use time based loss detection instead of nack.
bool FLAGS_quic_use_time_loss_detection = false;

// If true, allow peer port migration of established QUIC connections.
bool FLAGS_quic_allow_port_migration = true;

// If true, it will return as soon as an error is detected while validating
// CHLO.
bool FLAGS_use_early_return_when_verifying_chlo = true;

// If true, QUIC crypto reject message will include the reasons for rejection.
bool FLAGS_send_quic_crypto_reject_reason = false;
