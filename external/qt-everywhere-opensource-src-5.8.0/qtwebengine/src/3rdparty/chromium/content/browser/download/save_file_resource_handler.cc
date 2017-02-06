// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_file_resource_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/download/save_file_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_status.h"

namespace content {

SaveFileResourceHandler::SaveFileResourceHandler(
    net::URLRequest* request,
    SaveItemId save_item_id,
    SavePackageId save_package_id,
    int render_process_host_id,
    int render_frame_routing_id,
    const GURL& url,
    SaveFileManager* manager,
    AuthorizationState authorization_state)
    : ResourceHandler(request),
      save_item_id_(save_item_id),
      save_package_id_(save_package_id),
      render_process_id_(render_process_host_id),
      render_frame_routing_id_(render_frame_routing_id),
      url_(url),
      content_length_(0),
      save_manager_(manager),
      authorization_state_(authorization_state) {}

SaveFileResourceHandler::~SaveFileResourceHandler() {
}

bool SaveFileResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    bool* defer) {
  final_url_ = redirect_info.new_url;
  return true;
}

bool SaveFileResourceHandler::OnResponseStarted(ResourceResponse* response,
                                                bool* defer) {
  // |save_manager_| consumes (deletes):
  SaveFileCreateInfo* info = new SaveFileCreateInfo(
      url_, final_url_, save_item_id_, save_package_id_, render_process_id_,
      render_frame_routing_id_, GetRequestID(), content_disposition_,
      content_length_);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::StartSave, save_manager_, info));
  return true;
}

bool SaveFileResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  return authorization_state_ == AuthorizationState::AUTHORIZED;
}

bool SaveFileResourceHandler::OnBeforeNetworkStart(const GURL& url,
                                                   bool* defer) {
  return true;
}

bool SaveFileResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                         int* buf_size,
                                         int min_size) {
  DCHECK_EQ(AuthorizationState::AUTHORIZED, authorization_state_);
  DCHECK(buf && buf_size);
  if (!read_buffer_.get()) {
    *buf_size = min_size < 0 ? kReadBufSize : min_size;
    read_buffer_ = new net::IOBuffer(*buf_size);
  }
  *buf = read_buffer_.get();
  return true;
}

bool SaveFileResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  DCHECK_EQ(AuthorizationState::AUTHORIZED, authorization_state_);
  DCHECK(read_buffer_.get());
  // We are passing ownership of this buffer to the save file manager.
  scoped_refptr<net::IOBuffer> buffer;
  read_buffer_.swap(buffer);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::UpdateSaveProgress, save_manager_,
                 save_item_id_, base::RetainedRef(buffer), bytes_read));
  return true;
}

void SaveFileResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    const std::string& security_info,
    bool* defer) {
  if (authorization_state_ != AuthorizationState::AUTHORIZED)
    DCHECK(!status.is_success());

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::SaveFinished, save_manager_, save_item_id_,
                 save_package_id_,
                 status.is_success() && !status.is_io_pending()));
  read_buffer_ = NULL;
}

void SaveFileResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  NOTREACHED();
}

void SaveFileResourceHandler::set_content_length(
    const std::string& content_length) {
  base::StringToInt64(content_length, &content_length_);
}

}  // namespace content
