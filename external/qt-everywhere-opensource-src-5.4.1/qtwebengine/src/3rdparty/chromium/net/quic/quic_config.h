// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CONFIG_H_
#define NET_QUIC_QUIC_CONFIG_H_

#include <string>

#include "base/basictypes.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

namespace test {
class QuicConfigPeer;
}  // namespace test

class CryptoHandshakeMessage;

// Describes whether or not a given QuicTag is required or optional in the
// handshake message.
enum QuicConfigPresence {
  // This negotiable value can be absent from the handshake message. Default
  // value is selected as the negotiated value in such a case.
  PRESENCE_OPTIONAL,
  // This negotiable value is required in the handshake message otherwise the
  // Process*Hello function returns an error.
  PRESENCE_REQUIRED,
};

// Whether the CryptoHandshakeMessage is from the client or server.
enum HelloType {
  CLIENT,
  SERVER,
};

// An abstract base class that stores a value that can be sent in CHLO/SHLO
// message. These values can be OPTIONAL or REQUIRED, depending on |presence_|.
class NET_EXPORT_PRIVATE QuicConfigValue {
 public:
  QuicConfigValue(QuicTag tag, QuicConfigPresence presence);
  virtual ~QuicConfigValue();

  // Serialises tag name and value(s) to |out|.
  virtual void ToHandshakeMessage(CryptoHandshakeMessage* out) const = 0;

  // Selects a mutually acceptable value from those offered in |peer_hello|
  // and those defined in the subclass.
  virtual QuicErrorCode ProcessPeerHello(
      const CryptoHandshakeMessage& peer_hello,
      HelloType hello_type,
      std::string* error_details) = 0;

 protected:
  const QuicTag tag_;
  const QuicConfigPresence presence_;
};

class NET_EXPORT_PRIVATE QuicNegotiableValue : public QuicConfigValue {
 public:
  QuicNegotiableValue(QuicTag tag, QuicConfigPresence presence);
  virtual ~QuicNegotiableValue();

  bool negotiated() const {
    return negotiated_;
  }

 protected:
  bool negotiated_;
};

class NET_EXPORT_PRIVATE QuicNegotiableUint32 : public QuicNegotiableValue {
 public:
  // Default and max values default to 0.
  QuicNegotiableUint32(QuicTag name, QuicConfigPresence presence);
  virtual ~QuicNegotiableUint32();

  // Sets the maximum possible value that can be achieved after negotiation and
  // also the default values to be assumed if PRESENCE_OPTIONAL and the *HLO msg
  // doesn't contain a value corresponding to |name_|. |max| is serialised via
  // ToHandshakeMessage call if |negotiated_| is false.
  void set(uint32 max, uint32 default_value);

  // Returns the value negotiated if |negotiated_| is true, otherwise returns
  // default_value_ (used to set default values before negotiation finishes).
  uint32 GetUint32() const;

  // Serialises |name_| and value to |out|. If |negotiated_| is true then
  // |negotiated_value_| is serialised, otherwise |max_value_| is serialised.
  virtual void ToHandshakeMessage(CryptoHandshakeMessage* out) const OVERRIDE;

  // Sets |negotiated_value_| to the minimum of |max_value_| and the
  // corresponding value from |peer_hello|. If the corresponding value is
  // missing and PRESENCE_OPTIONAL then |negotiated_value_| is set to
  // |default_value_|.
  virtual QuicErrorCode ProcessPeerHello(
      const CryptoHandshakeMessage& peer_hello,
      HelloType hello_type,
      std::string* error_details) OVERRIDE;

 private:
  uint32 max_value_;
  uint32 default_value_;
  uint32 negotiated_value_;
};

class NET_EXPORT_PRIVATE QuicNegotiableTag : public QuicNegotiableValue {
 public:
  QuicNegotiableTag(QuicTag name, QuicConfigPresence presence);
  virtual ~QuicNegotiableTag();

  // Sets the possible values that |negotiated_tag_| can take after negotiation
  // and the default value that |negotiated_tag_| takes if OPTIONAL and *HLO
  // msg doesn't contain tag |name_|.
  void set(const QuicTagVector& possible_values, QuicTag default_value);

  // Returns the negotiated tag if |negotiated_| is true, otherwise returns
  // |default_value_| (used to set default values before negotiation finishes).
  QuicTag GetTag() const;

  // Serialises |name_| and vector (either possible or negotiated) to |out|. If
  // |negotiated_| is true then |negotiated_tag_| is serialised, otherwise
  // |possible_values_| is serialised.
  virtual void ToHandshakeMessage(CryptoHandshakeMessage* out) const OVERRIDE;

  // Selects the tag common to both tags in |client_hello| for |name_| and
  // |possible_values_| with preference to tag in |possible_values_|. The
  // selected tag is set as |negotiated_tag_|.
  virtual QuicErrorCode ProcessPeerHello(
      const CryptoHandshakeMessage& peer_hello,
      HelloType hello_type,
      std::string* error_details) OVERRIDE;

 private:
  // Reads the vector corresponding to |name_| from |msg| into |out|. If the
  // |name_| is absent in |msg| and |presence_| is set to OPTIONAL |out| is set
  // to |possible_values_|.
  QuicErrorCode ReadVector(const CryptoHandshakeMessage& msg,
                           const QuicTag** out,
                           size_t* out_length,
                           std::string* error_details) const;

  QuicTag negotiated_tag_;
  QuicTagVector possible_values_;
  QuicTag default_value_;
};

// Stores uint32 from CHLO or SHLO messages that are not negotiated.
class NET_EXPORT_PRIVATE QuicFixedUint32 : public QuicConfigValue {
 public:
  QuicFixedUint32(QuicTag name, QuicConfigPresence presence);
  virtual ~QuicFixedUint32();

  bool HasSendValue() const;

  uint32 GetSendValue() const;

  void SetSendValue(uint32 value);

  bool HasReceivedValue() const;

  uint32 GetReceivedValue() const;

  void SetReceivedValue(uint32 value);

  // If has_send_value is true, serialises |tag_| and |send_value_| to |out|.
  virtual void ToHandshakeMessage(CryptoHandshakeMessage* out) const OVERRIDE;

  // Sets |value_| to the corresponding value from |peer_hello_| if it exists.
  virtual QuicErrorCode ProcessPeerHello(
      const CryptoHandshakeMessage& peer_hello,
      HelloType hello_type,
      std::string* error_details) OVERRIDE;

 private:
  uint32 send_value_;
  bool has_send_value_;
  uint32 receive_value_;
  bool has_receive_value_;
};

// Stores tag from CHLO or SHLO messages that are not negotiated.
class NET_EXPORT_PRIVATE QuicFixedTag : public QuicConfigValue {
 public:
  QuicFixedTag(QuicTag name, QuicConfigPresence presence);
  virtual ~QuicFixedTag();

  bool HasSendValue() const;

  QuicTag GetSendValue() const;

  void SetSendValue(QuicTag value);

  bool HasReceivedValue() const;

  QuicTag GetReceivedValue() const;

  void SetReceivedValue(QuicTag value);

  // If has_send_value is true, serialises |tag_| and |send_value_| to |out|.
  virtual void ToHandshakeMessage(CryptoHandshakeMessage* out) const OVERRIDE;

  // Sets |value_| to the corresponding value from |client_hello_| if it exists.
  virtual QuicErrorCode ProcessPeerHello(
      const CryptoHandshakeMessage& peer_hello,
      HelloType hello_type,
      std::string* error_details) OVERRIDE;

 private:
  QuicTag send_value_;
  bool has_send_value_;
  QuicTag receive_value_;
  bool has_receive_value_;
};

// Stores tag from CHLO or SHLO messages that are not negotiated.
class NET_EXPORT_PRIVATE QuicFixedTagVector : public QuicConfigValue {
 public:
  QuicFixedTagVector(QuicTag name, QuicConfigPresence presence);
  virtual ~QuicFixedTagVector();

  bool HasSendValues() const;

  QuicTagVector GetSendValues() const;

  void SetSendValues(const QuicTagVector& values);

  bool HasReceivedValues() const;

  QuicTagVector GetReceivedValues() const;

  void SetReceivedValues(const QuicTagVector& values);

  // If has_send_value is true, serialises |tag_vector_| and |send_value_| to
  // |out|.
  virtual void ToHandshakeMessage(CryptoHandshakeMessage* out) const OVERRIDE;

  // Sets |receive_values_| to the corresponding value from |client_hello_| if
  // it exists.
  virtual QuicErrorCode ProcessPeerHello(
      const CryptoHandshakeMessage& peer_hello,
      HelloType hello_type,
      std::string* error_details) OVERRIDE;

 private:
  QuicTagVector send_values_;
  bool has_send_values_;
  QuicTagVector receive_values_;
  bool has_receive_values_;
};

// QuicConfig contains non-crypto configuration options that are negotiated in
// the crypto handshake.
class NET_EXPORT_PRIVATE QuicConfig {
 public:
  QuicConfig();
  ~QuicConfig();

  void set_congestion_feedback(const QuicTagVector& congestion_feedback,
                               QuicTag default_congestion_feedback);

  QuicTag congestion_feedback() const;

  void SetCongestionOptionsToSend(const QuicTagVector& congestion_options);

  bool HasReceivedCongestionOptions() const;

  QuicTagVector ReceivedCongestionOptions() const;

  void SetLossDetectionToSend(QuicTag loss_detection);

  bool HasReceivedLossDetection() const;

  QuicTag ReceivedLossDetection() const;

  void set_idle_connection_state_lifetime(
      QuicTime::Delta max_idle_connection_state_lifetime,
      QuicTime::Delta default_idle_conection_state_lifetime);

  QuicTime::Delta idle_connection_state_lifetime() const;

  QuicTime::Delta keepalive_timeout() const;

  void set_max_streams_per_connection(size_t max_streams,
                                      size_t default_streams);

  uint32 max_streams_per_connection() const;

  void set_max_time_before_crypto_handshake(
      QuicTime::Delta max_time_before_crypto_handshake);

  QuicTime::Delta max_time_before_crypto_handshake() const;

  // Sets the peer's default initial congestion window in packets.
  void SetInitialCongestionWindowToSend(size_t initial_window);

  bool HasReceivedInitialCongestionWindow() const;

  uint32 ReceivedInitialCongestionWindow() const;

  // Sets an estimated initial round trip time in us.
  void SetInitialRoundTripTimeUsToSend(size_t rtt_us);

  bool HasReceivedInitialRoundTripTimeUs() const;

  uint32 ReceivedInitialRoundTripTimeUs() const;

  // TODO(rjshade): Remove all InitialFlowControlWindow methods when removing
  // QUIC_VERSION_19.
  // Sets an initial stream flow control window size to transmit to the peer.
  void SetInitialFlowControlWindowToSend(uint32 window_bytes);

  uint32 GetInitialFlowControlWindowToSend() const;

  bool HasReceivedInitialFlowControlWindowBytes() const;

  uint32 ReceivedInitialFlowControlWindowBytes() const;

  // Sets an initial stream flow control window size to transmit to the peer.
  void SetInitialStreamFlowControlWindowToSend(uint32 window_bytes);

  uint32 GetInitialStreamFlowControlWindowToSend() const;

  bool HasReceivedInitialStreamFlowControlWindowBytes() const;

  uint32 ReceivedInitialStreamFlowControlWindowBytes() const;

  // Sets an initial session flow control window size to transmit to the peer.
  void SetInitialSessionFlowControlWindowToSend(uint32 window_bytes);

  uint32 GetInitialSessionFlowControlWindowToSend() const;

  bool HasReceivedInitialSessionFlowControlWindowBytes() const;

  uint32 ReceivedInitialSessionFlowControlWindowBytes() const;

  bool negotiated();

  // SetDefaults sets the members to sensible, default values.
  void SetDefaults();

  // Enabled pacing.
  void EnablePacing(bool enable_pacing);

  // ToHandshakeMessage serialises the settings in this object as a series of
  // tags /value pairs and adds them to |out|.
  void ToHandshakeMessage(CryptoHandshakeMessage* out) const;

  // Calls ProcessPeerHello on each negotiable parameter. On failure returns
  // the corresponding QuicErrorCode and sets detailed error in |error_details|.
  QuicErrorCode ProcessPeerHello(const CryptoHandshakeMessage& peer_hello,
                                 HelloType hello_type,
                                 std::string* error_details);

 private:
  friend class test::QuicConfigPeer;

  // Congestion control feedback type.
  QuicNegotiableTag congestion_feedback_;
  // Congestion control option.
  QuicFixedTagVector congestion_options_;
  // Loss detection feedback type.
  QuicFixedTag loss_detection_;
  // Idle connection state lifetime
  QuicNegotiableUint32 idle_connection_state_lifetime_seconds_;
  // Keepalive timeout, or 0 to turn off keepalive probes
  QuicNegotiableUint32 keepalive_timeout_seconds_;
  // Maximum number of streams that the connection can support.
  QuicNegotiableUint32 max_streams_per_connection_;
  // Maximum time till the session can be alive before crypto handshake is
  // finished. (Not negotiated).
  QuicTime::Delta max_time_before_crypto_handshake_;
  // Initial congestion window in packets.
  QuicFixedUint32 initial_congestion_window_;
  // Initial round trip time estimate in microseconds.
  QuicFixedUint32 initial_round_trip_time_us_;

  // TODO(rjshade): Remove when removing QUIC_VERSION_19.
  // Initial flow control receive window in bytes.
  QuicFixedUint32 initial_flow_control_window_bytes_;

  // Initial stream flow control receive window in bytes.
  QuicFixedUint32 initial_stream_flow_control_window_bytes_;
  // Initial session flow control receive window in bytes.
  QuicFixedUint32 initial_session_flow_control_window_bytes_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONFIG_H_
