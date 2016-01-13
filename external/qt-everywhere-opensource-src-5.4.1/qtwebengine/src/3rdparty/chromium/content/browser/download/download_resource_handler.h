// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
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
      public base::SupportsWeakPtr<DownloadResourceHandler> {
 public:
  struct DownloadTabInfo;

  // Size of the buffer used between the DownloadResourceHandler and the
  // downstream receiver of its output.
  static const int kDownloadByteStreamSize;

  // started_cb will be called exactly once on the UI thread.
  // |id| should be invalid if the id should be automatically assigned.
  DownloadResourceHandler(
      uint32 id,
      net::URLRequest* request,
      const DownloadUrlParameters::OnStartedCallback& started_cb,
      scoped_ptr<DownloadSaveInfo> save_info);

  virtual bool OnUploadProgress(uint64 position, uint64 size) OVERRIDE;

  virtual bool OnRequestRedirected(const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;

  // Send the download creation information to the download thread.
  virtual bool OnResponseStarted(ResourceResponse* response,
                                 bool* defer) OVERRIDE;

  // Pass-through implementation.
  virtual bool OnWillStart(const GURL& url, bool* defer) OVERRIDE;

  // Pass-through implementation.
  virtual bool OnBeforeNetworkStart(const GURL& url, bool* defer) OVERRIDE;

  // Create a new buffer, which will be handed to the download thread for file
  // writing and deletion.
  virtual bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                          int* buf_size,
                          int min_size) OVERRIDE;

  virtual bool OnReadCompleted(int bytes_read, bool* defer) OVERRIDE;

  virtual void OnResponseCompleted(const net::URLRequestStatus& status,
                                   const std::string& security_info,
                                   bool* defer) OVERRIDE;

  // N/A to this flavor of DownloadHandler.
  virtual void OnDataDownloaded(int bytes_downloaded) OVERRIDE;

  void PauseRequest();
  void ResumeRequest();

  // May result in this object being deleted by its owner.
  void CancelRequest();

  std::string DebugString() const;

 private:
  virtual ~DownloadResourceHandler();

  // Arrange for started_cb_ to be called on the UI thread with the
  // below values, nulling out started_cb_.  Should only be called
  // on the IO thread.
  void CallStartedCB(DownloadItem* item,
                     DownloadInterruptReason interrupt_reason);

  uint32 download_id_;
  // This is read only on the IO thread, but may only
  // be called on the UI thread.
  DownloadUrlParameters::OnStartedCallback started_cb_;
  scoped_ptr<DownloadSaveInfo> save_info_;

  // Stores information about the download that must be acquired on the UI
  // thread before StartOnUIThread is called.
  // Created on IO thread, but only accessed on UI thread. |tab_info_| holds
  // the pointer until we pass it off to StartOnUIThread or DeleteSoon.
  DownloadTabInfo* tab_info_;

  // Data flow
  scoped_refptr<net::IOBuffer> read_buffer_;       // From URLRequest.
  scoped_ptr<ByteStreamWriter> stream_writer_; // To rest of system.

  // Keeps the system from sleeping while this ResourceHandler is alive. If the
  // system enters power saving mode while a request is alive, it can cause the
  // request to fail and the associated download will be interrupted.
  scoped_ptr<PowerSaveBlocker> power_save_blocker_;

  // The following are used to collect stats.
  base::TimeTicks download_start_time_;
  base::TimeTicks last_read_time_;
  base::TimeTicks last_stream_pause_time_;
  base::TimeDelta total_pause_time_;
  size_t last_buffer_size_;
  int64 bytes_read_;

  int pause_count_;
  bool was_deferred_;

  // For DCHECKing
  bool on_response_started_called_;

  static const int kReadBufSize = 32768;  // bytes
  static const int kThrottleTimeMs = 200;  // milliseconds

  DISALLOW_COPY_AND_ASSIGN(DownloadResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_HANDLER_H_
