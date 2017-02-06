// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_DATA_REDUCTION_PROXY_MESSAGE_FILTER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_DATA_REDUCTION_PROXY_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"

namespace net {
class HostPortPair;
}

namespace data_reduction_proxy {

class DataReductionProxyConfig;
class DataReductionProxySettings;

// An IPC listener to handle DataReductionProxy IPC messages from the renderer.
class DataReductionProxyMessageFilter
    : public content::BrowserMessageFilter {
 public:
  // |settings| may be null.
  DataReductionProxyMessageFilter(DataReductionProxySettings* settings);

  // Sets |is_data_reduction_proxy| to true if the |proxy_server| corresponds to
  // a Data Reduction Proxy. |is_data_reduction_proxy| must not be null.
  void OnIsDataReductionProxy(const net::HostPortPair& proxy_server,
                              bool* is_data_reduction_proxy);

 private:
  ~DataReductionProxyMessageFilter() override;

  // BrowserMessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;

  // Must outlive |this|. May be null.
  DataReductionProxyConfig* config_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMessageFilter);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_DATA_REDUCTION_PROXY_MESSAGE_FILTER_H_
