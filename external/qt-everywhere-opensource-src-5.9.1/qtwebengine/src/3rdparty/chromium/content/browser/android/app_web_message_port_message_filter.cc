// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/app_web_message_port_message_filter.h"

#include "content/browser/android/app_web_message_port_service_impl.h"
#include "content/browser/message_port_service.h"
#include "content/common/app_web_message_port_messages.h"
#include "content/public/browser/android/app_web_message_port_service.h"
#include "content/public/browser/message_port_provider.h"

using content::BrowserThread;
using content::MessagePortProvider;

namespace content {

AppWebMessagePortMessageFilter::AppWebMessagePortMessageFilter(int route_id)
    : BrowserMessageFilter(AwMessagePortMsgStart), route_id_(route_id) {}

AppWebMessagePortMessageFilter::~AppWebMessagePortMessageFilter() {}

void AppWebMessagePortMessageFilter::OnChannelClosing() {
  MessagePortService::GetInstance()->OnMessagePortDelegateClosing(this);
  AppWebMessagePortServiceImpl::GetInstance()
      ->OnMessagePortMessageFilterClosing(this);
}

bool AppWebMessagePortMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppWebMessagePortMessageFilter, message)
    IPC_MESSAGE_FORWARD(
        AppWebMessagePortHostMsg_ConvertedWebToAppMessage,
        AppWebMessagePortServiceImpl::GetInstance(),
        AppWebMessagePortServiceImpl::OnConvertedWebToAppMessage)
    IPC_MESSAGE_HANDLER(AppWebMessagePortHostMsg_ConvertedAppToWebMessage,
                        OnConvertedAppToWebMessage)
    IPC_MESSAGE_HANDLER(AppWebMessagePortHostMsg_ClosePortAck, OnClosePortAck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AppWebMessagePortMessageFilter::OnConvertedAppToWebMessage(
    int msg_port_id,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids) {
  MessagePortService* msp = MessagePortService::GetInstance();
  msp->PostMessage(msg_port_id, message, sent_message_port_ids);
}

void AppWebMessagePortMessageFilter::OnClosePortAck(int message_port_id) {
  MessagePortService* msp = MessagePortService::GetInstance();
  msp->ClosePort(message_port_id);
  AppWebMessagePortServiceImpl::GetInstance()->CleanupPort(message_port_id);
}

void AppWebMessagePortMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void AppWebMessagePortMessageFilter::SendAppToWebMessage(
    int msg_port_route_id,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids) {
  Send(new AppWebMessagePortMsg_AppToWebMessage(
      route_id_,
      msg_port_route_id,  // same as the port id
      message, sent_message_port_ids));
}

void AppWebMessagePortMessageFilter::SendClosePortMessage(int message_port_id) {
  Send(new AppWebMessagePortMsg_ClosePort(route_id_, message_port_id));
}

void AppWebMessagePortMessageFilter::SendMessage(
    int msg_port_route_id,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids) {
  MessagePortService* msp = MessagePortService::GetInstance();
  for (int sent_port_id : sent_message_port_ids) {
    msp->HoldMessages(sent_port_id);
    msp->UpdateMessagePort(sent_port_id, this, sent_port_id);
  }
  Send(new AppWebMessagePortMsg_WebToAppMessage(
      route_id_,
      msg_port_route_id,  // same as the port id
      message, sent_message_port_ids));
}

void AppWebMessagePortMessageFilter::SendMessagesAreQueued(int route_id) {
  // TODO(sgurun) implement
  NOTREACHED();
}

}  // namespace content
