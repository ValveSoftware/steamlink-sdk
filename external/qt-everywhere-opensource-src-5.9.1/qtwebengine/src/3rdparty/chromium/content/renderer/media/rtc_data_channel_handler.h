// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_DATA_CHANNEL_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_RTC_DATA_CHANNEL_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandler.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandlerClient.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"

namespace content {

// RtcDataChannelHandler is a delegate for the RTC PeerConnection DataChannel
// API messages going between WebKit and native PeerConnection DataChannels in
// libjingle. Instances of this class are owned by WebKit.
// WebKit call all of these methods on the main render thread.
// Callbacks to the webrtc::DataChannelObserver implementation also occur on
// the main render thread.
class CONTENT_EXPORT RtcDataChannelHandler
    : NON_EXPORTED_BASE(public blink::WebRTCDataChannelHandler) {
 public:
  // This object can* be constructed on libjingle's signaling thread and then
  // ownership is passed to the UI thread where it's eventually given to WebKit.
  // The reason we must construct and hook ourselves up as an observer on the
  // signaling thread is to avoid missing out on any state changes or messages
  // that may occur before we've fully connected with webkit.
  // This period is basically between when the ctor is called and until
  // setClient is called.
  // * For local data channels, the object will be construced on the main thread
  //   and we don't have the issue described above.
  RtcDataChannelHandler(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      webrtc::DataChannelInterface* channel);
  ~RtcDataChannelHandler() override;

  // blink::WebRTCDataChannelHandler implementation.
  void setClient(
      blink::WebRTCDataChannelHandlerClient* client) override;
  blink::WebString label() override;
  bool isReliable() override;
  bool ordered() const override;
  unsigned short maxRetransmitTime() const override;
  unsigned short maxRetransmits() const override;
  blink::WebString protocol() const override;
  bool negotiated() const override;
  unsigned short id() const override;
  blink::WebRTCDataChannelHandlerClient::ReadyState state() const override;
  unsigned long bufferedAmount() override;
  bool sendStringData(const blink::WebString& data) override;
  bool sendRawData(const char* data, size_t length) override;
  void close() override;

  const scoped_refptr<webrtc::DataChannelInterface>& channel() const;

 private:
  void OnStateChange(webrtc::DataChannelInterface::DataState state);
  void OnBufferedAmountDecrease(unsigned previous_amount);
  void OnMessage(std::unique_ptr<webrtc::DataBuffer> buffer);
  void RecordMessageSent(size_t num_bytes);

  class CONTENT_EXPORT Observer
      : public NON_EXPORTED_BASE(
            base::RefCountedThreadSafe<RtcDataChannelHandler::Observer>),
        public NON_EXPORTED_BASE(webrtc::DataChannelObserver) {
   public:
    Observer(RtcDataChannelHandler* handler,
        const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
        webrtc::DataChannelInterface* channel);

    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread() const;
    const scoped_refptr<webrtc::DataChannelInterface>& channel() const;

    // Clears the internal |handler_| pointer so that no further callbacks
    // will be attempted, disassociates this observer from the channel and
    // releases the channel pointer. Must be called on the main thread.
    void Unregister();

   private:
    friend class base::RefCountedThreadSafe<RtcDataChannelHandler::Observer>;
    ~Observer() override;

    // webrtc::DataChannelObserver implementation.
    void OnStateChange() override;
    void OnBufferedAmountChange(uint64_t previous_amount) override;
    void OnMessage(const webrtc::DataBuffer& buffer) override;

    void OnStateChangeImpl(webrtc::DataChannelInterface::DataState state);
    void OnBufferedAmountDecreaseImpl(unsigned previous_amount);
    void OnMessageImpl(std::unique_ptr<webrtc::DataBuffer> buffer);

    RtcDataChannelHandler* handler_;
    const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
    scoped_refptr<webrtc::DataChannelInterface> channel_;
  };

  scoped_refptr<Observer> observer_;
  base::ThreadChecker thread_checker_;

  blink::WebRTCDataChannelHandlerClient* webkit_client_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_DATA_CHANNEL_HANDLER_H_
