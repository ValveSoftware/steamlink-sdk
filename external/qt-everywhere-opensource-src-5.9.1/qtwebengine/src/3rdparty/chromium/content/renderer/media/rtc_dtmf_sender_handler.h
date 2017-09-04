// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_DTMF_SENDER_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_RTC_DTMF_SENDER_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebRTCDTMFSenderHandler.h"
#include "third_party/WebKit/public/platform/WebRTCDTMFSenderHandlerClient.h"
#include "third_party/webrtc/api/dtmfsenderinterface.h"

namespace content {

// RtcDtmfSenderHandler is a delegate for the RTC DTMF Sender API messages
// going between WebKit and native DTMF Sender in libjingle.
// Instances of this class are owned by WebKit.
// WebKit call all of these methods on the main render thread.
// Callbacks to the webrtc::DtmfSenderObserverInterface implementation also
// occur on the main render thread.
class CONTENT_EXPORT RtcDtmfSenderHandler
    : NON_EXPORTED_BASE(public blink::WebRTCDTMFSenderHandler),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  explicit RtcDtmfSenderHandler(webrtc::DtmfSenderInterface* dtmf_sender);
  ~RtcDtmfSenderHandler() override;

  // blink::WebRTCDTMFSenderHandler implementation.
  void setClient(blink::WebRTCDTMFSenderHandlerClient* client) override;
  blink::WebString currentToneBuffer() override;
  bool canInsertDTMF() override;
  bool insertDTMF(const blink::WebString& tones,
                  long duration,
                  long interToneGap) override;

  void OnToneChange(const std::string& tone);

 private:
  scoped_refptr<webrtc::DtmfSenderInterface> dtmf_sender_;
  blink::WebRTCDTMFSenderHandlerClient* webkit_client_;
  class Observer;
  scoped_refptr<Observer> observer_;

  // |weak_factory_| must be the last member.
  base::WeakPtrFactory<RtcDtmfSenderHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RtcDtmfSenderHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_DTMF_SENDER_HANDLER_H_

