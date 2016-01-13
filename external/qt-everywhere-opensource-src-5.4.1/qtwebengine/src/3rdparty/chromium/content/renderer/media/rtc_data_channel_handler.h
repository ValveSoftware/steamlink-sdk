// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_DATA_CHANNEL_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_RTC_DATA_CHANNEL_HANDLER_H_

#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandler.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandlerClient.h"

namespace content {

// RtcDataChannelHandler is a delegate for the RTC PeerConnection DataChannel
// API messages going between WebKit and native PeerConnection DataChannels in
// libjingle. Instances of this class are owned by WebKit.
// WebKit call all of these methods on the main render thread.
// Callbacks to the webrtc::DataChannelObserver implementation also occur on
// the main render thread.
class CONTENT_EXPORT RtcDataChannelHandler
    : NON_EXPORTED_BASE(public blink::WebRTCDataChannelHandler),
      NON_EXPORTED_BASE(public webrtc::DataChannelObserver),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  explicit RtcDataChannelHandler(webrtc::DataChannelInterface* channel);
  virtual ~RtcDataChannelHandler();

  // blink::WebRTCDataChannelHandler implementation.
  virtual void setClient(
      blink::WebRTCDataChannelHandlerClient* client) OVERRIDE;
  virtual blink::WebString label() OVERRIDE;
  virtual bool isReliable() OVERRIDE;
  virtual bool ordered() const OVERRIDE;
  virtual unsigned short maxRetransmitTime() const OVERRIDE;
  virtual unsigned short maxRetransmits() const OVERRIDE;
  virtual blink::WebString protocol() const OVERRIDE;
  virtual bool negotiated() const OVERRIDE;
  virtual unsigned short id() const OVERRIDE;
  virtual unsigned long bufferedAmount() OVERRIDE;
  virtual bool sendStringData(const blink::WebString& data) OVERRIDE;
  virtual bool sendRawData(const char* data, size_t length) OVERRIDE;
  virtual void close() OVERRIDE;

  // webrtc::DataChannelObserver implementation.
  virtual void OnStateChange() OVERRIDE;
  virtual void OnMessage(const webrtc::DataBuffer& buffer) OVERRIDE;

 private:
  void RecordMessageSent(size_t num_bytes);

  scoped_refptr<webrtc::DataChannelInterface> channel_;
  blink::WebRTCDataChannelHandlerClient* webkit_client_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_DATA_CHANNEL_HANDLER_H_
