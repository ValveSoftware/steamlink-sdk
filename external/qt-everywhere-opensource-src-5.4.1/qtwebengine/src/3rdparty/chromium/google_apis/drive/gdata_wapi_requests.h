// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_GDATA_WAPI_REQUESTS_H_
#define GOOGLE_APIS_DRIVE_GDATA_WAPI_REQUESTS_H_

#include <string>

#include "google_apis/drive/base_requests.h"
#include "google_apis/drive/gdata_wapi_url_generator.h"

namespace google_apis {

//========================= GetResourceEntryRequest ==========================

// This class performs the request for fetching a single resource entry.
class GetResourceEntryRequest : public GetDataRequest {
 public:
  // |callback| must not be null.
  GetResourceEntryRequest(RequestSender* sender,
                          const GDataWapiUrlGenerator& url_generator,
                          const std::string& resource_id,
                          const GURL& embed_origin,
                          const GetDataCallback& callback);
  virtual ~GetResourceEntryRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  // Resource id of the requested entry.
  const std::string resource_id_;
  // Embed origin for an url to the sharing dialog. Can be empty.
  const GURL& embed_origin_;

  DISALLOW_COPY_AND_ASSIGN(GetResourceEntryRequest);
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_GDATA_WAPI_REQUESTS_H_
