// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_file_dir_job.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

#if defined(OS_POSIX)
#include <sys/stat.h>
#endif

namespace net {

URLRequestFileDirJob::URLRequestFileDirJob(URLRequest* request,
                                           NetworkDelegate* network_delegate,
                                           const base::FilePath& dir_path)
    : URLRequestJob(request, network_delegate),
      lister_(dir_path, this),
      dir_path_(dir_path),
      canceled_(false),
      list_complete_(false),
      wrote_header_(false),
      read_pending_(false),
      read_buffer_length_(0),
      weak_factory_(this) {
}

void URLRequestFileDirJob::StartAsync() {
  lister_.Start();

  NotifyHeadersComplete();
}

void URLRequestFileDirJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestFileDirJob::StartAsync,
                 weak_factory_.GetWeakPtr()));
}

void URLRequestFileDirJob::Kill() {
  if (canceled_)
    return;

  canceled_ = true;

  if (!list_complete_)
    lister_.Cancel();

  URLRequestJob::Kill();

  weak_factory_.InvalidateWeakPtrs();
}

bool URLRequestFileDirJob::ReadRawData(IOBuffer* buf, int buf_size,
                                       int* bytes_read) {
  DCHECK(bytes_read);
  *bytes_read = 0;

  if (is_done())
    return true;

  if (FillReadBuffer(buf->data(), buf_size, bytes_read))
    return true;

  // We are waiting for more data
  read_pending_ = true;
  read_buffer_ = buf;
  read_buffer_length_ = buf_size;
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  return false;
}

bool URLRequestFileDirJob::GetMimeType(std::string* mime_type) const {
  *mime_type = "text/html";
  return true;
}

bool URLRequestFileDirJob::GetCharset(std::string* charset) {
  // All the filenames are converted to UTF-8 before being added.
  *charset = "utf-8";
  return true;
}

void URLRequestFileDirJob::OnListFile(
    const DirectoryLister::DirectoryListerData& data) {
  // We wait to write out the header until we get the first file, so that we
  // can catch errors from DirectoryLister and show an error page.
  if (!wrote_header_) {
#if defined(OS_WIN)
    const base::string16& title = dir_path_.value();
#elif defined(OS_POSIX)
    // TODO(jungshik): Add SysNativeMBToUTF16 to sys_string_conversions.
    // On Mac, need to add NFKC->NFC conversion either here or in file_path.
    // On Linux, the file system encoding is not defined, but we assume that
    // SysNativeMBToWide takes care of it at least for now. We can try something
    // more sophisticated if necessary later.
    const base::string16& title = base::WideToUTF16(
        base::SysNativeMBToWide(dir_path_.value()));
#endif
    data_.append(GetDirectoryListingHeader(title));
    wrote_header_ = true;
  }

#if defined(OS_WIN)
  std::string raw_bytes;  // Empty on Windows means UTF-8 encoded name.
#elif defined(OS_POSIX)
  // TOOD(jungshik): The same issue as for the directory name.
  base::FilePath filename = data.info.GetName();
  const std::string& raw_bytes = filename.value();
#endif
  data_.append(GetDirectoryListingEntry(
      data.info.GetName().LossyDisplayName(),
      raw_bytes,
      data.info.IsDirectory(),
      data.info.GetSize(),
      data.info.GetLastModifiedTime()));

  // TODO(darin): coalesce more?
  CompleteRead();
}

void URLRequestFileDirJob::OnListDone(int error) {
  DCHECK(!canceled_);
  if (error != OK) {
    read_pending_ = false;
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, error));
  } else {
    list_complete_ = true;
    CompleteRead();
  }
}

URLRequestFileDirJob::~URLRequestFileDirJob() {}

void URLRequestFileDirJob::CompleteRead() {
  if (read_pending_) {
    int bytes_read;
    if (FillReadBuffer(read_buffer_->data(), read_buffer_length_,
                       &bytes_read)) {
      // We completed the read, so reset the read buffer.
      read_pending_ = false;
      read_buffer_ = NULL;
      read_buffer_length_ = 0;

      SetStatus(URLRequestStatus());
      NotifyReadComplete(bytes_read);
    } else {
      NOTREACHED();
      // TODO: Better error code.
      NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, 0));
    }
  }
}

bool URLRequestFileDirJob::FillReadBuffer(char* buf, int buf_size,
                                          int* bytes_read) {
  DCHECK(bytes_read);

  *bytes_read = 0;

  int count = std::min(buf_size, static_cast<int>(data_.size()));
  if (count) {
    memcpy(buf, &data_[0], count);
    data_.erase(0, count);
    *bytes_read = count;
    return true;
  } else if (list_complete_) {
    // EOF
    return true;
  }
  return false;
}

}  // namespace net
