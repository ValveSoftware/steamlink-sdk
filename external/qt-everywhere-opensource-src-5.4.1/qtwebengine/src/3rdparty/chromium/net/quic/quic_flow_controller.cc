// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_flow_controller.h"

#include "base/basictypes.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_protocol.h"

namespace net {

#define ENDPOINT (is_server_ ? "Server: " : " Client: ")

QuicFlowController::QuicFlowController(QuicConnection* connection,
                                       QuicStreamId id,
                                       bool is_server,
                                       uint64 send_window_offset,
                                       uint64 receive_window_offset,
                                       uint64 max_receive_window)
    : connection_(connection),
      id_(id),
      is_enabled_(true),
      is_server_(is_server),
      bytes_consumed_(0),
      highest_received_byte_offset_(0),
      bytes_sent_(0),
      send_window_offset_(send_window_offset),
      receive_window_offset_(receive_window_offset),
      max_receive_window_(max_receive_window),
      last_blocked_send_window_offset_(0) {
  DVLOG(1) << ENDPOINT << "Created flow controller for stream " << id_
           << ", setting initial receive window offset to: "
           << receive_window_offset_
           << ", max receive window to: "
           << max_receive_window_
           << ", setting send window offset to: " << send_window_offset_;
  if (connection_->version() < QUIC_VERSION_17) {
    DVLOG(1) << ENDPOINT << "Disabling QuicFlowController for stream " << id_
             << ", QUIC version " << connection_->version();
    Disable();
  }
}

void QuicFlowController::AddBytesConsumed(uint64 bytes_consumed) {
  if (!IsEnabled()) {
    return;
  }

  bytes_consumed_ += bytes_consumed;
  DVLOG(1) << ENDPOINT << "Stream " << id_ << " consumed: " << bytes_consumed_;

  MaybeSendWindowUpdate();
}

bool QuicFlowController::UpdateHighestReceivedOffset(uint64 new_offset) {
  if (!IsEnabled()) {
    return false;
  }

  // Only update if offset has increased.
  if (new_offset <= highest_received_byte_offset_) {
    return false;
  }

  DVLOG(1) << ENDPOINT << "Stream " << id_
           << " highest byte offset increased from: "
           << highest_received_byte_offset_ << " to " << new_offset;
  highest_received_byte_offset_ = new_offset;
  return true;
}

void QuicFlowController::AddBytesSent(uint64 bytes_sent) {
  if (!IsEnabled()) {
    return;
  }

  if (bytes_sent_ + bytes_sent > send_window_offset_) {
    LOG(DFATAL) << ENDPOINT << "Stream " << id_ << " Trying to send an extra "
                << bytes_sent << " bytes, when bytes_sent = " << bytes_sent_
                << ", and send_window_offset_ = " << send_window_offset_;
    bytes_sent_ = send_window_offset_;

    // This is an error on our side, close the connection as soon as possible.
    connection_->SendConnectionClose(QUIC_FLOW_CONTROL_SENT_TOO_MUCH_DATA);
    return;
  }

  bytes_sent_ += bytes_sent;
  DVLOG(1) << ENDPOINT << "Stream " << id_ << " sent: " << bytes_sent_;
}

bool QuicFlowController::FlowControlViolation() {
  if (!IsEnabled()) {
    return false;
  }

  if (highest_received_byte_offset_ > receive_window_offset_) {
    LOG(ERROR) << ENDPOINT << "Flow control violation on stream "
               << id_ << ", receive window offset: "
               << receive_window_offset_
               << ", highest received byte offset: "
               << highest_received_byte_offset_;
    return true;
  }
  return false;
}

void QuicFlowController::MaybeSendWindowUpdate() {
  if (!IsEnabled()) {
    return;
  }

  // Send WindowUpdate to increase receive window if
  // (receive window offset - consumed bytes) < (max window / 2).
  // This is behaviour copied from SPDY.
  DCHECK_LT(bytes_consumed_, receive_window_offset_);
  size_t consumed_window = receive_window_offset_ - bytes_consumed_;
  size_t threshold = (max_receive_window_ / 2);

  if (consumed_window < threshold) {
    // Update our receive window.
    receive_window_offset_ += (max_receive_window_ - consumed_window);

    DVLOG(1) << ENDPOINT << "Sending WindowUpdate frame for stream " << id_
             << ", consumed bytes: " << bytes_consumed_
             << ", consumed window: " << consumed_window
             << ", and threshold: " << threshold
             << ", and max recvw: " << max_receive_window_
             << ". New receive window offset is: " << receive_window_offset_;

    // Inform the peer of our new receive window.
    connection_->SendWindowUpdate(id_, receive_window_offset_);
  }
}

void QuicFlowController::MaybeSendBlocked() {
  if (!IsEnabled()) {
    return;
  }

  if (SendWindowSize() == 0 &&
      last_blocked_send_window_offset_ < send_window_offset_) {
    DVLOG(1) << ENDPOINT << "Stream " << id_ << " is flow control blocked. "
             << "Send window: " << SendWindowSize()
             << ", bytes sent: " << bytes_sent_
             << ", send limit: " << send_window_offset_;
    // The entire send_window has been consumed, we are now flow control
    // blocked.
    connection_->SendBlocked(id_);

    // Keep track of when we last sent a BLOCKED frame so that we only send one
    // at a given send offset.
    last_blocked_send_window_offset_ = send_window_offset_;
  }
}

bool QuicFlowController::UpdateSendWindowOffset(uint64 new_send_window_offset) {
  if (!IsEnabled()) {
    return false;
  }

  // Only update if send window has increased.
  if (new_send_window_offset <= send_window_offset_) {
    return false;
  }

  DVLOG(1) << ENDPOINT << "UpdateSendWindowOffset for stream " << id_
           << " with new offset " << new_send_window_offset
           << " , current offset: " << send_window_offset_;

  send_window_offset_ = new_send_window_offset;
  return true;
}

void QuicFlowController::Disable() {
  is_enabled_ = false;
}

bool QuicFlowController::IsEnabled() const {
  bool connection_flow_control_enabled =
      (id_ == kConnectionLevelId &&
       FLAGS_enable_quic_connection_flow_control_2);
  bool stream_flow_control_enabled =
      (id_ != kConnectionLevelId &&
       FLAGS_enable_quic_stream_flow_control_2);
  return (connection_flow_control_enabled || stream_flow_control_enabled) &&
         is_enabled_;
}

bool QuicFlowController::IsBlocked() const {
  return IsEnabled() && SendWindowSize() == 0;
}

uint64 QuicFlowController::SendWindowSize() const {
  if (bytes_sent_ > send_window_offset_) {
    return 0;
  }
  return send_window_offset_ - bytes_sent_;
}

}  // namespace net
