// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/message_port_provider.h"

#include "content/browser/browser_thread_impl.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/message_port_delegate.h"

#if defined(OS_ANDROID)
#include "content/browser/android/app_web_message_port_service_impl.h"
#endif

namespace content {

// static
void MessagePortProvider::PostMessageToFrame(
    WebContents* web_contents,
    const base::string16& source_origin,
    const base::string16& target_origin,
    const base::string16& data,
    const std::vector<int>& ports) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(OS_ANDROID)
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&content::AppWebMessagePortServiceImpl::RemoveSentPorts,
                 base::Unretained(AppWebMessagePortServiceImpl::GetInstance()),
                 ports));
#endif
  FrameMsg_PostMessage_Params params;
  params.is_data_raw_string = true;
  params.data = data;
  params.source_routing_id = MSG_ROUTING_NONE;
  params.source_origin = source_origin;
  params.target_origin = target_origin;
  params.message_ports = ports;

  RenderProcessHostImpl* rph =
      static_cast<RenderProcessHostImpl*>(web_contents->GetRenderProcessHost());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MessagePortMessageFilter::RouteMessageEventWithMessagePorts,
                 rph->message_port_message_filter(),
                 web_contents->GetMainFrame()->GetRoutingID(), params));
}

#if defined(OS_ANDROID)
// static
AppWebMessagePortService* MessagePortProvider::GetAppWebMessagePortService() {
  return AppWebMessagePortServiceImpl::GetInstance();
}
#endif

} // namespace content
