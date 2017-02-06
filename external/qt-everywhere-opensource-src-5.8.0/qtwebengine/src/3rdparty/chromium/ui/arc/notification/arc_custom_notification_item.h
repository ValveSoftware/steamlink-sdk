// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_ITEM_H_
#define UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_ITEM_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/gfx/image/image_skia.h"

namespace arc {

class ArcCustomNotificationItem : public ArcNotificationItem {
 public:
  class Observer {
   public:
    // Invoked when the notification data for this item has changed.
    virtual void OnItemDestroying() = 0;

    // Invoked when the notification data for the item is updated.
    virtual void OnItemUpdated() = 0;

   protected:
    virtual ~Observer() = default;
  };

  ArcCustomNotificationItem(ArcNotificationManager* manager,
                            message_center::MessageCenter* message_center,
                            const std::string& notification_key,
                            const AccountId& profile_id);
  ~ArcCustomNotificationItem() override;

  void UpdateWithArcNotificationData(
      const mojom::ArcNotificationData& data) override;

  void CloseFromCloseButton();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Increment |window_ref_count_| and a CreateNotificationWindow request
  // is sent when |window_ref_count_| goes from zero to one.
  void IncrementWindowRefCount();

  // Decrement |window_ref_count_| and a CloseNotificationWindow request
  // is sent when |window_ref_count_| goes from one to zero.
  void DecrementWindowRefCount();

  bool pinned() const { return pinned_; }
  const gfx::ImageSkia& snapshot() const { return snapshot_; }

 private:
  bool pinned_ = false;
  gfx::ImageSkia snapshot_;
  int window_ref_count_ = 0;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ArcCustomNotificationItem);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_ITEM_H_
