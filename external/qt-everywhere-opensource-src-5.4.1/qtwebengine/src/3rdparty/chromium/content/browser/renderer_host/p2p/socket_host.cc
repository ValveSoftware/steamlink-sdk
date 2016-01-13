// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_host.h"

#include "base/sys_byteorder.h"
#include "content/browser/renderer_host/p2p/socket_host_tcp.h"
#include "content/browser/renderer_host/p2p/socket_host_tcp_server.h"
#include "content/browser/renderer_host/p2p/socket_host_udp.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/hmac.h"
#include "third_party/libjingle/source/talk/base/asyncpacketsocket.h"
#include "third_party/libjingle/source/talk/base/byteorder.h"
#include "third_party/libjingle/source/talk/base/messagedigest.h"
#include "third_party/libjingle/source/talk/p2p/base/stun.h"

namespace {

const uint32 kStunMagicCookie = 0x2112A442;
const int kMinRtpHdrLen = 12;
const int kRtpExtnHdrLen = 4;
const int kDtlsRecordHeaderLen = 13;
const int kTurnChannelHdrLen = 4;
const int kAbsSendTimeExtnLen = 3;
const int kOneByteHdrLen = 1;

// Fake auth tag written by the render process if external authentication is
// enabled. HMAC in packet will be compared against this value before updating
// packet with actual HMAC value.
static const unsigned char kFakeAuthTag[10] = {
    0xba, 0xdd, 0xba, 0xdd, 0xba, 0xdd, 0xba, 0xdd, 0xba, 0xdd
};

bool IsTurnChannelData(const char* data) {
  return ((*data & 0xC0) == 0x40);
}

bool IsDtlsPacket(const char* data, int len) {
  const uint8* u = reinterpret_cast<const uint8*>(data);
  return (len >= kDtlsRecordHeaderLen && (u[0] > 19 && u[0] < 64));
}

bool IsRtcpPacket(const char* data) {
  int type = (static_cast<uint8>(data[1]) & 0x7F);
  return (type >= 64 && type < 96);
}

bool IsTurnSendIndicationPacket(const char* data) {
  uint16 type = talk_base::GetBE16(data);
  return (type == cricket::TURN_SEND_INDICATION);
}

bool IsRtpPacket(const char* data, int len) {
  return ((*data & 0xC0) == 0x80);
}

// Verifies rtp header and message length.
bool ValidateRtpHeader(const char* rtp, int length, size_t* header_length) {
  if (header_length)
    *header_length = 0;

  int cc_count = rtp[0] & 0x0F;
  int rtp_hdr_len_without_extn = kMinRtpHdrLen + 4 * cc_count;
  if (rtp_hdr_len_without_extn > length) {
    return false;
  }

  // If extension bit is not set, we are done with header processing, as input
  // length is verified above.
  if (!(rtp[0] & 0x10)) {
    if (header_length)
      *header_length = rtp_hdr_len_without_extn;

    return true;
  }

  rtp += rtp_hdr_len_without_extn;

  // Getting extension profile length.
  // Length is in 32 bit words.
  uint16 extn_length = talk_base::GetBE16(rtp + 2) * 4;

  // Verify input length against total header size.
  if (rtp_hdr_len_without_extn + kRtpExtnHdrLen + extn_length > length) {
    return false;
  }

  if (header_length)
    *header_length = rtp_hdr_len_without_extn + kRtpExtnHdrLen + extn_length;
  return true;
}

void UpdateAbsSendTimeExtnValue(char* extn_data, int len,
                                uint32 abs_send_time) {
  // Absolute send time in RTP streams.
  //
  // The absolute send time is signaled to the receiver in-band using the
  // general mechanism for RTP header extensions [RFC5285]. The payload
  // of this extension (the transmitted value) is a 24-bit unsigned integer
  // containing the sender's current time in seconds as a fixed point number
  // with 18 bits fractional part.
  //
  // The form of the absolute send time extension block:
  //
  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |  ID   | len=2 |              absolute send time               |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  DCHECK_EQ(len, kAbsSendTimeExtnLen);
  // Now() has resolution ~1-15ms, using HighResNow(). But it is warned not to
  // use it unless necessary, as it is expensive than Now().
  uint32 now_second = abs_send_time;
  if (!now_second) {
    uint64 now_us =
        (base::TimeTicks::HighResNow() - base::TimeTicks()).InMicroseconds();
    // Convert second to 24-bit unsigned with 18 bit fractional part
    now_second =
        ((now_us << 18) / base::Time::kMicrosecondsPerSecond) & 0x00FFFFFF;
  }
  // TODO(mallinath) - Add SetBE24 to byteorder.h in libjingle.
  extn_data[0] = static_cast<uint8>(now_second >> 16);
  extn_data[1] = static_cast<uint8>(now_second >> 8);
  extn_data[2] = static_cast<uint8>(now_second);
}

// Assumes |len| is actual packet length + tag length. Updates HMAC at end of
// the RTP packet.
void UpdateRtpAuthTag(char* rtp, int len,
                      const talk_base::PacketOptions& options) {
  // If there is no key, return.
  if (options.packet_time_params.srtp_auth_key.empty())
    return;

  size_t tag_length = options.packet_time_params.srtp_auth_tag_len;
  char* auth_tag = rtp + (len - tag_length);

  // We should have a fake HMAC value @ auth_tag.
  DCHECK_EQ(0, memcmp(auth_tag, kFakeAuthTag, tag_length));

  crypto::HMAC hmac(crypto::HMAC::SHA1);
  if (!hmac.Init(reinterpret_cast<const unsigned char*>(
        &options.packet_time_params.srtp_auth_key[0]),
        options.packet_time_params.srtp_auth_key.size())) {
    NOTREACHED();
    return;
  }

  if (hmac.DigestLength() < tag_length) {
    NOTREACHED();
    return;
  }

  // Copy ROC after end of rtp packet.
  memcpy(auth_tag, &options.packet_time_params.srtp_packet_index, 4);
  // Authentication of a RTP packet will have RTP packet + ROC size.
  int auth_required_length = len - tag_length + 4;

  unsigned char output[64];
  if (!hmac.Sign(base::StringPiece(rtp, auth_required_length),
                 output, sizeof(output))) {
    NOTREACHED();
    return;
  }
  // Copy HMAC from output to packet. This is required as auth tag length
  // may not be equal to the actual HMAC length.
  memcpy(auth_tag, output, tag_length);
}

}  // namespace

namespace content {

namespace packet_processing_helpers {

bool ApplyPacketOptions(char* data, int length,
                        const talk_base::PacketOptions& options,
                        uint32 abs_send_time) {
  DCHECK(data != NULL);
  DCHECK(length > 0);
  // if there is no valid |rtp_sendtime_extension_id| and |srtp_auth_key| in
  // PacketOptions, nothing to be updated in this packet.
  if (options.packet_time_params.rtp_sendtime_extension_id == -1 &&
      options.packet_time_params.srtp_auth_key.empty()) {
    return true;
  }

  DCHECK(!IsDtlsPacket(data, length));
  DCHECK(!IsRtcpPacket(data));

  // If there is a srtp auth key present then packet must be a RTP packet.
  // RTP packet may have been wrapped in a TURN Channel Data or
  // TURN send indication.
  int rtp_start_pos;
  int rtp_length;
  if (!GetRtpPacketStartPositionAndLength(
      data, length, &rtp_start_pos, &rtp_length)) {
    // This method should never return false.
    NOTREACHED();
    return false;
  }

  // Skip to rtp packet.
  char* start = data + rtp_start_pos;
  // If packet option has non default value (-1) for sendtime extension id,
  // then we should parse the rtp packet to update the timestamp. Otherwise
  // just calculate HMAC and update packet with it.
  if (options.packet_time_params.rtp_sendtime_extension_id != -1) {
    UpdateRtpAbsSendTimeExtn(
        start, rtp_length,
        options.packet_time_params.rtp_sendtime_extension_id, abs_send_time);
  }

  UpdateRtpAuthTag(start, rtp_length, options);
  return true;
}

bool GetRtpPacketStartPositionAndLength(const char* packet,
                                        int length,
                                        int* rtp_start_pos,
                                        int* rtp_packet_length) {
  int rtp_begin, rtp_length;
  if (IsTurnChannelData(packet)) {
    // Turn Channel Message header format.
    //   0                   1                   2                   3
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |         Channel Number        |            Length             |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |                                                               |
    // /                       Application Data                        /
    // /                                                               /
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    if (length < kTurnChannelHdrLen) {
      return false;
    }

    rtp_begin = kTurnChannelHdrLen;
    rtp_length = talk_base::GetBE16(&packet[2]);
    if (length < rtp_length + kTurnChannelHdrLen) {
      return false;
    }
  } else if (IsTurnSendIndicationPacket(packet)) {
    if (length <= P2PSocketHost::kStunHeaderSize) {
      // Message must be greater than 20 bytes, if it's carrying any payload.
      return false;
    }
    // Validate STUN message length.
    int stun_msg_len = talk_base::GetBE16(&packet[2]);
    if (stun_msg_len + P2PSocketHost::kStunHeaderSize != length) {
      return false;
    }

    // First skip mandatory stun header which is of 20 bytes.
    rtp_begin = P2PSocketHost::kStunHeaderSize;
    // Loop through STUN attributes until we find STUN DATA attribute.
    const char* start = packet + rtp_begin;
    bool data_attr_present = false;
    while ((packet + rtp_begin) - start < stun_msg_len) {
      // Keep reading STUN attributes until we hit DATA attribute.
      // Attribute will be a TLV structure.
      // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      // |         Type                  |            Length             |
      // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      // |                         Value (variable)                ....
      // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      // The value in the length field MUST contain the length of the Value
      // part of the attribute, prior to padding, measured in bytes.  Since
      // STUN aligns attributes on 32-bit boundaries, attributes whose content
      // is not a multiple of 4 bytes are padded with 1, 2, or 3 bytes of
      // padding so that its value contains a multiple of 4 bytes.  The
      // padding bits are ignored, and may be any value.
      uint16 attr_type, attr_length;
      // Getting attribute type and length.
      attr_type = talk_base::GetBE16(&packet[rtp_begin]);
      attr_length = talk_base::GetBE16(
          &packet[rtp_begin + sizeof(attr_type)]);
      // Checking for bogus attribute length.
      if (length < attr_length + rtp_begin) {
        return false;
      }

      if (attr_type != cricket::STUN_ATTR_DATA) {
        rtp_begin += sizeof(attr_type) + sizeof(attr_length) + attr_length;
        if ((attr_length % 4) != 0) {
          rtp_begin += (4 - (attr_length % 4));
        }
        continue;
      }

      data_attr_present = true;
      rtp_begin += 4;  // Skip STUN_DATA_ATTR header.
      rtp_length = attr_length;
      // One final check of length before exiting.
      if (length < rtp_length + rtp_begin) {
        return false;
      }
      // We found STUN_DATA_ATTR. We can skip parsing rest of the packet.
      break;
    }

    if (!data_attr_present) {
      // There is no data attribute present in the message. We can't do anything
      // with the message.
      return false;
    }

  } else {
    // This is a raw RTP packet.
    rtp_begin = 0;
    rtp_length = length;
  }

  // Making sure we have a valid RTP packet at the end.
  if (!(rtp_length < kMinRtpHdrLen) &&
      IsRtpPacket(packet + rtp_begin, rtp_length) &&
      ValidateRtpHeader(packet + rtp_begin, rtp_length, NULL)) {
    *rtp_start_pos = rtp_begin;
    *rtp_packet_length = rtp_length;
    return true;
  }
  return false;
}

// ValidateRtpHeader must be called before this method to make sure, we have
// a sane rtp packet.
bool UpdateRtpAbsSendTimeExtn(char* rtp, int length,
                              int extension_id, uint32 abs_send_time) {
  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |V=2|P|X|  CC   |M|     PT      |       sequence number         |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                           timestamp                           |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |           synchronization source (SSRC) identifier            |
  // +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  // |            contributing source (CSRC) identifiers             |
  // |                             ....                              |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  // Return if extension bit is not set.
  if (!(rtp[0] & 0x10)) {
    return true;
  }

  int cc_count = rtp[0] & 0x0F;
  int rtp_hdr_len_without_extn = kMinRtpHdrLen + 4 * cc_count;

  rtp += rtp_hdr_len_without_extn;

  // Getting extension profile ID and length.
  uint16 profile_id = talk_base::GetBE16(rtp);
  // Length is in 32 bit words.
  uint16 extn_length = talk_base::GetBE16(rtp + 2) * 4;

  rtp += kRtpExtnHdrLen;  // Moving past extn header.

  bool found = false;
  // WebRTC is using one byte header extension.
  // TODO(mallinath) - Handle two byte header extension.
  if (profile_id == 0xBEDE) {  // OneByte extension header
    //  0
    //  0 1 2 3 4 5 6 7
    // +-+-+-+-+-+-+-+-+
    // |  ID   |  len  |
    // +-+-+-+-+-+-+-+-+

    //  0                   1                   2                   3
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |       0xBE    |    0xDE       |           length=3            |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |  ID   | L=0   |     data      |  ID   |  L=1  |   data...
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //       ...data   |    0 (pad)    |    0 (pad)    |  ID   | L=3   |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |                          data                                 |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    char* extn_start = rtp;
    while (rtp - extn_start < extn_length) {
      const int id = (*rtp & 0xF0) >> 4;
      const int len = (*rtp & 0x0F) + 1;
      // The 4-bit length is the number minus one of data bytes of this header
      // extension element following the one-byte header.
      if (id == extension_id) {
        UpdateAbsSendTimeExtnValue(rtp + kOneByteHdrLen, len, abs_send_time);
        found = true;
        break;
      }
      rtp += kOneByteHdrLen + len;
      // Counting padding bytes.
      while ((*rtp == 0) && (rtp - extn_start < extn_length)) {
        ++rtp;
      }
    }
  }
  return found;
}

}  // packet_processing_helpers

P2PSocketHost::P2PSocketHost(IPC::Sender* message_sender, int socket_id)
    : message_sender_(message_sender),
      id_(socket_id),
      state_(STATE_UNINITIALIZED),
      dump_incoming_rtp_packet_(false),
      dump_outgoing_rtp_packet_(false),
      weak_ptr_factory_(this) {
}

P2PSocketHost::~P2PSocketHost() { }

// Verifies that the packet |data| has a valid STUN header.
// static
bool P2PSocketHost::GetStunPacketType(
    const char* data, int data_size, StunMessageType* type) {

  if (data_size < kStunHeaderSize)
    return false;

  uint32 cookie = base::NetToHost32(*reinterpret_cast<const uint32*>(data + 4));
  if (cookie != kStunMagicCookie)
    return false;

  uint16 length = base::NetToHost16(*reinterpret_cast<const uint16*>(data + 2));
  if (length != data_size - kStunHeaderSize)
    return false;

  int message_type = base::NetToHost16(*reinterpret_cast<const uint16*>(data));

  // Verify that the type is known:
  switch (message_type) {
    case STUN_BINDING_REQUEST:
    case STUN_BINDING_RESPONSE:
    case STUN_BINDING_ERROR_RESPONSE:
    case STUN_SHARED_SECRET_REQUEST:
    case STUN_SHARED_SECRET_RESPONSE:
    case STUN_SHARED_SECRET_ERROR_RESPONSE:
    case STUN_ALLOCATE_REQUEST:
    case STUN_ALLOCATE_RESPONSE:
    case STUN_ALLOCATE_ERROR_RESPONSE:
    case STUN_SEND_REQUEST:
    case STUN_SEND_RESPONSE:
    case STUN_SEND_ERROR_RESPONSE:
    case STUN_DATA_INDICATION:
      *type = static_cast<StunMessageType>(message_type);
      return true;

    default:
      return false;
  }
}

// static
bool P2PSocketHost::IsRequestOrResponse(StunMessageType type) {
  return type == STUN_BINDING_REQUEST || type == STUN_BINDING_RESPONSE ||
      type == STUN_ALLOCATE_REQUEST || type == STUN_ALLOCATE_RESPONSE;
}

// static
P2PSocketHost* P2PSocketHost::Create(IPC::Sender* message_sender,
                                     int socket_id,
                                     P2PSocketType type,
                                     net::URLRequestContextGetter* url_context,
                                     P2PMessageThrottler* throttler) {
  switch (type) {
    case P2P_SOCKET_UDP:
      return new P2PSocketHostUdp(message_sender, socket_id, throttler);
    case P2P_SOCKET_TCP_SERVER:
      return new P2PSocketHostTcpServer(
          message_sender, socket_id, P2P_SOCKET_TCP_CLIENT);

    case P2P_SOCKET_STUN_TCP_SERVER:
      return new P2PSocketHostTcpServer(
          message_sender, socket_id, P2P_SOCKET_STUN_TCP_CLIENT);

    case P2P_SOCKET_TCP_CLIENT:
    case P2P_SOCKET_SSLTCP_CLIENT:
    case P2P_SOCKET_TLS_CLIENT:
      return new P2PSocketHostTcp(message_sender, socket_id, type, url_context);

    case P2P_SOCKET_STUN_TCP_CLIENT:
    case P2P_SOCKET_STUN_SSLTCP_CLIENT:
    case P2P_SOCKET_STUN_TLS_CLIENT:
      return new P2PSocketHostStunTcp(
          message_sender, socket_id, type, url_context);
  }

  NOTREACHED();
  return NULL;
}

void P2PSocketHost::StartRtpDump(
    bool incoming,
    bool outgoing,
    const RenderProcessHost::WebRtcRtpPacketCallback& packet_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!packet_callback.is_null());
  DCHECK(incoming || outgoing);

  if (incoming)
    dump_incoming_rtp_packet_ = true;

  if (outgoing)
    dump_outgoing_rtp_packet_ = true;

  packet_dump_callback_ = packet_callback;
}

void P2PSocketHost::StopRtpDump(bool incoming, bool outgoing) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(incoming || outgoing);

  if (incoming)
    dump_incoming_rtp_packet_ = false;

  if (outgoing)
    dump_outgoing_rtp_packet_ = false;

  if (!dump_incoming_rtp_packet_ && !dump_outgoing_rtp_packet_)
    packet_dump_callback_.Reset();
}

void P2PSocketHost::DumpRtpPacket(const char* packet,
                                  size_t length,
                                  bool incoming) {
  if (IsDtlsPacket(packet, length) || IsRtcpPacket(packet))
    return;

  int rtp_packet_pos = 0;
  int rtp_packet_length = length;
  if (!packet_processing_helpers::GetRtpPacketStartPositionAndLength(
          packet, length, &rtp_packet_pos, &rtp_packet_length))
    return;

  packet += rtp_packet_pos;

  size_t header_length = 0;
  bool valid = ValidateRtpHeader(packet, rtp_packet_length, &header_length);
  if (!valid) {
    DCHECK(false);
    return;
  }

  scoped_ptr<uint8[]> header_buffer(new uint8[header_length]);
  memcpy(header_buffer.get(), packet, header_length);

  // Posts to the IO thread as the data members should be accessed on the IO
  // thread only.
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&P2PSocketHost::DumpRtpPacketOnIOThread,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     Passed(&header_buffer),
                                     header_length,
                                     rtp_packet_length,
                                     incoming));
}

void P2PSocketHost::DumpRtpPacketOnIOThread(scoped_ptr<uint8[]> packet_header,
                                            size_t header_length,
                                            size_t packet_length,
                                            bool incoming) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if ((incoming && !dump_incoming_rtp_packet_) ||
      (!incoming && !dump_outgoing_rtp_packet_) ||
      packet_dump_callback_.is_null()) {
    return;
  }

  // |packet_dump_callback_| must be called on the UI thread.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(packet_dump_callback_,
                                     Passed(&packet_header),
                                     header_length,
                                     packet_length,
                                     incoming));
}

}  // namespace content
