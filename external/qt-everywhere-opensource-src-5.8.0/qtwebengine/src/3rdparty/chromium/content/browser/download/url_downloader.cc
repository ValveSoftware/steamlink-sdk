// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/url_downloader.h"

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_save_info.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "ui/base/page_transition_types.h"

namespace content {

class UrlDownloader::RequestHandle : public DownloadRequestHandleInterface {
 public:
  RequestHandle(base::WeakPtr<UrlDownloader> downloader,
                base::WeakPtr<DownloadManagerImpl> download_manager_impl,
                scoped_refptr<base::SequencedTaskRunner> downloader_task_runner)
      : downloader_(downloader),
        download_manager_impl_(download_manager_impl),
        downloader_task_runner_(downloader_task_runner) {}
  RequestHandle(RequestHandle&& other)
      : downloader_(std::move(other.downloader_)),
        download_manager_impl_(std::move(other.download_manager_impl_)),
        downloader_task_runner_(std::move(other.downloader_task_runner_)) {}
  RequestHandle& operator=(RequestHandle&& other) {
    downloader_ = std::move(other.downloader_);
    download_manager_impl_ = std::move(other.download_manager_impl_);
    downloader_task_runner_ = std::move(other.downloader_task_runner_);
    return *this;
  }

  // DownloadRequestHandleInterface
  WebContents* GetWebContents() const override { return nullptr; }
  DownloadManager* GetDownloadManager() const override {
    return download_manager_impl_ ? download_manager_impl_.get() : nullptr;
  }
  void PauseRequest() const override {
    downloader_task_runner_->PostTask(
        FROM_HERE, base::Bind(&UrlDownloader::PauseRequest, downloader_));
  }
  void ResumeRequest() const override {
    downloader_task_runner_->PostTask(
        FROM_HERE, base::Bind(&UrlDownloader::ResumeRequest, downloader_));
  }
  void CancelRequest() const override {
    downloader_task_runner_->PostTask(
        FROM_HERE, base::Bind(&UrlDownloader::CancelRequest, downloader_));
  }

 private:
  base::WeakPtr<UrlDownloader> downloader_;
  base::WeakPtr<DownloadManagerImpl> download_manager_impl_;
  scoped_refptr<base::SequencedTaskRunner> downloader_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(RequestHandle);
};

// static
std::unique_ptr<UrlDownloader> UrlDownloader::BeginDownload(
    base::WeakPtr<DownloadManagerImpl> download_manager,
    std::unique_ptr<net::URLRequest> request,
    const Referrer& referrer) {
  if (!referrer.url.is_valid())
    request->SetReferrer(std::string());
  else
    request->SetReferrer(referrer.url.spec());

  if (request->url().SchemeIs(url::kBlobScheme))
    return nullptr;

  // From this point forward, the |UrlDownloader| is responsible for
  // |started_callback|.
  std::unique_ptr<UrlDownloader> downloader(
      new UrlDownloader(std::move(request), download_manager));
  downloader->Start();

  return downloader;
}

UrlDownloader::UrlDownloader(std::unique_ptr<net::URLRequest> request,
                             base::WeakPtr<DownloadManagerImpl> manager)
    : request_(std::move(request)),
      manager_(manager),
      core_(request_.get(), this),
      weak_ptr_factory_(this) {}

UrlDownloader::~UrlDownloader() {
}

void UrlDownloader::Start() {
  DCHECK(!request_->is_pending());

  if (!request_->status().is_success())
    return;

  request_->set_delegate(this);
  request_->Start();
}

void UrlDownloader::OnReceivedRedirect(net::URLRequest* request,
                                       const net::RedirectInfo& redirect_info,
                                       bool* defer_redirect) {
  DVLOG(1) << "OnReceivedRedirect: " << request_->url().spec();

  // We are going to block redirects even if DownloadRequestCore allows it.  No
  // redirects are expected for download requests that are made without a
  // renderer, which are currently exclusively resumption requests. Since there
  // is no security policy being applied here, it's safer to block redirects and
  // revisit if some previously unknown legitimate use case arises for redirects
  // while resuming.
  core_.OnWillAbort(DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE);
  request_->CancelWithError(net::ERR_UNSAFE_REDIRECT);
}

void UrlDownloader::OnResponseStarted(net::URLRequest* request) {
  DVLOG(1) << "OnResponseStarted: " << request_->url().spec();

  if (!request_->status().is_success()) {
    ResponseCompleted();
    return;
  }

  if (core_.OnResponseStarted(std::string()))
    StartReading(false);  // Read the first chunk.
  else
    ResponseCompleted();
}

void UrlDownloader::StartReading(bool is_continuation) {
  int bytes_read;

  // Make sure we track the buffer in at least one place.  This ensures it gets
  // deleted even in the case the request has already finished its job and
  // doesn't use the buffer.
  scoped_refptr<net::IOBuffer> buf;
  int buf_size;
  if (!core_.OnWillRead(&buf, &buf_size, -1)) {
    request_->CancelWithError(net::ERR_ABORTED);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&UrlDownloader::ResponseCompleted,
                              weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(buf.get());
  DCHECK(buf_size > 0);

  request_->Read(buf.get(), buf_size, &bytes_read);

  // If IO is pending, wait for the URLRequest to call OnReadCompleted.
  if (request_->status().is_io_pending())
    return;

  if (!is_continuation || bytes_read <= 0) {
    OnReadCompleted(request_.get(), bytes_read);
  } else {
    // Else, trigger OnReadCompleted asynchronously to avoid starving the IO
    // thread in case the URLRequest can provide data synchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&UrlDownloader::OnReadCompleted,
                   weak_ptr_factory_.GetWeakPtr(), request_.get(), bytes_read));
  }
}

void UrlDownloader::OnReadCompleted(net::URLRequest* request, int bytes_read) {
  DVLOG(1) << "OnReadCompleted: \"" << request_->url().spec() << "\""
           << " bytes_read = " << bytes_read;

  // bytes_read == -1 always implies an error.
  if (bytes_read == -1 || !request_->status().is_success()) {
    ResponseCompleted();
    return;
  }

  DCHECK(bytes_read >= 0);
  DCHECK(request_->status().is_success());

  bool defer = false;
  if (!core_.OnReadCompleted(bytes_read, &defer)) {
    request_->CancelWithError(net::ERR_ABORTED);
    return;
  } else if (defer) {
    return;
  }

  if (!request_->status().is_success())
    return;

  if (bytes_read > 0) {
    StartReading(true);  // Read the next chunk.
  } else {
    // URLRequest reported an EOF. Call ResponseCompleted.
    DCHECK_EQ(0, bytes_read);
    ResponseCompleted();
  }
}

void UrlDownloader::ResponseCompleted() {
  DVLOG(1) << "ResponseCompleted: " << request_->url().spec();

  core_.OnResponseCompleted(request_->status());
  Destroy();
}

void UrlDownloader::OnStart(
    std::unique_ptr<DownloadCreateInfo> create_info,
    std::unique_ptr<ByteStreamReader> stream_reader,
    const DownloadUrlParameters::OnStartedCallback& callback) {
  create_info->request_handle.reset(
      new RequestHandle(weak_ptr_factory_.GetWeakPtr(), manager_,
                        base::SequencedTaskRunnerHandle::Get()));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&DownloadManagerImpl::StartDownload,
                                     manager_, base::Passed(&create_info),
                                     base::Passed(&stream_reader), callback));
}

void UrlDownloader::OnReadyToRead() {
  if (request_->status().is_success())
    StartReading(false);  // Read the next chunk (OK to complete synchronously).
  else
    ResponseCompleted();
}

void UrlDownloader::PauseRequest() {
  core_.PauseRequest();
}

void UrlDownloader::ResumeRequest() {
  core_.ResumeRequest();
}

void UrlDownloader::CancelRequest() {
  Destroy();
}

void UrlDownloader::Destroy() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DownloadManagerImpl::RemoveUrlDownloader, manager_, this));
}

}  // namespace content
