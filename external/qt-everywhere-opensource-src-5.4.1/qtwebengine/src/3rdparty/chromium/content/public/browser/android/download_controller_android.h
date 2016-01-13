// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_ANDROID_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_ANDROID_H_

#include "content/common/content_export.h"
#include "content/public/common/context_menu_params.h"

namespace content {
class DownloadItem;
class WebContents;

// Interface to request GET downloads and send notifications for POST
// downloads.
class CONTENT_EXPORT DownloadControllerAndroid {
 public:
  // Returns the singleton instance of the DownloadControllerAndroid.
  static DownloadControllerAndroid* Get();

  // Starts a new download request with Android. Should be called on the
  // UI thread.
  virtual void CreateGETDownload(int render_process_id, int render_view_id,
                                 int request_id) = 0;

  // Should be called when a download is started. It can be either a GET
  // request with authentication or a POST request. Notifies the embedding
  // app about the download. Should be called on the UI thread.
  virtual void OnDownloadStarted(DownloadItem* download_item) = 0;

  // Called when a download is initiated by context menu.
  virtual void StartContextMenuDownload(
      const ContextMenuParams& params, WebContents* web_contents,
      bool is_link) = 0;

  // Called when a dangerous download item is verified or rejected.
  virtual void DangerousDownloadValidated(
      WebContents* web_contents, int download_id, bool accept) = 0;

 protected:
  virtual ~DownloadControllerAndroid() {};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_ANDROID_H_
