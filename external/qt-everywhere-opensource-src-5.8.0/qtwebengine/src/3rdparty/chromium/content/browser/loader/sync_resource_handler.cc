// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/sync_resource_handler.h"

#include "base/logging.h"
#include "content/browser/loader/netlog_observer.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/common/resource_messages.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"

namespace content {

SyncResourceHandler::SyncResourceHandler(
    net::URLRequest* request,
    IPC::Message* result_message,
    ResourceDispatcherHostImpl* resource_dispatcher_host)
    : ResourceHandler(request),
      read_buffer_(new net::IOBuffer(kReadBufSize)),
      result_message_(result_message),
      rdh_(resource_dispatcher_host),
      total_transfer_size_(0) {
  result_.final_url = request->url();
}

SyncResourceHandler::~SyncResourceHandler() {
  if (result_message_) {
    result_message_->set_reply_error();
    ResourceMessageFilter* filter = GetFilter();
    // If the filter doesn't exist at this point, the process has died and isn't
    // waiting for the result message anymore.
    if (filter)
      filter->Send(result_message_);
  }
}

bool SyncResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    bool* defer) {
  if (rdh_->delegate()) {
    rdh_->delegate()->OnRequestRedirected(
        redirect_info.new_url, request(), GetRequestInfo()->GetContext(),
        response);
  }

  NetLogObserver::PopulateResponseInfo(request(), response);
  // TODO(darin): It would be much better if this could live in WebCore, but
  // doing so requires API changes at all levels.  Similar code exists in
  // WebCore/platform/network/cf/ResourceHandleCFNet.cpp :-(
  if (redirect_info.new_url.GetOrigin() != result_.final_url.GetOrigin()) {
    LOG(ERROR) << "Cross origin redirect denied";
    return false;
  }
  result_.final_url = redirect_info.new_url;

  total_transfer_size_ += request()->GetTotalReceivedBytes();
  return true;
}

bool SyncResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    bool* defer) {
  const ResourceRequestInfoImpl* info = GetRequestInfo();
  if (!info->filter())
    return false;

  if (rdh_->delegate()) {
    rdh_->delegate()->OnResponseStarted(
        request(), info->GetContext(), response, info->filter());
  }

  NetLogObserver::PopulateResponseInfo(request(), response);

  // We don't care about copying the status here.
  result_.headers = response->head.headers;
  result_.mime_type = response->head.mime_type;
  result_.charset = response->head.charset;
  result_.download_file_path = response->head.download_file_path;
  result_.request_time = response->head.request_time;
  result_.response_time = response->head.response_time;
  result_.load_timing = response->head.load_timing;
  result_.devtools_info = response->head.devtools_info;
  return true;
}

bool SyncResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  return true;
}

bool SyncResourceHandler::OnBeforeNetworkStart(const GURL& url, bool* defer) {
  return true;
}

bool SyncResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                     int* buf_size,
                                     int min_size) {
  DCHECK(min_size == -1);
  *buf = read_buffer_.get();
  *buf_size = kReadBufSize;
  return true;
}

bool SyncResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  if (!bytes_read)
    return true;
  result_.data.append(read_buffer_->data(), bytes_read);
  return true;
}

void SyncResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    const std::string& security_info,
    bool* defer) {
  ResourceMessageFilter* filter = GetFilter();
  if (!filter)
    return;

  result_.error_code = status.error();

  int total_transfer_size = request()->GetTotalReceivedBytes();
  result_.encoded_data_length = total_transfer_size_ + total_transfer_size;

  ResourceHostMsg_SyncLoad::WriteReplyParams(result_message_, result_);
  filter->Send(result_message_);
  result_message_ = NULL;
  return;
}

void SyncResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  // Sync requests don't involve ResourceMsg_DataDownloaded messages
  // being sent back to renderers as progress is made.
}

}  // namespace content
