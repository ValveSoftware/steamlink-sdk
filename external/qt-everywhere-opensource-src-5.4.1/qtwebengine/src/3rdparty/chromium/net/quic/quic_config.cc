// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_config.h"

#include <algorithm>

#include "base/logging.h"
#include "net/quic/crypto/crypto_handshake_message.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_sent_packet_manager.h"
#include "net/quic/quic_utils.h"

using std::min;
using std::string;

namespace net {

// Reads the value corresponding to |name_| from |msg| into |out|. If the
// |name_| is absent in |msg| and |presence| is set to OPTIONAL |out| is set
// to |default_value|.
QuicErrorCode ReadUint32(const CryptoHandshakeMessage& msg,
                         QuicTag tag,
                         QuicConfigPresence presence,
                         uint32 default_value,
                         uint32* out,
                         string* error_details) {
  DCHECK(error_details != NULL);
  QuicErrorCode error = msg.GetUint32(tag, out);
  switch (error) {
    case QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND:
      if (presence == PRESENCE_REQUIRED) {
        *error_details = "Missing " + QuicUtils::TagToString(tag);
        break;
      }
      error = QUIC_NO_ERROR;
      *out = default_value;
      break;
    case QUIC_NO_ERROR:
      break;
    default:
      *error_details = "Bad " + QuicUtils::TagToString(tag);
      break;
  }
  return error;
}


QuicConfigValue::QuicConfigValue(QuicTag tag,
                                 QuicConfigPresence presence)
    : tag_(tag),
      presence_(presence) {
}
QuicConfigValue::~QuicConfigValue() {}

QuicNegotiableValue::QuicNegotiableValue(QuicTag tag,
                                         QuicConfigPresence presence)
    : QuicConfigValue(tag, presence),
      negotiated_(false) {
}
QuicNegotiableValue::~QuicNegotiableValue() {}

QuicNegotiableUint32::QuicNegotiableUint32(QuicTag tag,
                                           QuicConfigPresence presence)
    : QuicNegotiableValue(tag, presence),
      max_value_(0),
      default_value_(0) {
}
QuicNegotiableUint32::~QuicNegotiableUint32() {}

void QuicNegotiableUint32::set(uint32 max, uint32 default_value) {
  DCHECK_LE(default_value, max);
  max_value_ = max;
  default_value_ = default_value;
}

uint32 QuicNegotiableUint32::GetUint32() const {
  if (negotiated_) {
    return negotiated_value_;
  }
  return default_value_;
}

void QuicNegotiableUint32::ToHandshakeMessage(
    CryptoHandshakeMessage* out) const {
  if (negotiated_) {
    out->SetValue(tag_, negotiated_value_);
  } else {
    out->SetValue(tag_, max_value_);
  }
}

QuicErrorCode QuicNegotiableUint32::ProcessPeerHello(
    const CryptoHandshakeMessage& peer_hello,
    HelloType hello_type,
    string* error_details) {
  DCHECK(!negotiated_);
  DCHECK(error_details != NULL);
  uint32 value;
  QuicErrorCode error = ReadUint32(peer_hello,
                                   tag_,
                                   presence_,
                                   default_value_,
                                   &value,
                                   error_details);
  if (error != QUIC_NO_ERROR) {
    return error;
  }
  if (hello_type == SERVER && value > max_value_) {
    *error_details =
        "Invalid value received for " + QuicUtils::TagToString(tag_);
    return QUIC_INVALID_NEGOTIATED_VALUE;
  }

  negotiated_ = true;
  negotiated_value_ = min(value, max_value_);
  return QUIC_NO_ERROR;
}

QuicNegotiableTag::QuicNegotiableTag(QuicTag tag, QuicConfigPresence presence)
    : QuicNegotiableValue(tag, presence),
      negotiated_tag_(0),
      default_value_(0) {
}

QuicNegotiableTag::~QuicNegotiableTag() {}

void QuicNegotiableTag::set(const QuicTagVector& possible,
                            QuicTag default_value) {
  DCHECK(ContainsQuicTag(possible, default_value));
  possible_values_ = possible;
  default_value_ = default_value;
}

QuicTag QuicNegotiableTag::GetTag() const {
  if (negotiated_) {
    return negotiated_tag_;
  }
  return default_value_;
}

void QuicNegotiableTag::ToHandshakeMessage(CryptoHandshakeMessage* out) const {
  if (negotiated_) {
    // Because of the way we serialize and parse handshake messages we can
    // serialize this as value and still parse it as a vector.
    out->SetValue(tag_, negotiated_tag_);
  } else {
    out->SetVector(tag_, possible_values_);
  }
}

QuicErrorCode QuicNegotiableTag::ReadVector(
    const CryptoHandshakeMessage& msg,
    const QuicTag** out,
    size_t* out_length,
    string* error_details) const {
  DCHECK(error_details != NULL);
  QuicErrorCode error = msg.GetTaglist(tag_, out, out_length);
  switch (error) {
    case QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND:
      if (presence_ == PRESENCE_REQUIRED) {
        *error_details = "Missing " + QuicUtils::TagToString(tag_);
        break;
      }
      error = QUIC_NO_ERROR;
      *out_length = 1;
      *out = &default_value_;

    case QUIC_NO_ERROR:
      break;
    default:
      *error_details = "Bad " + QuicUtils::TagToString(tag_);
      break;
  }
  return error;
}

QuicErrorCode QuicNegotiableTag::ProcessPeerHello(
    const CryptoHandshakeMessage& peer_hello,
    HelloType hello_type,
    string* error_details) {
  DCHECK(!negotiated_);
  DCHECK(error_details != NULL);
  const QuicTag* received_tags;
  size_t received_tags_length;
  QuicErrorCode error = ReadVector(peer_hello, &received_tags,
                                   &received_tags_length, error_details);
  if (error != QUIC_NO_ERROR) {
    return error;
  }

  if (hello_type == SERVER) {
    if (received_tags_length != 1 ||
        !ContainsQuicTag(possible_values_,  *received_tags)) {
      *error_details = "Invalid " + QuicUtils::TagToString(tag_);
      return QUIC_INVALID_NEGOTIATED_VALUE;
    }
    negotiated_tag_ = *received_tags;
  } else {
    QuicTag negotiated_tag;
    if (!QuicUtils::FindMutualTag(possible_values_,
                                  received_tags,
                                  received_tags_length,
                                  QuicUtils::LOCAL_PRIORITY,
                                  &negotiated_tag,
                                  NULL)) {
      *error_details = "Unsupported " + QuicUtils::TagToString(tag_);
      return QUIC_CRYPTO_MESSAGE_PARAMETER_NO_OVERLAP;
    }
    negotiated_tag_ = negotiated_tag;
  }

  negotiated_ = true;
  return QUIC_NO_ERROR;
}

QuicFixedUint32::QuicFixedUint32(QuicTag tag, QuicConfigPresence presence)
    : QuicConfigValue(tag, presence),
      has_send_value_(false),
      has_receive_value_(false) {
}
QuicFixedUint32::~QuicFixedUint32() {}

bool QuicFixedUint32::HasSendValue() const {
  return has_send_value_;
}

uint32 QuicFixedUint32::GetSendValue() const {
  LOG_IF(DFATAL, !has_send_value_)
      << "No send value to get for tag:" << QuicUtils::TagToString(tag_);
  return send_value_;
}

void QuicFixedUint32::SetSendValue(uint32 value) {
  has_send_value_ = true;
  send_value_ = value;
}

bool QuicFixedUint32::HasReceivedValue() const {
  return has_receive_value_;
}

uint32 QuicFixedUint32::GetReceivedValue() const {
  LOG_IF(DFATAL, !has_receive_value_)
      << "No receive value to get for tag:" << QuicUtils::TagToString(tag_);
  return receive_value_;
}

void QuicFixedUint32::SetReceivedValue(uint32 value) {
  has_receive_value_ = true;
  receive_value_ = value;
}

void QuicFixedUint32::ToHandshakeMessage(CryptoHandshakeMessage* out) const {
  if (has_send_value_) {
    out->SetValue(tag_, send_value_);
  }
}

QuicErrorCode QuicFixedUint32::ProcessPeerHello(
    const CryptoHandshakeMessage& peer_hello,
    HelloType hello_type,
    string* error_details) {
  DCHECK(error_details != NULL);
  QuicErrorCode error = peer_hello.GetUint32(tag_, &receive_value_);
  switch (error) {
    case QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND:
      if (presence_ == PRESENCE_OPTIONAL) {
        return QUIC_NO_ERROR;
      }
      *error_details = "Missing " + QuicUtils::TagToString(tag_);
      break;
    case QUIC_NO_ERROR:
      has_receive_value_ = true;
      break;
    default:
      *error_details = "Bad " + QuicUtils::TagToString(tag_);
      break;
  }
  return error;
}

QuicFixedTag::QuicFixedTag(QuicTag name,
                           QuicConfigPresence presence)
    : QuicConfigValue(name, presence),
      has_send_value_(false),
      has_receive_value_(false) {
}

QuicFixedTag::~QuicFixedTag() {}

bool QuicFixedTag::HasSendValue() const {
  return has_send_value_;
}

uint32 QuicFixedTag::GetSendValue() const {
  LOG_IF(DFATAL, !has_send_value_)
      << "No send value to get for tag:" << QuicUtils::TagToString(tag_);
  return send_value_;
}

void QuicFixedTag::SetSendValue(uint32 value) {
  has_send_value_ = true;
  send_value_ = value;
}

bool QuicFixedTag::HasReceivedValue() const {
  return has_receive_value_;
}

uint32 QuicFixedTag::GetReceivedValue() const {
  LOG_IF(DFATAL, !has_receive_value_)
      << "No receive value to get for tag:" << QuicUtils::TagToString(tag_);
  return receive_value_;
}

void QuicFixedTag::SetReceivedValue(uint32 value) {
  has_receive_value_ = true;
  receive_value_ = value;
}

void QuicFixedTag::ToHandshakeMessage(CryptoHandshakeMessage* out) const {
  if (has_send_value_) {
    out->SetValue(tag_, send_value_);
  }
}

QuicErrorCode QuicFixedTag::ProcessPeerHello(
    const CryptoHandshakeMessage& peer_hello,
    HelloType hello_type,
    string* error_details) {
  DCHECK(error_details != NULL);
  QuicErrorCode error = peer_hello.GetUint32(tag_, &receive_value_);
  switch (error) {
    case QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND:
      if (presence_ == PRESENCE_OPTIONAL) {
        return QUIC_NO_ERROR;
      }
      *error_details = "Missing " + QuicUtils::TagToString(tag_);
      break;
    case QUIC_NO_ERROR:
      has_receive_value_ = true;
      break;
    default:
      *error_details = "Bad " + QuicUtils::TagToString(tag_);
      break;
  }
  return error;
}

QuicFixedTagVector::QuicFixedTagVector(QuicTag name,
                                       QuicConfigPresence presence)
    : QuicConfigValue(name, presence),
      has_send_values_(false),
      has_receive_values_(false) {
}

QuicFixedTagVector::~QuicFixedTagVector() {}

bool QuicFixedTagVector::HasSendValues() const {
  return has_send_values_;
}

QuicTagVector QuicFixedTagVector::GetSendValues() const {
  LOG_IF(DFATAL, !has_send_values_)
      << "No send values to get for tag:" << QuicUtils::TagToString(tag_);
  return send_values_;
}

void QuicFixedTagVector::SetSendValues(const QuicTagVector& values) {
  has_send_values_ = true;
  send_values_ = values;
}

bool QuicFixedTagVector::HasReceivedValues() const {
  return has_receive_values_;
}

QuicTagVector QuicFixedTagVector::GetReceivedValues() const {
  LOG_IF(DFATAL, !has_receive_values_)
      << "No receive value to get for tag:" << QuicUtils::TagToString(tag_);
  return receive_values_;
}

void QuicFixedTagVector::SetReceivedValues(const QuicTagVector& values) {
  has_receive_values_ = true;
  receive_values_ = values;
}

void QuicFixedTagVector::ToHandshakeMessage(CryptoHandshakeMessage* out) const {
  if (has_send_values_) {
    out->SetVector(tag_, send_values_);
  }
}

QuicErrorCode QuicFixedTagVector::ProcessPeerHello(
    const CryptoHandshakeMessage& peer_hello,
    HelloType hello_type,
    string* error_details) {
  DCHECK(error_details != NULL);
  const QuicTag* received_tags;
  size_t received_tags_length;
  QuicErrorCode error =
      peer_hello.GetTaglist(tag_, &received_tags, &received_tags_length);
  switch (error) {
    case QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND:
      if (presence_ == PRESENCE_OPTIONAL) {
        return QUIC_NO_ERROR;
      }
      *error_details = "Missing " + QuicUtils::TagToString(tag_);
      break;
    case QUIC_NO_ERROR:
      has_receive_values_ = true;
      for (size_t i = 0; i < received_tags_length; ++i) {
        receive_values_.push_back(received_tags[i]);
      }
      break;
    default:
      *error_details = "Bad " + QuicUtils::TagToString(tag_);
      break;
  }
  return error;
}

QuicConfig::QuicConfig()
    : congestion_feedback_(kCGST, PRESENCE_REQUIRED),
      congestion_options_(kCOPT, PRESENCE_OPTIONAL),
      loss_detection_(kLOSS, PRESENCE_OPTIONAL),
      idle_connection_state_lifetime_seconds_(kICSL, PRESENCE_REQUIRED),
      keepalive_timeout_seconds_(kKATO, PRESENCE_OPTIONAL),
      max_streams_per_connection_(kMSPC, PRESENCE_REQUIRED),
      max_time_before_crypto_handshake_(QuicTime::Delta::Zero()),
      initial_congestion_window_(kSWND, PRESENCE_OPTIONAL),
      initial_round_trip_time_us_(kIRTT, PRESENCE_OPTIONAL),
      // TODO(rjshade): Make this PRESENCE_REQUIRED when retiring
      // QUIC_VERSION_17.
      initial_flow_control_window_bytes_(kIFCW, PRESENCE_OPTIONAL),
      // TODO(rjshade): Make this PRESENCE_REQUIRED when retiring
      // QUIC_VERSION_19.
      initial_stream_flow_control_window_bytes_(kSFCW, PRESENCE_OPTIONAL),
      // TODO(rjshade): Make this PRESENCE_REQUIRED when retiring
      // QUIC_VERSION_19.
      initial_session_flow_control_window_bytes_(kCFCW, PRESENCE_OPTIONAL) {
}

QuicConfig::~QuicConfig() {}

void QuicConfig::set_congestion_feedback(
    const QuicTagVector& congestion_feedback,
    QuicTag default_congestion_feedback) {
  congestion_feedback_.set(congestion_feedback, default_congestion_feedback);
}

QuicTag QuicConfig::congestion_feedback() const {
  return congestion_feedback_.GetTag();
}

void QuicConfig::SetCongestionOptionsToSend(
    const QuicTagVector& congestion_options) {
  congestion_options_.SetSendValues(congestion_options);
}

bool QuicConfig::HasReceivedCongestionOptions() const {
  return congestion_options_.HasReceivedValues();
}

QuicTagVector QuicConfig::ReceivedCongestionOptions() const {
  return congestion_options_.GetReceivedValues();
}

void QuicConfig::SetLossDetectionToSend(QuicTag loss_detection) {
  loss_detection_.SetSendValue(loss_detection);
}

bool QuicConfig::HasReceivedLossDetection() const {
  return loss_detection_.HasReceivedValue();
}

QuicTag QuicConfig::ReceivedLossDetection() const {
  return loss_detection_.GetReceivedValue();
}

void QuicConfig::set_idle_connection_state_lifetime(
    QuicTime::Delta max_idle_connection_state_lifetime,
    QuicTime::Delta default_idle_conection_state_lifetime) {
  idle_connection_state_lifetime_seconds_.set(
      max_idle_connection_state_lifetime.ToSeconds(),
      default_idle_conection_state_lifetime.ToSeconds());
}

QuicTime::Delta QuicConfig::idle_connection_state_lifetime() const {
  return QuicTime::Delta::FromSeconds(
      idle_connection_state_lifetime_seconds_.GetUint32());
}

QuicTime::Delta QuicConfig::keepalive_timeout() const {
  return QuicTime::Delta::FromSeconds(
      keepalive_timeout_seconds_.GetUint32());
}

void QuicConfig::set_max_streams_per_connection(size_t max_streams,
                                                size_t default_streams) {
  max_streams_per_connection_.set(max_streams, default_streams);
}

uint32 QuicConfig::max_streams_per_connection() const {
  return max_streams_per_connection_.GetUint32();
}

void QuicConfig::set_max_time_before_crypto_handshake(
    QuicTime::Delta max_time_before_crypto_handshake) {
  max_time_before_crypto_handshake_ = max_time_before_crypto_handshake;
}

QuicTime::Delta QuicConfig::max_time_before_crypto_handshake() const {
  return max_time_before_crypto_handshake_;
}

void QuicConfig::SetInitialCongestionWindowToSend(size_t initial_window) {
  initial_congestion_window_.SetSendValue(initial_window);
}

bool QuicConfig::HasReceivedInitialCongestionWindow() const {
  return initial_congestion_window_.HasReceivedValue();
}

uint32 QuicConfig::ReceivedInitialCongestionWindow() const {
  return initial_congestion_window_.GetReceivedValue();
}

void QuicConfig::SetInitialRoundTripTimeUsToSend(size_t rtt) {
  initial_round_trip_time_us_.SetSendValue(rtt);
}

bool QuicConfig::HasReceivedInitialRoundTripTimeUs() const {
  return initial_round_trip_time_us_.HasReceivedValue();
}

uint32 QuicConfig::ReceivedInitialRoundTripTimeUs() const {
  return initial_round_trip_time_us_.GetReceivedValue();
}

void QuicConfig::SetInitialFlowControlWindowToSend(uint32 window_bytes) {
  if (window_bytes < kDefaultFlowControlSendWindow) {
    LOG(DFATAL) << "Initial flow control receive window (" << window_bytes
                << ") cannot be set lower than default ("
                << kDefaultFlowControlSendWindow << ").";
    window_bytes = kDefaultFlowControlSendWindow;
  }
  initial_flow_control_window_bytes_.SetSendValue(window_bytes);
}

uint32 QuicConfig::GetInitialFlowControlWindowToSend() const {
  return initial_flow_control_window_bytes_.GetSendValue();
}

bool QuicConfig::HasReceivedInitialFlowControlWindowBytes() const {
  return initial_flow_control_window_bytes_.HasReceivedValue();
}

uint32 QuicConfig::ReceivedInitialFlowControlWindowBytes() const {
  return initial_flow_control_window_bytes_.GetReceivedValue();
}

void QuicConfig::SetInitialStreamFlowControlWindowToSend(uint32 window_bytes) {
  if (window_bytes < kDefaultFlowControlSendWindow) {
    LOG(DFATAL) << "Initial stream flow control receive window ("
                << window_bytes << ") cannot be set lower than default ("
                << kDefaultFlowControlSendWindow << ").";
    window_bytes = kDefaultFlowControlSendWindow;
  }
  initial_stream_flow_control_window_bytes_.SetSendValue(window_bytes);
}

uint32 QuicConfig::GetInitialStreamFlowControlWindowToSend() const {
  return initial_stream_flow_control_window_bytes_.GetSendValue();
}

bool QuicConfig::HasReceivedInitialStreamFlowControlWindowBytes() const {
  return initial_stream_flow_control_window_bytes_.HasReceivedValue();
}

uint32 QuicConfig::ReceivedInitialStreamFlowControlWindowBytes() const {
  return initial_stream_flow_control_window_bytes_.GetReceivedValue();
}

void QuicConfig::SetInitialSessionFlowControlWindowToSend(uint32 window_bytes) {
  if (window_bytes < kDefaultFlowControlSendWindow) {
    LOG(DFATAL) << "Initial session flow control receive window ("
                << window_bytes << ") cannot be set lower than default ("
                << kDefaultFlowControlSendWindow << ").";
    window_bytes = kDefaultFlowControlSendWindow;
  }
  initial_session_flow_control_window_bytes_.SetSendValue(window_bytes);
}

uint32 QuicConfig::GetInitialSessionFlowControlWindowToSend() const {
  return initial_session_flow_control_window_bytes_.GetSendValue();
}

bool QuicConfig::HasReceivedInitialSessionFlowControlWindowBytes() const {
  return initial_session_flow_control_window_bytes_.HasReceivedValue();
}

uint32 QuicConfig::ReceivedInitialSessionFlowControlWindowBytes() const {
  return initial_session_flow_control_window_bytes_.GetReceivedValue();
}

bool QuicConfig::negotiated() {
  // TODO(ianswett): Add the negotiated parameters once and iterate over all
  // of them in negotiated, ToHandshakeMessage, ProcessClientHello, and
  // ProcessServerHello.
  return congestion_feedback_.negotiated() &&
      idle_connection_state_lifetime_seconds_.negotiated() &&
      keepalive_timeout_seconds_.negotiated() &&
      max_streams_per_connection_.negotiated();
}

void QuicConfig::SetDefaults() {
  QuicTagVector congestion_feedback;
  if (FLAGS_enable_quic_pacing) {
    congestion_feedback.push_back(kPACE);
  }
  congestion_feedback.push_back(kQBIC);
  congestion_feedback_.set(congestion_feedback, kQBIC);
  idle_connection_state_lifetime_seconds_.set(kDefaultTimeoutSecs,
                                              kDefaultInitialTimeoutSecs);
  // kKATO is optional. Return 0 if not negotiated.
  keepalive_timeout_seconds_.set(0, 0);
  max_streams_per_connection_.set(kDefaultMaxStreamsPerConnection,
                                  kDefaultMaxStreamsPerConnection);
  max_time_before_crypto_handshake_ = QuicTime::Delta::FromSeconds(
      kDefaultMaxTimeForCryptoHandshakeSecs);

  SetInitialFlowControlWindowToSend(kDefaultFlowControlSendWindow);
  SetInitialStreamFlowControlWindowToSend(kDefaultFlowControlSendWindow);
  SetInitialSessionFlowControlWindowToSend(kDefaultFlowControlSendWindow);
}

void QuicConfig::EnablePacing(bool enable_pacing) {
  QuicTagVector congestion_feedback;
  if (enable_pacing) {
    congestion_feedback.push_back(kPACE);
  }
  congestion_feedback.push_back(kQBIC);
  congestion_feedback_.set(congestion_feedback, kQBIC);
}

void QuicConfig::ToHandshakeMessage(CryptoHandshakeMessage* out) const {
  congestion_feedback_.ToHandshakeMessage(out);
  idle_connection_state_lifetime_seconds_.ToHandshakeMessage(out);
  keepalive_timeout_seconds_.ToHandshakeMessage(out);
  max_streams_per_connection_.ToHandshakeMessage(out);
  initial_congestion_window_.ToHandshakeMessage(out);
  initial_round_trip_time_us_.ToHandshakeMessage(out);
  loss_detection_.ToHandshakeMessage(out);
  initial_flow_control_window_bytes_.ToHandshakeMessage(out);
  initial_stream_flow_control_window_bytes_.ToHandshakeMessage(out);
  initial_session_flow_control_window_bytes_.ToHandshakeMessage(out);
  congestion_options_.ToHandshakeMessage(out);
}

QuicErrorCode QuicConfig::ProcessPeerHello(
    const CryptoHandshakeMessage& peer_hello,
    HelloType hello_type,
    string* error_details) {
  DCHECK(error_details != NULL);

  QuicErrorCode error = QUIC_NO_ERROR;
  if (error == QUIC_NO_ERROR) {
    error = congestion_feedback_.ProcessPeerHello(
        peer_hello,  hello_type, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = idle_connection_state_lifetime_seconds_.ProcessPeerHello(
        peer_hello, hello_type, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = keepalive_timeout_seconds_.ProcessPeerHello(
        peer_hello, hello_type, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = max_streams_per_connection_.ProcessPeerHello(
        peer_hello, hello_type, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = initial_congestion_window_.ProcessPeerHello(
        peer_hello, hello_type, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = initial_round_trip_time_us_.ProcessPeerHello(
        peer_hello, hello_type, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = initial_flow_control_window_bytes_.ProcessPeerHello(
        peer_hello, hello_type, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = initial_stream_flow_control_window_bytes_.ProcessPeerHello(
        peer_hello, hello_type, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = initial_session_flow_control_window_bytes_.ProcessPeerHello(
        peer_hello, hello_type, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = loss_detection_.ProcessPeerHello(
        peer_hello, hello_type, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = congestion_options_.ProcessPeerHello(
        peer_hello, hello_type, error_details);
  }
  return error;
}

}  // namespace net
