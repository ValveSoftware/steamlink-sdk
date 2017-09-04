// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SYNC_LOAD_RESPONSE_H_
#define CONTENT_CHILD_SYNC_LOAD_RESPONSE_H_

#include <string>

#include "content/public/common/resource_response_info.h"
#include "url/gurl.h"

namespace content {

// See the SyncLoad method. (The name of this struct is not
// suffixed with "Info" because it also contains the response data.)
struct CONTENT_EXPORT SyncLoadResponse : ResourceResponseInfo {
  SyncLoadResponse();
  ~SyncLoadResponse();

  // The response error code.
  int error_code;

  // The final URL of the response.  This may differ from the request URL in
  // the case of a server redirect.
  GURL url;

  // The response data.
  std::string data;
};

}  // namespace content

#endif  // CONTENT_CHILD_SYNC_LOAD_RESPONSE_H_
