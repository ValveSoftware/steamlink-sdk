// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_ANDROID_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_ANDROID_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_item.h"
#include "content/public/common/context_menu_params.h"

namespace content {
class DownloadItem;
class WebContents;

// Interface to request GET downloads and send notifications for POST
// downloads.
class CONTENT_EXPORT DownloadControllerAndroid : public DownloadItem::Observer {
 public:
  // Returns the singleton instance of the DownloadControllerAndroid.
  static DownloadControllerAndroid* Get();

  // Called to set the DownloadControllerAndroid instance.
  static void SetDownloadControllerAndroid(
      DownloadControllerAndroid* download_controller);

  // UMA histogram enum for download cancellation reasons. Keep this
  // in sync with MobileDownloadCancelReason in histograms.xml. This should be
  // append only.
  enum DownloadCancelReason {
    CANCEL_REASON_NOT_CANCELED = 0,
    CANCEL_REASON_ACTION_BUTTON,
    CANCEL_REASON_NOTIFICATION_DISMISSED,
    CANCEL_REASON_OVERWRITE_INFOBAR_DISMISSED,
    CANCEL_REASON_NO_STORAGE_PERMISSION,
    CANCEL_REASON_MAX
  };
  static void RecordDownloadCancelReason(DownloadCancelReason reason);

  // Starts a new download request with Android. Should be called on the
  // UI thread.
  virtual void CreateGETDownload(int render_process_id, int render_view_id,
                                 int request_id, bool must_download) = 0;

  // Should be called when a download is started. It can be either a GET
  // request with authentication or a POST request. Notifies the embedding
  // app about the download. Should be called on the UI thread.
  virtual void OnDownloadStarted(DownloadItem* download_item) = 0;

  // Called when a download is initiated by context menu.
  virtual void StartContextMenuDownload(
      const ContextMenuParams& params, WebContents* web_contents,
      bool is_link, const std::string& extra_headers) = 0;

  // Called when a dangerous download item is verified or rejected.
  virtual void DangerousDownloadValidated(WebContents* web_contents,
                                          const std::string& download_guid,
                                          bool accept) = 0;

  // Callback when user permission prompt finishes. Args: whether file access
  // permission is acquired.
  typedef base::Callback<void(bool)> AcquireFileAccessPermissionCallback;

  // Called to prompt the user for file access permission. When finished,
  // |callback| will be executed.
  virtual void AcquireFileAccessPermission(
      WebContents* web_contents,
      const AcquireFileAccessPermissionCallback& callback) = 0;

  // Called by unit test to approve or disapprove file access request.
  virtual void SetApproveFileAccessRequestForTesting(bool approve) {};

  // Called to set the default download file name if it cannot be resolved
  // from url and content disposition
  virtual void SetDefaultDownloadFileName(const std::string& file_name) {}

 protected:
  ~DownloadControllerAndroid() override {};
  static DownloadControllerAndroid* download_controller_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_ANDROID_H_
