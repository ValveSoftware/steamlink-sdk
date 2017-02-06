// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/cross_site_transferring_request.h"

#include "base/logging.h"
#include "content/browser/loader/cross_site_resource_handler.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

void CancelRequestOnIOThread(GlobalRequestID global_request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ResourceDispatcherHostImpl::Get()->CancelTransferringNavigation(
      global_request_id);
}

}  // namespace

CrossSiteTransferringRequest::CrossSiteTransferringRequest(
    GlobalRequestID global_request_id)
    : global_request_id_(global_request_id) {
  DCHECK(global_request_id_ != GlobalRequestID());
}

CrossSiteTransferringRequest::~CrossSiteTransferringRequest() {
  if (global_request_id_ == GlobalRequestID())
    return;
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CancelRequestOnIOThread, global_request_id_));
}

void CrossSiteTransferringRequest::ReleaseRequest() {
  DCHECK_NE(-1, global_request_id_.child_id);
  DCHECK_NE(-1, global_request_id_.request_id);
  global_request_id_ = GlobalRequestID();
}

}  // namespace content
