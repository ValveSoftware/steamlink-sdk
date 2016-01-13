// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/blob/blob_url_request_job_factory.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_util.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/blob/blob_storage_context.h"
#include "webkit/browser/blob/blob_url_request_job.h"
#include "webkit/browser/fileapi/file_system_context.h"

namespace webkit_blob {

namespace {

int kUserDataKey;  // The value is not important, the addr is a key.

BlobDataHandle* GetRequestedBlobDataHandle(net::URLRequest* request) {
  return static_cast<BlobDataHandle*>(request->GetUserData(&kUserDataKey));
}

}  // namespace

// static
scoped_ptr<net::URLRequest> BlobProtocolHandler::CreateBlobRequest(
    scoped_ptr<BlobDataHandle> blob_data_handle,
    const net::URLRequestContext* request_context,
    net::URLRequest::Delegate* request_delegate) {
  const GURL kBlobUrl("blob://see_user_data/");
  scoped_ptr<net::URLRequest> request = request_context->CreateRequest(
      kBlobUrl, net::DEFAULT_PRIORITY, request_delegate, NULL);
  SetRequestedBlobDataHandle(request.get(), blob_data_handle.Pass());
  return request.Pass();
}

// static
void BlobProtocolHandler::SetRequestedBlobDataHandle(
    net::URLRequest* request,
    scoped_ptr<BlobDataHandle> blob_data_handle) {
  request->SetUserData(&kUserDataKey, blob_data_handle.release());
}

BlobProtocolHandler::BlobProtocolHandler(
    BlobStorageContext* context,
    fileapi::FileSystemContext* file_system_context,
    base::MessageLoopProxy* loop_proxy)
    : file_system_context_(file_system_context),
      file_loop_proxy_(loop_proxy) {
  if (context)
    context_ = context->AsWeakPtr();
}

BlobProtocolHandler::~BlobProtocolHandler() {
}

net::URLRequestJob* BlobProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  return new webkit_blob::BlobURLRequestJob(
      request, network_delegate, LookupBlobData(request),
      file_system_context_, file_loop_proxy_);
}

scoped_refptr<webkit_blob::BlobData>
BlobProtocolHandler::LookupBlobData(net::URLRequest* request) const {
  BlobDataHandle* blob_data_handle = GetRequestedBlobDataHandle(request);
  if (blob_data_handle)
    return blob_data_handle->data();
  if (!context_.get())
    return NULL;

  // Support looking up based on uuid, the FeedbackExtensionAPI relies on this.
  // TODO(michaeln): Replace this use case and others like it with a BlobReader
  // impl that does not depend on urlfetching to perform this function.
  const std::string kPrefix("blob:uuid/");
  if (!StartsWithASCII(request->url().spec(), kPrefix, true))
    return NULL;
  std::string uuid = request->url().spec().substr(kPrefix.length());
  scoped_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(uuid);
  return handle.get() ? handle->data() : NULL;
}

}  // namespace webkit_blob
