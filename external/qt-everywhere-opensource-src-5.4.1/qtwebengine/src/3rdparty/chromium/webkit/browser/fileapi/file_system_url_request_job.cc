// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_system_url_request_job.h"

#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util_proxy.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/common/fileapi/file_system_util.h"

using net::NetworkDelegate;
using net::URLRequest;
using net::URLRequestJob;
using net::URLRequestStatus;

namespace fileapi {

static net::HttpResponseHeaders* CreateHttpResponseHeaders() {
  // HttpResponseHeaders expects its input string to be terminated by two NULs.
  static const char kStatus[] = "HTTP/1.1 200 OK\0";
  static const size_t kStatusLen = arraysize(kStatus);

  net::HttpResponseHeaders* headers =
      new net::HttpResponseHeaders(std::string(kStatus, kStatusLen));

  // Tell WebKit never to cache this content.
  std::string cache_control(net::HttpRequestHeaders::kCacheControl);
  cache_control.append(": no-cache");
  headers->AddHeader(cache_control);

  return headers;
}

FileSystemURLRequestJob::FileSystemURLRequestJob(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    const std::string& storage_domain,
    FileSystemContext* file_system_context)
    : URLRequestJob(request, network_delegate),
      storage_domain_(storage_domain),
      file_system_context_(file_system_context),
      is_directory_(false),
      remaining_bytes_(0),
      weak_factory_(this) {
}

FileSystemURLRequestJob::~FileSystemURLRequestJob() {}

void FileSystemURLRequestJob::Start() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FileSystemURLRequestJob::StartAsync,
                 weak_factory_.GetWeakPtr()));
}

void FileSystemURLRequestJob::Kill() {
  reader_.reset();
  URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
}

bool FileSystemURLRequestJob::ReadRawData(net::IOBuffer* dest, int dest_size,
                                          int *bytes_read) {
  DCHECK_NE(dest_size, 0);
  DCHECK(bytes_read);
  DCHECK_GE(remaining_bytes_, 0);

  if (reader_.get() == NULL)
    return false;

  if (remaining_bytes_ < dest_size)
    dest_size = static_cast<int>(remaining_bytes_);

  if (!dest_size) {
    *bytes_read = 0;
    return true;
  }

  const int rv = reader_->Read(dest, dest_size,
                               base::Bind(&FileSystemURLRequestJob::DidRead,
                                          weak_factory_.GetWeakPtr()));
  if (rv >= 0) {
    // Data is immediately available.
    *bytes_read = rv;
    remaining_bytes_ -= rv;
    DCHECK_GE(remaining_bytes_, 0);
    return true;
  }
  if (rv == net::ERR_IO_PENDING)
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  else
    NotifyFailed(rv);
  return false;
}

bool FileSystemURLRequestJob::GetMimeType(std::string* mime_type) const {
  DCHECK(request_);
  DCHECK(url_.is_valid());
  base::FilePath::StringType extension = url_.path().Extension();
  if (!extension.empty())
    extension = extension.substr(1);
  return net::GetWellKnownMimeTypeFromExtension(extension, mime_type);
}

void FileSystemURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      if (ranges.size() == 1) {
        byte_range_ = ranges[0];
      } else {
        // We don't support multiple range requests in one single URL request.
        // TODO(adamk): decide whether we want to support multiple range
        // requests.
        NotifyFailed(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
      }
    }
  }
}

void FileSystemURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (response_info_)
    *info = *response_info_;
}

int FileSystemURLRequestJob::GetResponseCode() const {
  if (response_info_)
    return 200;
  return URLRequestJob::GetResponseCode();
}

void FileSystemURLRequestJob::StartAsync() {
  if (!request_)
    return;
  DCHECK(!reader_.get());
  url_ = file_system_context_->CrackURL(request_->url());
  if (!url_.is_valid()) {
    file_system_context_->AttemptAutoMountForURLRequest(
        request_,
        storage_domain_,
        base::Bind(&FileSystemURLRequestJob::DidAttemptAutoMount,
                   weak_factory_.GetWeakPtr()));
    return;
  }
  if (!file_system_context_->CanServeURLRequest(url_)) {
    // In incognito mode the API is not usable and there should be no data.
    NotifyFailed(net::ERR_FILE_NOT_FOUND);
    return;
  }
  file_system_context_->operation_runner()->GetMetadata(
      url_,
      base::Bind(&FileSystemURLRequestJob::DidGetMetadata,
                 weak_factory_.GetWeakPtr()));
}

void FileSystemURLRequestJob::DidAttemptAutoMount(base::File::Error result) {
  if (result >= 0 &&
      file_system_context_->CrackURL(request_->url()).is_valid()) {
    StartAsync();
  } else {
    NotifyFailed(net::ERR_FILE_NOT_FOUND);
  }
}

void FileSystemURLRequestJob::DidGetMetadata(
    base::File::Error error_code,
    const base::File::Info& file_info) {
  if (error_code != base::File::FILE_OK) {
    NotifyFailed(error_code == base::File::FILE_ERROR_INVALID_URL ?
                 net::ERR_INVALID_URL : net::ERR_FILE_NOT_FOUND);
    return;
  }

  // We may have been orphaned...
  if (!request_)
    return;

  is_directory_ = file_info.is_directory;

  if (!byte_range_.ComputeBounds(file_info.size)) {
    NotifyFailed(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  if (is_directory_) {
    NotifyHeadersComplete();
    return;
  }

  remaining_bytes_ = byte_range_.last_byte_position() -
                     byte_range_.first_byte_position() + 1;
  DCHECK_GE(remaining_bytes_, 0);

  DCHECK(!reader_.get());
  reader_ = file_system_context_->CreateFileStreamReader(
      url_, byte_range_.first_byte_position(), base::Time());

  set_expected_content_size(remaining_bytes_);
  response_info_.reset(new net::HttpResponseInfo());
  response_info_->headers = CreateHttpResponseHeaders();
  NotifyHeadersComplete();
}

void FileSystemURLRequestJob::DidRead(int result) {
  if (result > 0)
    SetStatus(URLRequestStatus());  // Clear the IO_PENDING status
  else if (result == 0)
    NotifyDone(URLRequestStatus());
  else
    NotifyFailed(result);

  remaining_bytes_ -= result;
  DCHECK_GE(remaining_bytes_, 0);

  NotifyReadComplete(result);
}

bool FileSystemURLRequestJob::IsRedirectResponse(GURL* location,
                                                 int* http_status_code) {
  if (is_directory_) {
    // This happens when we discovered the file is a directory, so needs a
    // slash at the end of the path.
    std::string new_path = request_->url().path();
    new_path.push_back('/');
    GURL::Replacements replacements;
    replacements.SetPathStr(new_path);
    *location = request_->url().ReplaceComponents(replacements);
    *http_status_code = 301;  // simulate a permanent redirect
    return true;
  }

  return false;
}

void FileSystemURLRequestJob::NotifyFailed(int rv) {
  NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
}

}  // namespace fileapi
