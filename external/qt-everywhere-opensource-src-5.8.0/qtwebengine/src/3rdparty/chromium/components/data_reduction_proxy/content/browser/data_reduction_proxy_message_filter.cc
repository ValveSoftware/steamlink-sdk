// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_message_filter.h"

#include "components/data_reduction_proxy/content/common/data_reduction_proxy_messages.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/host_port_pair.h"

namespace data_reduction_proxy {

DataReductionProxyMessageFilter::DataReductionProxyMessageFilter(
    DataReductionProxySettings* settings)
    : BrowserMessageFilter(DataReductionProxyStart),
      config_(nullptr) {
  if (settings)
    config_ = settings->Config();
}

DataReductionProxyMessageFilter::~DataReductionProxyMessageFilter() {
}

bool DataReductionProxyMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DataReductionProxyMessageFilter, message)
    IPC_MESSAGE_HANDLER(DataReductionProxyViewHostMsg_IsDataReductionProxy,
                        OnIsDataReductionProxy)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DataReductionProxyMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, content::BrowserThread::ID* thread) {
  if (message.type() ==
      DataReductionProxyViewHostMsg_IsDataReductionProxy::ID) {
      *thread = content::BrowserThread::IO;
  }
}

void DataReductionProxyMessageFilter::OnIsDataReductionProxy(
    const net::HostPortPair& proxy_server,
    bool* is_data_reduction_proxy) {
  *is_data_reduction_proxy = false;
  if (!config_)
    return;
  *is_data_reduction_proxy =
      config_->IsDataReductionProxy(proxy_server, nullptr);
}

}  // namespace data_reduction_proxy
