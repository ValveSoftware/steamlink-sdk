// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_internals_message_handler.h"

#include "content/browser/webrtc/webrtc_internals.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"

namespace content {

WebRTCInternalsMessageHandler::WebRTCInternalsMessageHandler()
    : WebRTCInternalsMessageHandler(WebRTCInternals::GetInstance()) {}

WebRTCInternalsMessageHandler::WebRTCInternalsMessageHandler(
    WebRTCInternals* webrtc_internals)
    : webrtc_internals_(webrtc_internals) {
  webrtc_internals_->AddObserver(this);
}

WebRTCInternalsMessageHandler::~WebRTCInternalsMessageHandler() {
  webrtc_internals_->RemoveObserver(this);
}

void WebRTCInternalsMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("getAllStats",
      base::Bind(&WebRTCInternalsMessageHandler::OnGetAllStats,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback("enableAudioDebugRecordings",
      base::Bind(
          &WebRTCInternalsMessageHandler::OnSetAudioDebugRecordingsEnabled,
          base::Unretained(this),
          true));

  web_ui()->RegisterMessageCallback("disableAudioDebugRecordings",
      base::Bind(
          &WebRTCInternalsMessageHandler::OnSetAudioDebugRecordingsEnabled,
          base::Unretained(this),
          false));

  web_ui()->RegisterMessageCallback(
      "enableEventLogRecordings",
      base::Bind(&WebRTCInternalsMessageHandler::OnSetEventLogRecordingsEnabled,
                 base::Unretained(this), true));

  web_ui()->RegisterMessageCallback(
      "disableEventLogRecordings",
      base::Bind(&WebRTCInternalsMessageHandler::OnSetEventLogRecordingsEnabled,
                 base::Unretained(this), false));

  web_ui()->RegisterMessageCallback("finishedDOMLoad",
      base::Bind(&WebRTCInternalsMessageHandler::OnDOMLoadDone,
                 base::Unretained(this)));
}

RenderFrameHost* WebRTCInternalsMessageHandler::GetWebRTCInternalsHost() const {
  RenderFrameHost* host = web_ui()->GetWebContents()->GetMainFrame();
  if (host) {
    // Make sure we only ever execute the script in the webrtc-internals page.
    const GURL url(host->GetLastCommittedURL());
    if (!url.SchemeIs(kChromeUIScheme) ||
        url.host() != kChromeUIWebRTCInternalsHost) {
      // Some other page is currently loaded even though we might be in the
      // process of loading webrtc-internals.  So, the current RFH is not the
      // one we're waiting for.
      host = nullptr;
    }
  }

  return host;
}

void WebRTCInternalsMessageHandler::OnGetAllStats(
    const base::ListValue* /* unused_list */) {
  for (RenderProcessHost::iterator i(
       content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->Send(new PeerConnectionTracker_GetAllStats());
  }
}

void WebRTCInternalsMessageHandler::OnSetAudioDebugRecordingsEnabled(
    bool enable, const base::ListValue* /* unused_list */) {
  if (enable) {
    webrtc_internals_->EnableAudioDebugRecordings(web_ui()->GetWebContents());
  } else {
    webrtc_internals_->DisableAudioDebugRecordings();
  }
}

void WebRTCInternalsMessageHandler::OnSetEventLogRecordingsEnabled(
    bool enable,
    const base::ListValue* /* unused_list */) {
  if (enable) {
    webrtc_internals_->EnableEventLogRecordings(web_ui()->GetWebContents());
  } else {
    webrtc_internals_->DisableEventLogRecordings();
  }
}

void WebRTCInternalsMessageHandler::OnDOMLoadDone(
    const base::ListValue* /* unused_list */) {
  webrtc_internals_->UpdateObserver(this);

  if (webrtc_internals_->IsAudioDebugRecordingsEnabled()) {
    RenderFrameHost* host = GetWebRTCInternalsHost();
    if (!host)
      return;

    std::vector<const base::Value*> args_vector;
    base::string16 script =
        WebUI::GetJavascriptCall("setAudioDebugRecordingsEnabled", args_vector);
    host->ExecuteJavaScript(script);
  }
}

void WebRTCInternalsMessageHandler::OnUpdate(const char* command,
                                             const base::Value* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* host = GetWebRTCInternalsHost();
  if (!host)
    return;

  std::vector<const base::Value*> args_vector;
  if (args)
    args_vector.push_back(args);

  base::string16 update = WebUI::GetJavascriptCall(command, args_vector);
  host->ExecuteJavaScript(update);
}

}  // namespace content
