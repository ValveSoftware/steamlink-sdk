// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_service_worker_message_filter.h"

#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/extension_messages.h"

namespace extensions {

ExtensionServiceWorkerMessageFilter::ExtensionServiceWorkerMessageFilter(
    int render_process_id,
    content::BrowserContext* context)
    : content::BrowserMessageFilter(ExtensionWorkerMsgStart),
      render_process_id_(render_process_id),
      dispatcher_(new ExtensionFunctionDispatcher(context)) {}

ExtensionServiceWorkerMessageFilter::~ExtensionServiceWorkerMessageFilter() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void ExtensionServiceWorkerMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    content::BrowserThread::ID* thread) {
  if (message.type() == ExtensionHostMsg_RequestWorker::ID) {
    *thread = content::BrowserThread::UI;
  }
}

bool ExtensionServiceWorkerMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionServiceWorkerMessageFilter, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RequestWorker, OnRequestWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionServiceWorkerMessageFilter::OnRequestWorker(
    const ExtensionHostMsg_Request_Params& params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dispatcher_->Dispatch(params, nullptr, render_process_id_);
}

}  // namespace extensions
