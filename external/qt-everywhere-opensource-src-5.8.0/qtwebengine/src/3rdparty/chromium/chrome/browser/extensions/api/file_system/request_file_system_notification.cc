// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/request_file_system_notification.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/extensions/extension_app_icon_loader.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"

using file_manager::Volume;
using message_center::Notification;

namespace {

// Extension icon size for the notification.
const int kIconSize = 48;

std::unique_ptr<Notification> CreateAutoGrantedNotification(
    const extensions::Extension& extension,
    const base::WeakPtr<Volume>& volume,
    bool writable,
    message_center::NotificationDelegate* delegate) {
  DCHECK(delegate);

  // If the volume is gone, then do not show the notification.
  if (!volume.get())
    return std::unique_ptr<Notification>(nullptr);

  const std::string notification_id =
      extension.id() + "-" + volume->volume_id();
  message_center::RichNotificationData data;
  data.clickable = false;

  // TODO(mtomasz): Share this code with RequestFileSystemDialogView.
  const base::string16 display_name =
      base::UTF8ToUTF16(!volume->volume_label().empty() ? volume->volume_label()
                                                        : volume->volume_id());
  const base::string16 message = l10n_util::GetStringFUTF16(
      writable
          ? IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_NOTIFICATION_WRITABLE_MESSAGE
          : IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_NOTIFICATION_MESSAGE,
      display_name);

  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      base::UTF8ToUTF16(extension.name()), message,
      gfx::Image(),      // Updated asynchronously later.
      base::string16(),  // display_source
      GURL(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 notification_id),
      data, delegate));

  return notification;
}

}  // namespace

// static
void RequestFileSystemNotification::ShowAutoGrantedNotification(
    Profile* profile,
    const extensions::Extension& extension,
    const base::WeakPtr<Volume>& volume,
    bool writable) {
  DCHECK(profile);
  scoped_refptr<RequestFileSystemNotification>
      request_file_system_notification = make_scoped_refptr(
          new RequestFileSystemNotification(profile, extension));
  std::unique_ptr<message_center::Notification> notification(
      CreateAutoGrantedNotification(
          extension, volume, writable,
          request_file_system_notification.get() /* delegate */));
  if (notification.get())
    request_file_system_notification->Show(std::move(notification));
}

void RequestFileSystemNotification::OnAppImageUpdated(
    const std::string& id, const gfx::ImageSkia& image) {
  extension_icon_.reset(new gfx::Image(image));

  // If there is a pending notification, then show it now.
  if (pending_notification_.get()) {
    pending_notification_->set_icon(*extension_icon_.get());
    g_browser_process->message_center()->AddNotification(
        std::move(pending_notification_));
  }
}

RequestFileSystemNotification::RequestFileSystemNotification(
    Profile* profile,
    const extensions::Extension& extension)
    : icon_loader_(
          new extensions::ExtensionAppIconLoader(profile, kIconSize, this)) {
  icon_loader_->FetchImage(extension.id());
}

RequestFileSystemNotification::~RequestFileSystemNotification() {
}

void RequestFileSystemNotification::Show(
    std::unique_ptr<Notification> notification) {
  pending_notification_.reset(notification.release());
  // If the extension icon is not known yet, then defer showing the notification
  // until it is (from SetAppImage).
  if (!extension_icon_.get())
    return;

  pending_notification_->set_icon(*extension_icon_.get());
  g_browser_process->message_center()->AddNotification(
      std::move(pending_notification_));
}
