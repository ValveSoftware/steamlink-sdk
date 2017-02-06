// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PINGBACK_CLIENT_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PINGBACK_CLIENT_H_

#include <list>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace data_reduction_proxy {
class DataReductionProxyData;
struct DataReductionProxyPageLoadTiming;

// Manages pingbacks about page load timing information to the data saver proxy
// server. This class is not thread safe.
class DataReductionProxyPingbackClient : public net::URLFetcherDelegate {
 public:
  // The caller must ensure that |url_request_context| remains alive for the
  // lifetime of the |DataReductionProxyPingbackClient| instance.
  explicit DataReductionProxyPingbackClient(
      net::URLRequestContextGetter* url_request_context);
  ~DataReductionProxyPingbackClient() override;

  // Sends a pingback to the data saver proxy server about various timing
  // information.
  virtual void SendPingback(const DataReductionProxyData& data,
                            const DataReductionProxyPageLoadTiming& timing);

  // Sets the probability of actually sending a pingback to the server for any
  // call to SendPingback.
  void SetPingbackReportingFraction(float pingback_reporting_fraction);

 protected:
  // Generates a float in the range [0, 1). Virtualized in testing.
  virtual float GenerateRandomFloat() const;

 private:
  // URLFetcherDelegate implmentation:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Whether a pingback should be sent.
  bool ShouldSendPingback() const;

  // Creates an URLFetcher that will POST to |secure_proxy_url_| using
  // |url_request_context_|. The max retires is set to 5.
  std::unique_ptr<net::URLFetcher> MaybeCreateFetcherForDataAndStart(
      const std::string& data);

  net::URLRequestContextGetter* url_request_context_;

  // The URL for the data saver proxy's ping back service.
  const GURL pingback_url_;

  // The currently running fetcher.
  std::unique_ptr<net::URLFetcher> current_fetcher_;

  // Serialized data to send to the data saver proxy server.
  std::list<std::string> data_to_send_;

  // The probability of sending a pingback to the server.
  float pingback_reporting_fraction_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyPingbackClient);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PINGBACK_CLIENT_H_
