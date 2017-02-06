// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_url_operation.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_fetcher.h"

namespace extensions {
namespace image_writer {

using content::BrowserThread;

WriteFromUrlOperation::WriteFromUrlOperation(
    base::WeakPtr<OperationManager> manager,
    const ExtensionId& extension_id,
    net::URLRequestContextGetter* request_context,
    GURL url,
    const std::string& hash,
    const std::string& device_path)
    : Operation(manager, extension_id, device_path),
      request_context_(request_context),
      url_(url),
      hash_(hash),
      download_continuation_() {}

WriteFromUrlOperation::~WriteFromUrlOperation() {
}

void WriteFromUrlOperation::StartImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  GetDownloadTarget(base::Bind(
      &WriteFromUrlOperation::Download,
      this,
      base::Bind(
          &WriteFromUrlOperation::VerifyDownload,
          this,
          base::Bind(
              &WriteFromUrlOperation::Unzip,
              this,
              base::Bind(&WriteFromUrlOperation::Write,
                         this,
                         base::Bind(&WriteFromUrlOperation::VerifyWrite,
                                    this,
                                    base::Bind(&WriteFromUrlOperation::Finish,
                                               this)))))));
}

void WriteFromUrlOperation::GetDownloadTarget(
    const base::Closure& continuation) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (IsCancelled()) {
    return;
  }

  if (url_.ExtractFileName().empty()) {
    if (!base::CreateTemporaryFileInDir(temp_dir_.path(), &image_path_)) {
      Error(error::kTempFileError);
      return;
    }
  } else {
    base::FilePath file_name =
        base::FilePath::FromUTF8Unsafe(url_.ExtractFileName());
    image_path_ = temp_dir_.path().Append(file_name);
  }

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, continuation);
}

void WriteFromUrlOperation::Download(const base::Closure& continuation) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (IsCancelled()) {
    return;
  }

  download_continuation_ = continuation;

  SetStage(image_writer_api::STAGE_DOWNLOAD);

  // Store the URL fetcher on this object so that it is destroyed before this
  // object is.
  url_fetcher_ = net::URLFetcher::Create(url_, net::URLFetcher::GET, this);

  url_fetcher_->SetRequestContext(request_context_);
  url_fetcher_->SaveResponseToFileAtPath(
      image_path_,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));

  AddCleanUpFunction(
      base::Bind(&WriteFromUrlOperation::DestroyUrlFetcher, this));

  url_fetcher_->Start();
}

void WriteFromUrlOperation::DestroyUrlFetcher() { url_fetcher_.reset(); }

void WriteFromUrlOperation::OnURLFetchUploadProgress(
    const net::URLFetcher* source,
    int64_t current,
    int64_t total) {
  // No-op
}

void WriteFromUrlOperation::OnURLFetchDownloadProgress(
    const net::URLFetcher* source,
    int64_t current,
    int64_t total) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (IsCancelled()) {
    url_fetcher_.reset(NULL);
  }

  int progress = (kProgressComplete * current) / total;

  SetProgress(progress);
}

void WriteFromUrlOperation::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (source->GetStatus().is_success() && source->GetResponseCode() == 200) {
    SetProgress(kProgressComplete);

    download_continuation_.Run();

    // Remove the reference to ourselves in this closure.
    download_continuation_ = base::Closure();
  } else {
    Error(error::kDownloadInterrupted);
  }
}

void WriteFromUrlOperation::VerifyDownload(const base::Closure& continuation) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (IsCancelled()) {
    return;
  }

  // Skip verify if no hash.
  if (hash_.empty()) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, continuation);
    return;
  }

  SetStage(image_writer_api::STAGE_VERIFYDOWNLOAD);

  GetMD5SumOfFile(
      image_path_,
      0,
      0,
      kProgressComplete,
      base::Bind(
          &WriteFromUrlOperation::VerifyDownloadCompare, this, continuation));
}

void WriteFromUrlOperation::VerifyDownloadCompare(
    const base::Closure& continuation,
    const std::string& download_hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (download_hash != hash_) {
    Error(error::kDownloadHashError);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &WriteFromUrlOperation::VerifyDownloadComplete, this, continuation));
}

void WriteFromUrlOperation::VerifyDownloadComplete(
    const base::Closure& continuation) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (IsCancelled()) {
    return;
  }

  SetProgress(kProgressComplete);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, continuation);
}

} // namespace image_writer
} // namespace extensions
