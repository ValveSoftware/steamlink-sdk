// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "content/browser/download/download_request_core.h"
#include "content/browser/loader/resource_handler.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {
class ByteStreamReader;
class ByteStreamWriter;
class DownloadRequestHandle;
class PowerSaveBlocker;
struct DownloadCreateInfo;

// Forwards data to the download thread.
class CONTENT_EXPORT DownloadResourceHandler
    : public ResourceHandler,
      public DownloadRequestCore::Delegate,
      public base::SupportsWeakPtr<DownloadResourceHandler> {
 public:
  struct DownloadTabInfo;

  // started_cb will be called exactly once on the UI thread.
  // |id| should be invalid if the id should be automatically assigned.
  DownloadResourceHandler(net::URLRequest* request);

  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* response,
                           bool* defer) override;

  // Send the download creation information to the download thread.
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;

  // Pass-through implementation.
  bool OnWillStart(const GURL& url, bool* defer) override;

  // Pass-through implementation.
  bool OnBeforeNetworkStart(const GURL& url, bool* defer) override;

  // Create a new buffer, which will be handed to the download thread for file
  // writing and deletion.
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;

  bool OnReadCompleted(int bytes_read, bool* defer) override;

  void OnResponseCompleted(const net::URLRequestStatus& status,
                           const std::string& security_info,
                           bool* defer) override;

  // N/A to this flavor of DownloadHandler.
  void OnDataDownloaded(int bytes_downloaded) override;

  void PauseRequest();
  void ResumeRequest();

  // May result in this object being deleted by its owner.
  void CancelRequest();

  std::string DebugString() const;

 private:
  ~DownloadResourceHandler() override;

  // DownloadRequestCore::Delegate
  void OnStart(
      std::unique_ptr<DownloadCreateInfo> download_create_info,
      std::unique_ptr<ByteStreamReader> stream_reader,
      const DownloadUrlParameters::OnStartedCallback& callback) override;
  void OnReadyToRead() override;

  // Stores information about the download that must be acquired on the UI
  // thread before StartOnUIThread is called.
  // Created on IO thread, but only accessed on UI thread. |tab_info_| holds
  // the pointer until we pass it off to StartOnUIThread or DeleteSoon.
  // Marked as a std::unique_ptr<> to indicate ownership of the structure, but
  // deletion must occur on the IO thread.
  std::unique_ptr<DownloadTabInfo> tab_info_;

  DownloadRequestCore core_;
  DISALLOW_COPY_AND_ASSIGN(DownloadResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_HANDLER_H_
