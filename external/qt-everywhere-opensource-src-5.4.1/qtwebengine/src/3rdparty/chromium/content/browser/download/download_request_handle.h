// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_HANDLE_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_HANDLE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/common/content_export.h"

namespace content {
class DownloadManager;
class WebContents;

// A handle used by the download system for operations on the URLRequest
// or objects conditional on it (e.g. WebContentsImpl).
// This class needs to be copyable, so we can pass it across threads and not
// worry about lifetime or const-ness.
//
// DownloadRequestHandleInterface is defined for mocking purposes.
class CONTENT_EXPORT DownloadRequestHandleInterface {
 public:
  virtual ~DownloadRequestHandleInterface() {}

  // These functions must be called on the UI thread.
  virtual WebContents* GetWebContents() const = 0;
  virtual DownloadManager* GetDownloadManager() const = 0;

  // Pauses or resumes the matching URL request.
  virtual void PauseRequest() const = 0;
  virtual void ResumeRequest() const = 0;

  // Cancels the request.
  virtual void CancelRequest() const = 0;

  // Describes the object.
  virtual std::string DebugString() const = 0;
};


class CONTENT_EXPORT DownloadRequestHandle
    : public DownloadRequestHandleInterface {
 public:
  virtual ~DownloadRequestHandle();

  // Create a null DownloadRequestHandle: getters will return null, and
  // all actions are no-ops.
  // TODO(rdsmith): Ideally, actions would be forbidden rather than
  // no-ops, to confirm that no non-testing code actually uses
  // a null DownloadRequestHandle.  But for now, we need the no-op
  // behavior for unit tests.  Long-term, this should be fixed by
  // allowing mocking of ResourceDispatcherHost in unit tests.
  DownloadRequestHandle();

  // Note that |rdh| is required to be non-null.
  DownloadRequestHandle(const base::WeakPtr<DownloadResourceHandler>& handler,
                        int child_id,
                        int render_view_id,
                        int request_id);

  // Implement DownloadRequestHandleInterface interface.
  virtual WebContents* GetWebContents() const OVERRIDE;
  virtual DownloadManager* GetDownloadManager() const OVERRIDE;
  virtual void PauseRequest() const OVERRIDE;
  virtual void ResumeRequest() const OVERRIDE;
  virtual void CancelRequest() const OVERRIDE;
  virtual std::string DebugString() const OVERRIDE;

 private:
  base::WeakPtr<DownloadResourceHandler> handler_;

  // The ID of the child process that started the download.
  int child_id_;

  // The ID of the render view that started the download.
  int render_view_id_;

  // The ID associated with the request used for the download.
  int request_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_HANDLE_H_
