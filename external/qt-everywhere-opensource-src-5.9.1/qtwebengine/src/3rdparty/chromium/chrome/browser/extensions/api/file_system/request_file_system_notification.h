// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_REQUEST_FILE_SYSTEM_NOTIFICATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_REQUEST_FILE_SYSTEM_NOTIFICATION_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "ui/message_center/notification_delegate.h"

class Profile;

namespace extensions {
class Extension;
}  // namespace extensions

namespace file_manager {
class Volume;
}  // namespace file_manager

namespace gfx {
class Image;
class ImageSkia;
}  // namespace gfx

namespace message_center {
class Notification;
}  // namespace message_center

// Shows notifications for the chrome.fileSystem.requestFileSystem() API.
class RequestFileSystemNotification
    : public message_center::NotificationDelegate,
      public AppIconLoaderDelegate {
 public:
  // Shows a notification about automatically granted access to a file system.
  static void ShowAutoGrantedNotification(
      Profile* profile,
      const extensions::Extension& extension,
      const base::WeakPtr<file_manager::Volume>& volume,
      bool writable);

 private:
  RequestFileSystemNotification(Profile* profile,
                                const extensions::Extension& extension);
  ~RequestFileSystemNotification() override;

  // Shows the notification. Can be called only once.
  void Show(std::unique_ptr<message_center::Notification> notification);

  // AppIconLoaderDelegate overrides:
  void OnAppImageUpdated(const std::string& id,
                         const gfx::ImageSkia& image) override;

  std::unique_ptr<AppIconLoader> icon_loader_;
  std::unique_ptr<gfx::Image> extension_icon_;
  std::unique_ptr<message_center::Notification> pending_notification_;

  DISALLOW_COPY_AND_ASSIGN(RequestFileSystemNotification);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_REQUEST_FILE_SYSTEM_NOTIFICATION_H_
