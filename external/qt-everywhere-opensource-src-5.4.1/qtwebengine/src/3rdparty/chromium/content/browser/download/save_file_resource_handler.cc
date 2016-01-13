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
#include "net/url_request/url_request_status.h"

namespace content {

SaveFileResourceHandler::SaveFileResourceHandler(net::URLRequest* request,
                                                 int render_process_host_id,
                                                 int render_view_id,
                                                 const GURL& url,
                                                 SaveFileManager* manager)
    : ResourceHandler(request),
      save_id_(-1),
      render_process_id_(render_process_host_id),
      render_view_id_(render_view_id),
      url_(url),
      content_length_(0),
      save_manager_(manager) {
}

SaveFileResourceHandler::~SaveFileResourceHandler() {
}

bool SaveFileResourceHandler::OnUploadProgress(uint64 position, uint64 size) {
  return true;
}

bool SaveFileResourceHandler::OnRequestRedirected(
    const GURL& url,
    ResourceResponse* response,
    bool* defer) {
  final_url_ = url;
  return true;
}

bool SaveFileResourceHandler::OnResponseStarted(ResourceResponse* response,
                                                bool* defer) {
  save_id_ = save_manager_->GetNextId();
  // |save_manager_| consumes (deletes):
  SaveFileCreateInfo* info = new SaveFileCreateInfo;
  info->url = url_;
  info->final_url = final_url_;
  info->total_bytes = content_length_;
  info->save_id = save_id_;
  info->render_process_id = render_process_id_;
  info->render_view_id = render_view_id_;
  info->request_id = GetRequestID();
  info->content_disposition = content_disposition_;
  info->save_source = SaveFileCreateInfo::SAVE_FILE_FROM_NET;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::StartSave, save_manager_, info));
  return true;
}

bool SaveFileResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  return true;
}

bool SaveFileResourceHandler::OnBeforeNetworkStart(const GURL& url,
                                                   bool* defer) {
  return true;
}

bool SaveFileResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                         int* buf_size,
                                         int min_size) {
  DCHECK(buf && buf_size);
  if (!read_buffer_.get()) {
    *buf_size = min_size < 0 ? kReadBufSize : min_size;
    read_buffer_ = new net::IOBuffer(*buf_size);
  }
  *buf = read_buffer_.get();
  return true;
}

bool SaveFileResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  DCHECK(read_buffer_.get());
  // We are passing ownership of this buffer to the save file manager.
  scoped_refptr<net::IOBuffer> buffer;
  read_buffer_.swap(buffer);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::UpdateSaveProgress,
          save_manager_, save_id_, buffer, bytes_read));
  return true;
}

void SaveFileResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    const std::string& security_info,
    bool* defer) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::SaveFinished, save_manager_, save_id_, url_,
          render_process_id_, status.is_success() && !status.is_io_pending()));
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
