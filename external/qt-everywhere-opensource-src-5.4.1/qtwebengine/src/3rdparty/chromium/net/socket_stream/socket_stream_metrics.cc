// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket_stream/socket_stream_metrics.h"

#include <string.h>

#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace net {

SocketStreamMetrics::SocketStreamMetrics(const GURL& url)
    : received_bytes_(0),
      received_counts_(0),
      sent_bytes_(0),
      sent_counts_(0) {
  ProtocolType protocol_type = PROTOCOL_UNKNOWN;
  if (url.SchemeIs("ws"))
    protocol_type = PROTOCOL_WEBSOCKET;
  else if (url.SchemeIs("wss"))
    protocol_type = PROTOCOL_WEBSOCKET_SECURE;

  UMA_HISTOGRAM_ENUMERATION("Net.SocketStream.ProtocolType",
                            protocol_type, NUM_PROTOCOL_TYPES);
}

SocketStreamMetrics::~SocketStreamMetrics() {}

void SocketStreamMetrics::OnWaitConnection() {
  wait_start_time_ = base::TimeTicks::Now();
}

void SocketStreamMetrics::OnStartConnection() {
  connect_start_time_ = base::TimeTicks::Now();
  if (!wait_start_time_.is_null())
    UMA_HISTOGRAM_TIMES("Net.SocketStream.ConnectionLatency",
                        connect_start_time_ - wait_start_time_);
  OnCountConnectionType(ALL_CONNECTIONS);
}

void SocketStreamMetrics::OnConnected() {
  connect_establish_time_ = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("Net.SocketStream.ConnectionEstablish",
                      connect_establish_time_ - connect_start_time_);
}

void SocketStreamMetrics::OnRead(int len) {
  received_bytes_ += len;
  ++received_counts_;
}

void SocketStreamMetrics::OnWrite(int len) {
  sent_bytes_ += len;
  ++sent_counts_;
}

void SocketStreamMetrics::OnClose() {
  base::TimeTicks closed_time = base::TimeTicks::Now();
  if (!connect_establish_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Net.SocketStream.Duration",
                             closed_time - connect_establish_time_);
    UMA_HISTOGRAM_COUNTS("Net.SocketStream.ReceivedBytes",
                         received_bytes_);
    UMA_HISTOGRAM_COUNTS("Net.SocketStream.ReceivedCounts",
                         received_counts_);
    UMA_HISTOGRAM_COUNTS("Net.SocketStream.SentBytes",
                         sent_bytes_);
    UMA_HISTOGRAM_COUNTS("Net.SocketStream.SentCounts",
                         sent_counts_);
  }
}

void SocketStreamMetrics::OnCountConnectionType(ConnectionType type) {
  UMA_HISTOGRAM_ENUMERATION("Net.SocketStream.ConnectionType", type,
                            NUM_CONNECTION_TYPES);
}

void SocketStreamMetrics::OnCountWireProtocolType(WireProtocolType type) {
  UMA_HISTOGRAM_ENUMERATION("Net.SocketStream.WireProtocolType", type,
                            NUM_WIRE_PROTOCOL_TYPES);
}

}  // namespace net
