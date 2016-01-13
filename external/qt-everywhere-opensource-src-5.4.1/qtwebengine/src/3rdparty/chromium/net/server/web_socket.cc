// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/web_socket.h"

#include <limits>

#include "base/base64.h"
#include "base/rand_util.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "net/server/http_connection.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"

namespace net {

namespace {

static uint32 WebSocketKeyFingerprint(const std::string& str) {
  std::string result;
  const char* p_char = str.c_str();
  int length = str.length();
  int spaces = 0;
  for (int i = 0; i < length; ++i) {
    if (p_char[i] >= '0' && p_char[i] <= '9')
      result.append(&p_char[i], 1);
    else if (p_char[i] == ' ')
      spaces++;
  }
  if (spaces == 0)
    return 0;
  int64 number = 0;
  if (!base::StringToInt64(result, &number))
    return 0;
  return base::HostToNet32(static_cast<uint32>(number / spaces));
}

class WebSocketHixie76 : public net::WebSocket {
 public:
  static net::WebSocket* Create(HttpConnection* connection,
                                const HttpServerRequestInfo& request,
                                size_t* pos) {
    if (connection->recv_data().length() < *pos + kWebSocketHandshakeBodyLen)
      return NULL;
    return new WebSocketHixie76(connection, request, pos);
  }

  virtual void Accept(const HttpServerRequestInfo& request) OVERRIDE {
    std::string key1 = request.GetHeaderValue("sec-websocket-key1");
    std::string key2 = request.GetHeaderValue("sec-websocket-key2");

    uint32 fp1 = WebSocketKeyFingerprint(key1);
    uint32 fp2 = WebSocketKeyFingerprint(key2);

    char data[16];
    memcpy(data, &fp1, 4);
    memcpy(data + 4, &fp2, 4);
    memcpy(data + 8, &key3_[0], 8);

    base::MD5Digest digest;
    base::MD5Sum(data, 16, &digest);

    std::string origin = request.GetHeaderValue("origin");
    std::string host = request.GetHeaderValue("host");
    std::string location = "ws://" + host + request.path;
    connection_->Send(base::StringPrintf(
        "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
        "Upgrade: WebSocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Origin: %s\r\n"
        "Sec-WebSocket-Location: %s\r\n"
        "\r\n",
        origin.c_str(),
        location.c_str()));
    connection_->Send(reinterpret_cast<char*>(digest.a), 16);
  }

  virtual ParseResult Read(std::string* message) OVERRIDE {
    DCHECK(message);
    const std::string& data = connection_->recv_data();
    if (data[0])
      return FRAME_ERROR;

    size_t pos = data.find('\377', 1);
    if (pos == std::string::npos)
      return FRAME_INCOMPLETE;

    std::string buffer(data.begin() + 1, data.begin() + pos);
    message->swap(buffer);
    connection_->Shift(pos + 1);

    return FRAME_OK;
  }

  virtual void Send(const std::string& message) OVERRIDE {
    char message_start = 0;
    char message_end = -1;
    connection_->Send(&message_start, 1);
    connection_->Send(message);
    connection_->Send(&message_end, 1);
  }

 private:
  static const int kWebSocketHandshakeBodyLen;

  WebSocketHixie76(HttpConnection* connection,
                   const HttpServerRequestInfo& request,
                   size_t* pos) : WebSocket(connection) {
    std::string key1 = request.GetHeaderValue("sec-websocket-key1");
    std::string key2 = request.GetHeaderValue("sec-websocket-key2");

    if (key1.empty()) {
      connection->Send(HttpServerResponseInfo::CreateFor500(
          "Invalid request format. Sec-WebSocket-Key1 is empty or isn't "
          "specified."));
      return;
    }

    if (key2.empty()) {
      connection->Send(HttpServerResponseInfo::CreateFor500(
          "Invalid request format. Sec-WebSocket-Key2 is empty or isn't "
          "specified."));
      return;
    }

    key3_ = connection->recv_data().substr(
        *pos,
        *pos + kWebSocketHandshakeBodyLen);
    *pos += kWebSocketHandshakeBodyLen;
  }

  std::string key3_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHixie76);
};

const int WebSocketHixie76::kWebSocketHandshakeBodyLen = 8;


// Constants for hybi-10 frame format.

typedef int OpCode;

const OpCode kOpCodeContinuation = 0x0;
const OpCode kOpCodeText = 0x1;
const OpCode kOpCodeBinary = 0x2;
const OpCode kOpCodeClose = 0x8;
const OpCode kOpCodePing = 0x9;
const OpCode kOpCodePong = 0xA;

const unsigned char kFinalBit = 0x80;
const unsigned char kReserved1Bit = 0x40;
const unsigned char kReserved2Bit = 0x20;
const unsigned char kReserved3Bit = 0x10;
const unsigned char kOpCodeMask = 0xF;
const unsigned char kMaskBit = 0x80;
const unsigned char kPayloadLengthMask = 0x7F;

const size_t kMaxSingleBytePayloadLength = 125;
const size_t kTwoBytePayloadLengthField = 126;
const size_t kEightBytePayloadLengthField = 127;
const size_t kMaskingKeyWidthInBytes = 4;

class WebSocketHybi17 : public WebSocket {
 public:
  static WebSocket* Create(HttpConnection* connection,
                           const HttpServerRequestInfo& request,
                           size_t* pos) {
    std::string version = request.GetHeaderValue("sec-websocket-version");
    if (version != "8" && version != "13")
      return NULL;

    std::string key = request.GetHeaderValue("sec-websocket-key");
    if (key.empty()) {
      connection->Send(HttpServerResponseInfo::CreateFor500(
          "Invalid request format. Sec-WebSocket-Key is empty or isn't "
          "specified."));
      return NULL;
    }
    return new WebSocketHybi17(connection, request, pos);
  }

  virtual void Accept(const HttpServerRequestInfo& request) OVERRIDE {
    static const char* const kWebSocketGuid =
        "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string key = request.GetHeaderValue("sec-websocket-key");
    std::string data = base::StringPrintf("%s%s", key.c_str(), kWebSocketGuid);
    std::string encoded_hash;
    base::Base64Encode(base::SHA1HashString(data), &encoded_hash);

    std::string response = base::StringPrintf(
        "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
        "Upgrade: WebSocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        encoded_hash.c_str());
    connection_->Send(response);
  }

  virtual ParseResult Read(std::string* message) OVERRIDE {
    const std::string& frame = connection_->recv_data();
    int bytes_consumed = 0;

    ParseResult result =
        WebSocket::DecodeFrameHybi17(frame, true, &bytes_consumed, message);
    if (result == FRAME_OK)
      connection_->Shift(bytes_consumed);
    if (result == FRAME_CLOSE)
      closed_ = true;
    return result;
  }

  virtual void Send(const std::string& message) OVERRIDE {
    if (closed_)
      return;
    std::string data = WebSocket::EncodeFrameHybi17(message, 0);
    connection_->Send(data);
  }

 private:
  WebSocketHybi17(HttpConnection* connection,
                  const HttpServerRequestInfo& request,
                  size_t* pos)
    : WebSocket(connection),
      op_code_(0),
      final_(false),
      reserved1_(false),
      reserved2_(false),
      reserved3_(false),
      masked_(false),
      payload_(0),
      payload_length_(0),
      frame_end_(0),
      closed_(false) {
  }

  OpCode op_code_;
  bool final_;
  bool reserved1_;
  bool reserved2_;
  bool reserved3_;
  bool masked_;
  const char* payload_;
  size_t payload_length_;
  const char* frame_end_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHybi17);
};

}  // anonymous namespace

WebSocket* WebSocket::CreateWebSocket(HttpConnection* connection,
                                      const HttpServerRequestInfo& request,
                                      size_t* pos) {
  WebSocket* socket = WebSocketHybi17::Create(connection, request, pos);
  if (socket)
    return socket;

  return WebSocketHixie76::Create(connection, request, pos);
}

// static
WebSocket::ParseResult WebSocket::DecodeFrameHybi17(const std::string& frame,
                                                    bool client_frame,
                                                    int* bytes_consumed,
                                                    std::string* output) {
  size_t data_length = frame.length();
  if (data_length < 2)
    return FRAME_INCOMPLETE;

  const char* buffer_begin = const_cast<char*>(frame.data());
  const char* p = buffer_begin;
  const char* buffer_end = p + data_length;

  unsigned char first_byte = *p++;
  unsigned char second_byte = *p++;

  bool final = (first_byte & kFinalBit) != 0;
  bool reserved1 = (first_byte & kReserved1Bit) != 0;
  bool reserved2 = (first_byte & kReserved2Bit) != 0;
  bool reserved3 = (first_byte & kReserved3Bit) != 0;
  int op_code = first_byte & kOpCodeMask;
  bool masked = (second_byte & kMaskBit) != 0;
  if (!final || reserved1 || reserved2 || reserved3)
    return FRAME_ERROR;  // Extensions and not supported.

  bool closed = false;
  switch (op_code) {
  case kOpCodeClose:
    closed = true;
    break;
  case kOpCodeText:
    break;
  case kOpCodeBinary: // We don't support binary frames yet.
  case kOpCodeContinuation: // We don't support binary frames yet.
  case kOpCodePing: // We don't support binary frames yet.
  case kOpCodePong: // We don't support binary frames yet.
  default:
    return FRAME_ERROR;
  }

  if (client_frame && !masked) // In Hybi-17 spec client MUST mask his frame.
    return FRAME_ERROR;

  uint64 payload_length64 = second_byte & kPayloadLengthMask;
  if (payload_length64 > kMaxSingleBytePayloadLength) {
    int extended_payload_length_size;
    if (payload_length64 == kTwoBytePayloadLengthField)
      extended_payload_length_size = 2;
    else {
      DCHECK(payload_length64 == kEightBytePayloadLengthField);
      extended_payload_length_size = 8;
    }
    if (buffer_end - p < extended_payload_length_size)
      return FRAME_INCOMPLETE;
    payload_length64 = 0;
    for (int i = 0; i < extended_payload_length_size; ++i) {
      payload_length64 <<= 8;
      payload_length64 |= static_cast<unsigned char>(*p++);
    }
  }

  size_t actual_masking_key_length = masked ? kMaskingKeyWidthInBytes : 0;
  static const uint64 max_payload_length = 0x7FFFFFFFFFFFFFFFull;
  static size_t max_length = std::numeric_limits<size_t>::max();
  if (payload_length64 > max_payload_length ||
      payload_length64 + actual_masking_key_length > max_length) {
    // WebSocket frame length too large.
    return FRAME_ERROR;
  }
  size_t payload_length = static_cast<size_t>(payload_length64);

  size_t total_length = actual_masking_key_length + payload_length;
  if (static_cast<size_t>(buffer_end - p) < total_length)
    return FRAME_INCOMPLETE;

  if (masked) {
    output->resize(payload_length);
    const char* masking_key = p;
    char* payload = const_cast<char*>(p + kMaskingKeyWidthInBytes);
    for (size_t i = 0; i < payload_length; ++i)  // Unmask the payload.
      (*output)[i] = payload[i] ^ masking_key[i % kMaskingKeyWidthInBytes];
  } else {
    std::string buffer(p, p + payload_length);
    output->swap(buffer);
  }

  size_t pos = p + actual_masking_key_length + payload_length - buffer_begin;
  *bytes_consumed = pos;
  return closed ? FRAME_CLOSE : FRAME_OK;
}

// static
std::string WebSocket::EncodeFrameHybi17(const std::string& message,
                                         int masking_key) {
  std::vector<char> frame;
  OpCode op_code = kOpCodeText;
  size_t data_length = message.length();

  frame.push_back(kFinalBit | op_code);
  char mask_key_bit = masking_key != 0 ? kMaskBit : 0;
  if (data_length <= kMaxSingleBytePayloadLength)
    frame.push_back(data_length | mask_key_bit);
  else if (data_length <= 0xFFFF) {
    frame.push_back(kTwoBytePayloadLengthField | mask_key_bit);
    frame.push_back((data_length & 0xFF00) >> 8);
    frame.push_back(data_length & 0xFF);
  } else {
    frame.push_back(kEightBytePayloadLengthField | mask_key_bit);
    char extended_payload_length[8];
    size_t remaining = data_length;
    // Fill the length into extended_payload_length in the network byte order.
    for (int i = 0; i < 8; ++i) {
      extended_payload_length[7 - i] = remaining & 0xFF;
      remaining >>= 8;
    }
    frame.insert(frame.end(),
                 extended_payload_length,
                 extended_payload_length + 8);
    DCHECK(!remaining);
  }

  const char* data = const_cast<char*>(message.data());
  if (masking_key != 0) {
    const char* mask_bytes = reinterpret_cast<char*>(&masking_key);
    frame.insert(frame.end(), mask_bytes, mask_bytes + 4);
    for (size_t i = 0; i < data_length; ++i)  // Mask the payload.
      frame.push_back(data[i] ^ mask_bytes[i % kMaskingKeyWidthInBytes]);
  } else {
    frame.insert(frame.end(), data, data + data_length);
  }
  return std::string(&frame[0], frame.size());
}

WebSocket::WebSocket(HttpConnection* connection) : connection_(connection) {
}

}  // namespace net
