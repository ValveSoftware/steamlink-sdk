// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_WEBRTC_INTERNALS_MESSAGE_HANDLER_H_
#define CONTENT_BROWSER_MEDIA_WEBRTC_INTERNALS_MESSAGE_HANDLER_H_

#include "base/memory/ref_counted.h"
#include "content/browser/media/webrtc_internals_ui_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

namespace content {

// This class handles messages to and from WebRTCInternalsUI.
// It delegates all its work to WebRTCInternalsProxy on the IO thread.
class WebRTCInternalsMessageHandler : public WebUIMessageHandler,
                                      public WebRTCInternalsUIObserver{
 public:
  WebRTCInternalsMessageHandler();
  virtual ~WebRTCInternalsMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // WebRTCInternalsUIObserver override.
  virtual void OnUpdate(const std::string& command,
                        const base::Value* args) OVERRIDE;

 private:
  // Javascript message handler.
  void OnGetAllStats(const base::ListValue* list);
  void OnSetAecRecordingEnabled(bool enable, const base::ListValue* list);
  void OnDOMLoadDone(const base::ListValue* list);

  DISALLOW_COPY_AND_ASSIGN(WebRTCInternalsMessageHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_WEBRTC_INTERNALS_MESSAGE_HANDLER_H_
