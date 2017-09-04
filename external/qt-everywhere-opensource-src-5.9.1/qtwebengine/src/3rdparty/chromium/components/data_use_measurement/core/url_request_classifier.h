// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USE_MEASUREMENT_CORE_URL_REQUEST_CLASSIFIER_H_
#define COMPONENTS_DATA_USE_MEASUREMENT_CORE_URL_REQUEST_CLASSIFIER_H_

namespace net {
class URLRequest;
}

namespace data_use_measurement {

// Interface for a classifier that can classify URL requests.
class URLRequestClassifier {
 public:
  virtual ~URLRequestClassifier() {}

  // Returns true if the URLRequest |request| is initiated by user traffic.
  virtual bool IsUserRequest(const net::URLRequest& request) const = 0;
};

}  // namespace data_use_measurement

#endif  // COMPONENTS_DATA_USE_MEASUREMENT_CORE_URL_REQUEST_CLASSIFIER_H_
