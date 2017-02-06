// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_ITEM_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_ITEM_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/signin/core/account_id/account_id.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/arc/notification/arc_notification_manager.h"
#include "ui/message_center/message_center.h"

namespace arc {

// The class represents each ARC notification. One instance of this class
// corresponds to one ARC notification.
class ArcNotificationItem {
 public:
  ArcNotificationItem(ArcNotificationManager* manager,
                      message_center::MessageCenter* message_center,
                      const std::string& notification_key,
                      const AccountId& profile_id);
  virtual ~ArcNotificationItem();

  virtual void UpdateWithArcNotificationData(
      const mojom::ArcNotificationData& data);

  // Methods called from ArcNotificationManager:
  void OnClosedFromAndroid(bool by_user);

  // Methods called from ArcNotificationItemDelegate:
  void Close(bool by_user);
  void Click();
  void ButtonClick(int button_index);

  const std::string& notification_key() const { return notification_key_; }

 protected:
  static int ConvertAndroidPriority(int android_priority);
  static gfx::Image ConvertAndroidSmallIcon(
      const mojom::ArcBitmapPtr& arc_bitmap);

  // Checks whether there is on-going |notification_|. If so, cache the |data|
  // in |newer_data_| and returns true. Otherwise, returns false.
  bool CacheArcNotificationData(const mojom::ArcNotificationData& data);

  // Sets the pending |notification_|.
  void SetNotification(
      std::unique_ptr<message_center::Notification> notification);

  // Add |notification_| to message center and update again if there is
  // |newer_data_|.
  void AddToMessageCenter();

  bool CalledOnValidThread() const;

  const AccountId& profile_id() const { return profile_id_; }
  const std::string& notification_id() const { return notification_id_; }
  message_center::MessageCenter* message_center() { return message_center_; }
  ArcNotificationManager* manager() { return manager_; }

  message_center::Notification* pending_notification() {
    return notification_.get();
  }

 private:
  void OnImageDecoded(const SkBitmap& bitmap);

  ArcNotificationManager* const manager_;
  message_center::MessageCenter* const message_center_;
  const AccountId profile_id_;

  const std::string notification_key_;
  const std::string notification_id_;

  // Stores on-going notification data during the image decoding.
  // This field will be removed after removing async task of image decoding.
  std::unique_ptr<message_center::Notification> notification_;

  // The flag to indicate that the removing is initiated by the manager and we
  // don't need to notify a remove event to the manager.
  // This is true only when:
  //   (1) the notification is being removed
  //   (2) the removing is initiated by manager
  bool being_removed_by_manager_ = false;

  // Stores the latest notification data which is newer than the on-going data.
  // If the on-going data is either none or the latest, this is null.
  // This field will be removed after removing async task of image decoding.
  mojom::ArcNotificationDataPtr newer_data_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<ArcNotificationItem> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationItem);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_ITEM_H_
