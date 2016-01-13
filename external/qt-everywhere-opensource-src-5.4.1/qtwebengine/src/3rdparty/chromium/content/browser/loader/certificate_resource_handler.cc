// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/certificate_resource_handler.h"

#include "base/strings/string_util.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

namespace content {

CertificateResourceHandler::CertificateResourceHandler(
    net::URLRequest* request)
    : ResourceHandler(request),
      content_length_(0),
      read_buffer_(NULL),
      resource_buffer_(NULL),
      cert_type_(net::CERTIFICATE_MIME_TYPE_UNKNOWN) {
}

CertificateResourceHandler::~CertificateResourceHandler() {
}

bool CertificateResourceHandler::OnUploadProgress(uint64 position,
                                                  uint64 size) {
  return true;
}

bool CertificateResourceHandler::OnRequestRedirected(const GURL& url,
                                                     ResourceResponse* resp,
                                                     bool* defer) {
  url_ = url;
  return true;
}

bool CertificateResourceHandler::OnResponseStarted(ResourceResponse* resp,
                                                   bool* defer) {
  cert_type_ = net::GetCertificateMimeTypeForMimeType(resp->head.mime_type);
  return cert_type_ != net::CERTIFICATE_MIME_TYPE_UNKNOWN;
}

bool CertificateResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  return true;
}

bool CertificateResourceHandler::OnBeforeNetworkStart(const GURL& url,
                                                      bool* defer) {
  return true;
}

bool CertificateResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                            int* buf_size,
                                            int min_size) {
  static const int kReadBufSize = 32768;

  // TODO(gauravsh): Should we use 'min_size' here?
  DCHECK(buf && buf_size);
  if (!read_buffer_.get()) {
    read_buffer_ = new net::IOBuffer(kReadBufSize);
  }
  *buf = read_buffer_.get();
  *buf_size = kReadBufSize;

  return true;
}

bool CertificateResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  if (!bytes_read)
    return true;

  // We have more data to read.
  DCHECK(read_buffer_.get());
  content_length_ += bytes_read;

  // Release the ownership of the buffer, and store a reference
  // to it. A new one will be allocated in OnWillRead().
  scoped_refptr<net::IOBuffer> buffer;
  read_buffer_.swap(buffer);
  // TODO(gauravsh): Should this be handled by a separate thread?
  buffer_.push_back(std::make_pair(buffer, bytes_read));

  return true;
}

void CertificateResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& urs,
    const std::string& sec_info,
    bool* defer) {
  if (urs.status() != net::URLRequestStatus::SUCCESS)
    return;

  AssembleResource();

  const void* content_bytes = NULL;
  if (resource_buffer_.get())
    content_bytes = resource_buffer_->data();

  // Note that it's up to the browser to verify that the certificate
  // data is well-formed.
  const ResourceRequestInfo* info = GetRequestInfo();
  GetContentClient()->browser()->AddCertificate(
      cert_type_, content_bytes, content_length_,
      info->GetChildID(), info->GetRenderFrameID());
}

void CertificateResourceHandler::AssembleResource() {
  // 0-length IOBuffers are not allowed.
  if (content_length_ == 0) {
    resource_buffer_ = NULL;
    return;
  }

  // Create the new buffer.
  resource_buffer_ = new net::IOBuffer(content_length_);

  // Copy the data into it.
  size_t bytes_copied = 0;
  for (size_t i = 0; i < buffer_.size(); ++i) {
    net::IOBuffer* data = buffer_[i].first.get();
    size_t data_len = buffer_[i].second;
    DCHECK(data != NULL);
    DCHECK_LE(bytes_copied + data_len, content_length_);
    memcpy(resource_buffer_->data() + bytes_copied, data->data(), data_len);
    bytes_copied += data_len;
  }
  DCHECK_EQ(content_length_, bytes_copied);
}

void CertificateResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  NOTREACHED();
}

}  // namespace content
