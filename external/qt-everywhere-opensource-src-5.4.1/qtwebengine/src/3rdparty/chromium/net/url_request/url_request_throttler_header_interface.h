// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_THROTTLER_HEADER_INTERFACE_H_
#define NET_URL_REQUEST_URL_REQUEST_THROTTLER_HEADER_INTERFACE_H_

#include <string>

namespace net {

// Interface to an HTTP header to enforce we have the methods we need.
class URLRequestThrottlerHeaderInterface {
 public:
  virtual ~URLRequestThrottlerHeaderInterface() {}

  // Method that enables us to fetch the header value by its key.
  // ex: location: www.example.com -> key = "location" value = "www.example.com"
  // If the key does not exist, it returns an empty string.
  virtual std::string GetNormalizedValue(const std::string& key) const = 0;

  // Returns the HTTP response code associated with the request.
  virtual int GetResponseCode() const = 0;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_THROTTLER_HEADER_INTERFACE_H_
