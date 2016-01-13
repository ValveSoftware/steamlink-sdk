// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_dtmf_sender_handler.h"

#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

using webrtc::DtmfSenderInterface;

namespace content {

RtcDtmfSenderHandler::RtcDtmfSenderHandler(DtmfSenderInterface* dtmf_sender)
    : dtmf_sender_(dtmf_sender),
      webkit_client_(NULL) {
  DVLOG(1) << "::ctor";
  dtmf_sender_->RegisterObserver(this);
}

RtcDtmfSenderHandler::~RtcDtmfSenderHandler() {
  DVLOG(1) << "::dtor";
  dtmf_sender_->UnregisterObserver();
}

void RtcDtmfSenderHandler::setClient(
    blink::WebRTCDTMFSenderHandlerClient* client) {
  webkit_client_ = client;
}

blink::WebString RtcDtmfSenderHandler::currentToneBuffer() {
  return base::UTF8ToUTF16(dtmf_sender_->tones());
}

bool RtcDtmfSenderHandler::canInsertDTMF() {
  return dtmf_sender_->CanInsertDtmf();
}

bool RtcDtmfSenderHandler::insertDTMF(const blink::WebString& tones,
                                      long duration,
                                      long interToneGap) {
  std::string utf8_tones = base::UTF16ToUTF8(tones);
  return dtmf_sender_->InsertDtmf(utf8_tones, static_cast<int>(duration),
                                  static_cast<int>(interToneGap));
}

void RtcDtmfSenderHandler::OnToneChange(const std::string& tone) {
  if (!webkit_client_) {
    LOG(ERROR) << "WebRTCDTMFSenderHandlerClient not set.";
    return;
  }
  webkit_client_->didPlayTone(base::UTF8ToUTF16(tone));
}

}  // namespace content

