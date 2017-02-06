// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_NETWORK_DELEGATE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_NETWORK_DELEGATE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "net/base/completion_callback.h"
#include "net/base/layered_network_delegate.h"
#include "net/proxy/proxy_retry_info.h"

class GURL;

namespace base {
class Value;
}

namespace net {
class HttpResponseHeaders;
class HttpRequestHeaders;
class NetworkDelegate;
class ProxyConfig;
class ProxyInfo;
class ProxyServer;
class ProxyService;
class URLRequest;
}

namespace data_reduction_proxy {
class DataReductionProxyBypassStats;
class DataReductionProxyConfig;
class DataReductionProxyConfigurator;
class DataReductionProxyExperimentsStats;
class DataReductionProxyIOData;
class DataReductionProxyRequestOptions;
class DataUseGroupProvider;
class DataUseGroup;

// Values of the UMA DataReductionProxy.LoFi.TransformationType histogram.
// This enum must remain synchronized with
// DataReductionProxyLoFiTransformationType in
// metrics/histograms/histograms.xml.
enum LoFiTransformationType {
  PREVIEW = 0,
  NO_TRANSFORMATION_PREVIEW_REQUESTED,
  LO_FI_TRANSFORMATION_TYPES_INDEX_BOUNDARY,
};

// DataReductionProxyNetworkDelegate is a LayeredNetworkDelegate that wraps a
// NetworkDelegate and adds Data Reduction Proxy specific logic.
class DataReductionProxyNetworkDelegate : public net::LayeredNetworkDelegate {
 public:
  // Provides an additional proxy configuration that can be consulted after
  // proxy resolution. Used to get the Data Reduction Proxy config and give it
  // to the OnResolveProxyHandler and RecordBytesHistograms.
  typedef base::Callback<const net::ProxyConfig&()> ProxyConfigGetter;

  // Constructs a DataReductionProxyNetworkDelegate object with the given
  // |network_delegate|, |config|, |handler|, |configurator|, and
  // |experiments_stats|. Takes ownership of
  // and wraps the |network_delegate|, calling an internal implementation for
  // each delegate method. For example, the implementation of
  // OnHeadersReceived() calls OnHeadersReceivedInternal().
  DataReductionProxyNetworkDelegate(
      std::unique_ptr<net::NetworkDelegate> network_delegate,
      DataReductionProxyConfig* config,
      DataReductionProxyRequestOptions* handler,
      const DataReductionProxyConfigurator* configurator);
  ~DataReductionProxyNetworkDelegate() override;

  // Initializes member variables to record data reduction proxy prefs and
  // report UMA.
  void InitIODataAndUMA(
      DataReductionProxyIOData* io_data,
      DataReductionProxyBypassStats* bypass_stats);

  // Creates a |Value| summary of the state of the network session. The caller
  // is responsible for deleting the returned value.
  base::Value* SessionNetworkStatsInfoToValue() const;

  void SetDataUseGroupProvider(
      std::unique_ptr<DataUseGroupProvider> data_use_group_provider);

 private:
  // Resets if Lo-Fi has been used for the last main frame load to false.
  void OnBeforeURLRequestInternal(net::URLRequest* request,
                                  const net::CompletionCallback& callback,
                                  GURL* new_url) override;

  // Called after connection. Allows the delegate to read/write
  // |headers| before they get sent out. |headers| is valid only until
  // OnCompleted or OnURLRequestDestroyed is called for this request.
  void OnBeforeSendHeadersInternal(
      net::URLRequest* request,
      const net::ProxyInfo& proxy_info,
      const net::ProxyRetryInfoMap& proxy_retry_info,
      net::HttpRequestHeaders* headers) override;

  // Indicates that the URL request has been completed or failed.
  // |started| indicates whether the request has been started. If false,
  // some information like the socket address is not available.
  void OnCompletedInternal(net::URLRequest* request,
                           bool started) override;

  // Calculates actual data usage that went over the network at the HTTP layer
  // (e.g. not including network layer overhead) and estimates original data
  // usage for |request|. Passing in -1 for |original_content_length| indicates
  // that the original content length of the response could not be determined.
  void CalculateAndRecordDataUsage(const net::URLRequest& request,
                                   DataReductionProxyRequestType request_type,
                                   int64_t original_content_length);

  // Posts to the UI thread to UpdateContentLengthPrefs in the data reduction
  // proxy metrics and updates |received_content_length_| and
  // |original_content_length_|.
  void AccumulateDataUsage(int64_t data_used,
                           int64_t original_size,
                           DataReductionProxyRequestType request_type,
                           const scoped_refptr<DataUseGroup>& data_use_group,
                           const std::string& mime_type);

  // Record information such as histograms related to the Content-Length of
  // |request|. |original_content_length| is the length of the resource if
  // fetched over a direct connection without the Data Reduction Proxy, or -1 if
  // no original content length is available.
  void RecordContentLength(const net::URLRequest& request,
                           DataReductionProxyRequestType request_type,
                           int64_t original_content_length);

  // Records UMA that counts how many pages were transformed by various Lo-Fi
  // transformations.
  void RecordLoFiTransformationType(LoFiTransformationType type);

  // Returns whether |request| would have used the data reduction proxy server
  // if the holdback fieldtrial weren't enabled. |proxy_info| is the list of
  // proxies being used, and |proxy_retry_info| contains a list of bad proxies.
  bool WasEligibleWithoutHoldback(
      const net::URLRequest& request,
      const net::ProxyInfo& proxy_info,
      const net::ProxyRetryInfoMap& proxy_retry_info) const;

  // Total size of all content that has been received over the network.
  int64_t total_received_bytes_;

  // Total original size of all content before it was transferred.
  int64_t total_original_received_bytes_;

  // All raw Data Reduction Proxy pointers must outlive |this|.
  DataReductionProxyConfig* data_reduction_proxy_config_;

  DataReductionProxyBypassStats* data_reduction_proxy_bypass_stats_;

  DataReductionProxyRequestOptions* data_reduction_proxy_request_options_;

  DataReductionProxyIOData* data_reduction_proxy_io_data_;

  const DataReductionProxyConfigurator* configurator_;

  std::unique_ptr<DataUseGroupProvider> data_use_group_provider_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyNetworkDelegate);
};
}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_NETWORK_DELEGATE_H_
