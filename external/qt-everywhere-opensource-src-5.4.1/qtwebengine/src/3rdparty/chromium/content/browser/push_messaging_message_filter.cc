// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/push_messaging_message_filter.h"

#include <string>

#include "base/bind.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/push_messaging_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/push_messaging_service.h"

namespace content {

PushMessagingMessageFilter::PushMessagingMessageFilter(int render_process_id)
    : BrowserMessageFilter(PushMessagingMsgStart),
      render_process_id_(render_process_id),
      service_(NULL),
      weak_factory_(this) {}

PushMessagingMessageFilter::~PushMessagingMessageFilter() {}

bool PushMessagingMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PushMessagingMessageFilter, message)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_Register, OnRegister)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PushMessagingMessageFilter::OnRegister(int routing_id,
                                            int callbacks_id,
                                            const std::string& sender_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // TODO(mvanouwerkerk): Validate arguments?
  // TODO(mvanouwerkerk): A WebContentsObserver could avoid this PostTask
  //                      by receiving the IPC on the UI thread.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&PushMessagingMessageFilter::DoRegister,
                                     weak_factory_.GetWeakPtr(),
                                     routing_id,
                                     callbacks_id,
                                     sender_id));
}

void PushMessagingMessageFilter::DoRegister(int routing_id,
                                            int callbacks_id,
                                            const std::string& sender_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!service()) {
    DidRegister(routing_id, callbacks_id, GURL(), "", false);
    return;
  }
  // TODO(mvanouwerkerk): Pass in a real app ID based on Service Worker ID.
  std::string app_id = "https://example.com 0";
  service_->Register(app_id,
                     sender_id,
                     base::Bind(&PushMessagingMessageFilter::DidRegister,
                                weak_factory_.GetWeakPtr(),
                                routing_id,
                                callbacks_id));
}

void PushMessagingMessageFilter::DidRegister(int routing_id,
                                             int callbacks_id,
                                             const GURL& endpoint,
                                             const std::string& registration_id,
                                             bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (success) {
    Send(new PushMessagingMsg_RegisterSuccess(routing_id,
                                              callbacks_id,
                                              endpoint,
                                              registration_id));
  } else {
    Send(new PushMessagingMsg_RegisterError(routing_id, callbacks_id));
  }
}

PushMessagingService* PushMessagingMessageFilter::service() {
  if (!service_) {
    RenderProcessHostImpl* host = static_cast<RenderProcessHostImpl*>(
        RenderProcessHost::FromID(render_process_id_));
    if (!host)
      return NULL;
    service_ = host->GetBrowserContext()->GetPushMessagingService();
  }
  return service_;
}

}  // namespace content
