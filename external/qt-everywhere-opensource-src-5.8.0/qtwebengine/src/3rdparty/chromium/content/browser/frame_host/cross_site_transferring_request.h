// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_CROSS_SITE_TRANSFERRING_REQUEST_H_
#define CONTENT_BROWSER_FRAME_HOST_CROSS_SITE_TRANSFERRING_REQUEST_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"

namespace content {

class CrossSiteResourceHandler;

// A UI thread object that owns a request being transferred.  Deleting the
// object without releasing the request will delete the underlying URLRequest.
// This is needed to clean up the URLRequest when a cross site navigation is
// cancelled.
class CONTENT_EXPORT CrossSiteTransferringRequest {
 public:
  explicit CrossSiteTransferringRequest(GlobalRequestID global_request_id);
  ~CrossSiteTransferringRequest();

  // Relinquishes ownership of the request, so another process can take
  // control of it.
  void ReleaseRequest();

  GlobalRequestID request_id() const { return global_request_id_; }

 private:
  // No need for a weak pointer here - nothing should have ownership of the
  // cross site request until after |this| is deleted, or ReleaseRequest is
  // called.
  GlobalRequestID global_request_id_;

  DISALLOW_COPY_AND_ASSIGN(CrossSiteTransferringRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_CROSS_SITE_TRANSFERRING_REQUEST_H_
