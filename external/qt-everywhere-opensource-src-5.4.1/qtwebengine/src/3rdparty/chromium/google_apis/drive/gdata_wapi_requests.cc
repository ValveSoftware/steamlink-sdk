// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/gdata_wapi_requests.h"

#include "google_apis/drive/gdata_wapi_url_generator.h"

namespace google_apis {

//============================ GetResourceEntryRequest =======================

GetResourceEntryRequest::GetResourceEntryRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const std::string& resource_id,
    const GURL& embed_origin,
    const GetDataCallback& callback)
    : GetDataRequest(sender, callback),
      url_generator_(url_generator),
      resource_id_(resource_id),
      embed_origin_(embed_origin) {
  DCHECK(!callback.is_null());
}

GetResourceEntryRequest::~GetResourceEntryRequest() {}

GURL GetResourceEntryRequest::GetURL() const {
  return url_generator_.GenerateEditUrlWithEmbedOrigin(
      resource_id_, embed_origin_);
}

}  // namespace google_apis
