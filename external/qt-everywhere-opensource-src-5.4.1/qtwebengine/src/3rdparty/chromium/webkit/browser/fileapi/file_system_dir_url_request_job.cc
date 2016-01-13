// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_system_dir_url_request_job.h"

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/fileapi/directory_entry.h"
#include "webkit/common/fileapi/file_system_util.h"

using net::NetworkDelegate;
using net::URLRequest;
using net::URLRequestJob;
using net::URLRequestStatus;

namespace fileapi {

FileSystemDirURLRequestJob::FileSystemDirURLRequestJob(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    const std::string& storage_domain,
    FileSystemContext* file_system_context)
    : URLRequestJob(request, network_delegate),
      storage_domain_(storage_domain),
      file_system_context_(file_system_context),
      weak_factory_(this) {
}

FileSystemDirURLRequestJob::~FileSystemDirURLRequestJob() {
}

bool FileSystemDirURLRequestJob::ReadRawData(net::IOBuffer* dest, int dest_size,
                                             int *bytes_read) {
  int count = std::min(dest_size, static_cast<int>(data_.size()));
  if (count > 0) {
    memcpy(dest->data(), data_.data(), count);
    data_.erase(0, count);
  }
  *bytes_read = count;
  return true;
}

void FileSystemDirURLRequestJob::Start() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FileSystemDirURLRequestJob::StartAsync,
                 weak_factory_.GetWeakPtr()));
}

void FileSystemDirURLRequestJob::Kill() {
  URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
}

bool FileSystemDirURLRequestJob::GetMimeType(std::string* mime_type) const {
  *mime_type = "text/html";
  return true;
}

bool FileSystemDirURLRequestJob::GetCharset(std::string* charset) {
  *charset = "utf-8";
  return true;
}

void FileSystemDirURLRequestJob::StartAsync() {
  if (!request_)
    return;
  url_ = file_system_context_->CrackURL(request_->url());
  if (!url_.is_valid()) {
    file_system_context_->AttemptAutoMountForURLRequest(
        request_,
        storage_domain_,
        base::Bind(&FileSystemDirURLRequestJob::DidAttemptAutoMount,
                   weak_factory_.GetWeakPtr()));
    return;
  }
  if (!file_system_context_->CanServeURLRequest(url_)) {
    // In incognito mode the API is not usable and there should be no data.
    if (url_.is_valid() && VirtualPath::IsRootPath(url_.virtual_path())) {
      // Return an empty directory if the filesystem root is queried.
      DidReadDirectory(base::File::FILE_OK,
                       std::vector<DirectoryEntry>(),
                       false);
      return;
    }
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                                net::ERR_FILE_NOT_FOUND));
    return;
  }
  file_system_context_->operation_runner()->ReadDirectory(
      url_,
      base::Bind(&FileSystemDirURLRequestJob::DidReadDirectory, this));
}

void FileSystemDirURLRequestJob::DidAttemptAutoMount(base::File::Error result) {
  if (result >= 0 &&
      file_system_context_->CrackURL(request_->url()).is_valid()) {
    StartAsync();
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                                net::ERR_FILE_NOT_FOUND));
  }
}

void FileSystemDirURLRequestJob::DidReadDirectory(
    base::File::Error result,
    const std::vector<DirectoryEntry>& entries,
    bool has_more) {
  if (result != base::File::FILE_OK) {
    int rv = net::ERR_FILE_NOT_FOUND;
    if (result == base::File::FILE_ERROR_INVALID_URL)
      rv = net::ERR_INVALID_URL;
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
    return;
  }

  if (!request_)
    return;

  if (data_.empty()) {
    base::FilePath relative_path = url_.path();
#if defined(OS_POSIX)
    relative_path =
        base::FilePath(FILE_PATH_LITERAL("/") + relative_path.value());
#endif
    const base::string16& title = relative_path.LossyDisplayName();
    data_.append(net::GetDirectoryListingHeader(title));
  }

  typedef std::vector<DirectoryEntry>::const_iterator EntryIterator;
  for (EntryIterator it = entries.begin(); it != entries.end(); ++it) {
    const base::string16& name = base::FilePath(it->name).LossyDisplayName();
    data_.append(net::GetDirectoryListingEntry(
        name, std::string(), it->is_directory, it->size,
        it->last_modified_time));
  }

  if (!has_more) {
    set_expected_content_size(data_.size());
    NotifyHeadersComplete();
  }
}

}  // namespace fileapi
