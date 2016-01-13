// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_DTMF_SENDER_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_RTC_DTMF_SENDER_HANDLER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebRTCDTMFSenderHandler.h"
#include "third_party/WebKit/public/platform/WebRTCDTMFSenderHandlerClient.h"
#include "third_party/libjingle/source/talk/app/webrtc/dtmfsenderinterface.h"

namespace content {

// RtcDtmfSenderHandler is a delegate for the RTC DTMF Sender API messages
// going between WebKit and native DTMF Sender in libjingle.
// Instances of this class are owned by WebKit.
// WebKit call all of these methods on the main render thread.
// Callbacks to the webrtc::DtmfSenderObserverInterface implementation also
// occur on the main render thread.
class CONTENT_EXPORT RtcDtmfSenderHandler
    : NON_EXPORTED_BASE(public blink::WebRTCDTMFSenderHandler),
      NON_EXPORTED_BASE(public webrtc::DtmfSenderObserverInterface),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  explicit RtcDtmfSenderHandler(webrtc::DtmfSenderInterface* dtmf_sender);
  virtual ~RtcDtmfSenderHandler();

  // blink::WebRTCDTMFSenderHandler implementation.
  virtual void setClient(
      blink::WebRTCDTMFSenderHandlerClient* client) OVERRIDE;
  virtual blink::WebString currentToneBuffer() OVERRIDE;
  virtual bool canInsertDTMF() OVERRIDE;
  virtual bool insertDTMF(const blink::WebString& tones, long duration,
                          long interToneGap) OVERRIDE;

  // webrtc::DtmfSenderObserverInterface implementation.
  virtual void OnToneChange(const std::string& tone) OVERRIDE;

 private:
  scoped_refptr<webrtc::DtmfSenderInterface> dtmf_sender_;
  blink::WebRTCDTMFSenderHandlerClient* webkit_client_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_DTMF_SENDER_HANDLER_H_

