// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/content/content_url_request_classifier.h"

#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"

namespace data_use_measurement {

bool IsUserRequest(const net::URLRequest& request) {
  // The presence of ResourecRequestInfo in |request| implies that this request
  // was created for a content::WebContents. For now we could add a condition to
  // check ProcessType in info is content::PROCESS_TYPE_RENDERER, but it won't
  // be compatible with upcoming PlzNavigate architecture. So just existence of
  // ResourceRequestInfo is verified, and the current check should be compatible
  // with upcoming changes in PlzNavigate.
  // TODO(rajendrant): Verify this condition for different use cases. See
  // crbug.com/626063.
  return content::ResourceRequestInfo::ForRequest(&request) != nullptr;
}

bool ContentURLRequestClassifier::IsUserRequest(
    const net::URLRequest& request) const {
  return data_use_measurement::IsUserRequest(request);
}

}  // namespace data_use_measurement
