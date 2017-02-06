// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_dtmf_sender_handler.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"

using webrtc::DtmfSenderInterface;

namespace content {

class RtcDtmfSenderHandler::Observer :
    public base::RefCountedThreadSafe<Observer>,
    public webrtc::DtmfSenderObserverInterface {
 public:
  explicit Observer(const base::WeakPtr<RtcDtmfSenderHandler>& handler)
      : main_thread_(base::ThreadTaskRunnerHandle::Get()), handler_(handler) {}

 private:
  friend class base::RefCountedThreadSafe<Observer>;

  ~Observer() override {}

  void OnToneChange(const std::string& tone) override {
    main_thread_->PostTask(FROM_HERE,
        base::Bind(&RtcDtmfSenderHandler::Observer::OnToneChangeOnMainThread,
                   this, tone));
  }

  void OnToneChangeOnMainThread(const std::string& tone) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (handler_)
      handler_->OnToneChange(tone);
  }

  base::ThreadChecker thread_checker_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  base::WeakPtr<RtcDtmfSenderHandler> handler_;
};

RtcDtmfSenderHandler::RtcDtmfSenderHandler(DtmfSenderInterface* dtmf_sender)
    : dtmf_sender_(dtmf_sender),
      webkit_client_(NULL),
      weak_factory_(this) {
  DVLOG(1) << "::ctor";
  observer_ = new Observer(weak_factory_.GetWeakPtr());
  dtmf_sender_->RegisterObserver(observer_.get());
}

RtcDtmfSenderHandler::~RtcDtmfSenderHandler() {
  DVLOG(1) << "::dtor";
  dtmf_sender_->UnregisterObserver();
  // Release |observer| before |weak_factory_| is destroyed.
  observer_ = NULL;
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
  std::string utf8_tones = base::UTF16ToUTF8(base::StringPiece16(tones));
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

