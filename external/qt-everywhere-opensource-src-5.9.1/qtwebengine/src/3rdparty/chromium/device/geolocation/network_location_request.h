// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_NETWORK_LOCATION_REQUEST_H_
#define DEVICE_GEOLOCATION_NETWORK_LOCATION_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "device/geolocation/geolocation_export.h"
#include "device/geolocation/wifi_data_provider.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace device {
struct Geoposition;

// Takes wifi data and sends it to a server to get a position fix.
// It performs formatting of the request and interpretation of the response.
class NetworkLocationRequest : private net::URLFetcherDelegate {
 public:
  // ID passed to URLFetcher::Create(). Used for testing.
  DEVICE_GEOLOCATION_EXPORT static int url_fetcher_id_for_tests;

  // Called when a new geo position is available. The second argument indicates
  // whether there was a server error or not. It is true when there was a
  // server or network error - either no response or a 500 error code.
  typedef base::Callback<void(const Geoposition& /* position */,
                              bool /* server_error */,
                              const base::string16& /* access_token */,
                              const WifiData& /* wifi_data */)>
      LocationResponseCallback;

  // |url| is the server address to which the request wil be sent.
  NetworkLocationRequest(
      const scoped_refptr<net::URLRequestContextGetter>& context,
      const GURL& url,
      LocationResponseCallback callback);
  ~NetworkLocationRequest() override;

  // Makes a new request. Returns true if the new request was successfully
  // started. In all cases, any currently pending request will be canceled.
  bool MakeRequest(const base::string16& access_token,
                   const WifiData& wifi_data,
                   const base::Time& wifi_timestamp);

  bool is_request_pending() const { return url_fetcher_ != NULL; }
  const GURL& url() const { return url_; }

 private:
  // net::URLFetcherDelegate
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  const scoped_refptr<net::URLRequestContextGetter> url_context_;
  const LocationResponseCallback location_response_callback_;
  const GURL url_;
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // Keep a copy of the data sent in the request, so we can refer back to it
  // when the response arrives.
  WifiData wifi_data_;
  base::Time wifi_timestamp_;

  // The start time for the request.
  base::TimeTicks request_start_time_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLocationRequest);
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_NETWORK_LOCATION_REQUEST_H_
