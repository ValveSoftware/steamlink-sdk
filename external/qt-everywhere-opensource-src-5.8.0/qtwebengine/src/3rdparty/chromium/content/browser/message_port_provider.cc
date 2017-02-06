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

namespace content {

// static
void MessagePortProvider::PostMessageToFrame(
    WebContents* web_contents,
    const base::string16& source_origin,
    const base::string16& target_origin,
    const base::string16& data,
    const std::vector<int>& ports) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

// static
void MessagePortProvider::CreateMessageChannel(MessagePortDelegate* delegate,
                                               int* port1,
                                               int* port2) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  *port1 = 0;
  *port2 = 0;
  MessagePortService* msp = MessagePortService::GetInstance();
  msp->Create(MSG_ROUTING_NONE, delegate, port1);
  msp->Create(MSG_ROUTING_NONE, delegate, port2);
  // Update the routing number of the message ports to be equal to the message
  // port numbers.
  msp->UpdateMessagePort(*port1, delegate, *port1);
  msp->UpdateMessagePort(*port2, delegate, *port2);
  msp->Entangle(*port1, *port2);
  msp->Entangle(*port2, *port1);
}

// static
void MessagePortProvider::PostMessageToPort(
    int sender_port_id,
    const base::string16& message,
    const std::vector<int>& sent_ports) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MessagePortService* msp = MessagePortService::GetInstance();
  msp->PostMessage(sender_port_id, message, sent_ports);
}

// static
void MessagePortProvider::ClosePort(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MessagePortService* msp = MessagePortService::GetInstance();
  msp->ClosePort(message_port_id);
}

// static
void MessagePortProvider::HoldMessages(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MessagePortService* msp = MessagePortService::GetInstance();
  msp->HoldMessages(message_port_id);
}

// static
void MessagePortProvider::ReleaseMessages(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MessagePortService* msp = MessagePortService::GetInstance();
  msp->ReleaseMessages(message_port_id);
}

// static
void MessagePortProvider::OnMessagePortDelegateClosing(
    MessagePortDelegate* delegate) {
  MessagePortService::GetInstance()->OnMessagePortDelegateClosing(delegate);
}

// static
void MessagePortProvider::UpdateMessagePort(int message_port_id,
                                            MessagePortDelegate* delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MessagePortService::GetInstance()->UpdateMessagePort(message_port_id,
                                                       delegate,
                                                       message_port_id);
}

} // namespace content
