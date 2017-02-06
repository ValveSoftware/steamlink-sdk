// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_H_
#define CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/browser/download/download_request_core.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/common/referrer.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"

namespace content {
class ByteStreamReader;
struct DownloadCreateInfo;
class DownloadManagerImpl;

class UrlDownloader : public net::URLRequest::Delegate,
                      public DownloadRequestCore::Delegate {
 public:
  UrlDownloader(std::unique_ptr<net::URLRequest> request,
                base::WeakPtr<DownloadManagerImpl> manager);
  ~UrlDownloader() override;

  static std::unique_ptr<UrlDownloader> BeginDownload(
      base::WeakPtr<DownloadManagerImpl> download_manager,
      std::unique_ptr<net::URLRequest> request,
      const Referrer& referrer);

 private:
  class RequestHandle;

  void Start();

  // URLRequest::Delegate:
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  void StartReading(bool is_continuation);
  void ResponseCompleted();

  // DownloadRequestCore::Delegate
  void OnStart(
      std::unique_ptr<DownloadCreateInfo> download_create_info,
      std::unique_ptr<ByteStreamReader> stream_reader,
      const DownloadUrlParameters::OnStartedCallback& callback) override;
  void OnReadyToRead() override;

  void PauseRequest();
  void ResumeRequest();
  void CancelRequest();

  // Called when the UrlDownloader is done with the request. Posts a task to
  // remove itself from its download manager, which in turn would cause the
  // UrlDownloader to be freed.
  void Destroy();

  std::unique_ptr<net::URLRequest> request_;
  base::WeakPtr<DownloadManagerImpl> manager_;
  DownloadRequestCore core_;

  base::WeakPtrFactory<UrlDownloader> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_H_
