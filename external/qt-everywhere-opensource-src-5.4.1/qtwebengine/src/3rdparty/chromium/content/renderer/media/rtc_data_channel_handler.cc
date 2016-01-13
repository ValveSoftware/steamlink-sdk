// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_data_channel_handler.h"

#include <limits>
#include <string>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"

namespace content {

namespace {

enum DataChannelCounters {
  CHANNEL_CREATED,
  CHANNEL_OPENED,
  CHANNEL_RELIABLE,
  CHANNEL_ORDERED,
  CHANNEL_NEGOTIATED,
  CHANNEL_BOUNDARY
};

void IncrementCounter(DataChannelCounters counter) {
  UMA_HISTOGRAM_ENUMERATION("WebRTC.DataChannelCounters",
                            counter,
                            CHANNEL_BOUNDARY);
}

}  // namespace

RtcDataChannelHandler::RtcDataChannelHandler(
    webrtc::DataChannelInterface* channel)
    : channel_(channel),
      webkit_client_(NULL) {
  DVLOG(1) << "::ctor";
  channel_->RegisterObserver(this);

  IncrementCounter(CHANNEL_CREATED);
  if (isReliable())
    IncrementCounter(CHANNEL_RELIABLE);
  if (ordered())
    IncrementCounter(CHANNEL_ORDERED);
  if (negotiated())
    IncrementCounter(CHANNEL_NEGOTIATED);

  UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.DataChannelMaxRetransmits",
                              maxRetransmits(), 0,
                              std::numeric_limits<unsigned short>::max(), 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.DataChannelMaxRetransmitTime",
                              maxRetransmitTime(), 0,
                              std::numeric_limits<unsigned short>::max(), 50);
}

RtcDataChannelHandler::~RtcDataChannelHandler() {
  DVLOG(1) << "::dtor";
  channel_->UnregisterObserver();
}

void RtcDataChannelHandler::setClient(
    blink::WebRTCDataChannelHandlerClient* client) {
  webkit_client_ = client;
}

blink::WebString RtcDataChannelHandler::label() {
  return base::UTF8ToUTF16(channel_->label());
}

bool RtcDataChannelHandler::isReliable() {
  return channel_->reliable();
}

bool RtcDataChannelHandler::ordered() const {
  return channel_->ordered();
}

unsigned short RtcDataChannelHandler::maxRetransmitTime() const {
  return channel_->maxRetransmitTime();
}

unsigned short RtcDataChannelHandler::maxRetransmits() const {
  return channel_->maxRetransmits();
}

blink::WebString RtcDataChannelHandler::protocol() const {
  return base::UTF8ToUTF16(channel_->protocol());
}

bool RtcDataChannelHandler::negotiated() const {
  return channel_->negotiated();
}

unsigned short RtcDataChannelHandler::id() const {
  return channel_->id();
}

unsigned long RtcDataChannelHandler::bufferedAmount() {
  return channel_->buffered_amount();
}

bool RtcDataChannelHandler::sendStringData(const blink::WebString& data) {
  std::string utf8_buffer = base::UTF16ToUTF8(data);
  talk_base::Buffer buffer(utf8_buffer.c_str(), utf8_buffer.length());
  webrtc::DataBuffer data_buffer(buffer, false);
  RecordMessageSent(data_buffer.size());
  return channel_->Send(data_buffer);
}

bool RtcDataChannelHandler::sendRawData(const char* data, size_t length) {
  talk_base::Buffer buffer(data, length);
  webrtc::DataBuffer data_buffer(buffer, true);
  RecordMessageSent(data_buffer.size());
  return channel_->Send(data_buffer);
}

void RtcDataChannelHandler::close() {
  channel_->Close();
}

void RtcDataChannelHandler::OnStateChange() {
  if (!webkit_client_) {
    LOG(ERROR) << "WebRTCDataChannelHandlerClient not set.";
    return;
  }
  DVLOG(1) << "OnStateChange " << channel_->state();
  switch (channel_->state()) {
    case webrtc::DataChannelInterface::kConnecting:
      webkit_client_->didChangeReadyState(
          blink::WebRTCDataChannelHandlerClient::ReadyStateConnecting);
      break;
    case webrtc::DataChannelInterface::kOpen:
      IncrementCounter(CHANNEL_OPENED);
      webkit_client_->didChangeReadyState(
          blink::WebRTCDataChannelHandlerClient::ReadyStateOpen);
      break;
    case webrtc::DataChannelInterface::kClosing:
      webkit_client_->didChangeReadyState(
          blink::WebRTCDataChannelHandlerClient::ReadyStateClosing);
      break;
    case webrtc::DataChannelInterface::kClosed:
      webkit_client_->didChangeReadyState(
          blink::WebRTCDataChannelHandlerClient::ReadyStateClosed);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void RtcDataChannelHandler::OnMessage(const webrtc::DataBuffer& buffer) {
  if (!webkit_client_) {
    LOG(ERROR) << "WebRTCDataChannelHandlerClient not set.";
    return;
  }

  if (buffer.binary) {
    webkit_client_->didReceiveRawData(buffer.data.data(), buffer.data.length());
  } else {
    base::string16 utf16;
    if (!base::UTF8ToUTF16(buffer.data.data(), buffer.data.length(), &utf16)) {
      LOG(ERROR) << "Failed convert received data to UTF16";
      return;
    }
    webkit_client_->didReceiveStringData(utf16);
  }
}

void RtcDataChannelHandler::RecordMessageSent(size_t num_bytes) {
  // Currently, messages are capped at some fairly low limit (16 Kb?)
  // but we may allow unlimited-size messages at some point, so making
  // the histogram maximum quite large (100 Mb) to have some
  // granularity at the higher end in that eventuality. The histogram
  // buckets are exponentially growing in size, so we'll still have
  // good granularity at the low end.

  // This makes the last bucket in the histogram count messages from
  // 100 Mb to infinity.
  const int kMaxBucketSize = 100 * 1024 * 1024;
  const int kNumBuckets = 50;

  if (isReliable()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.ReliableDataChannelMessageSize",
                                num_bytes,
                                1, kMaxBucketSize, kNumBuckets);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.UnreliableDataChannelMessageSize",
                                num_bytes,
                                1, kMaxBucketSize, kNumBuckets);
  }
}

}  // namespace content
