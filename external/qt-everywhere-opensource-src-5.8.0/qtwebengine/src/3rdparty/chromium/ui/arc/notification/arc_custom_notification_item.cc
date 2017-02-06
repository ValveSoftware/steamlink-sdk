// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_custom_notification_item.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/bitmap/bitmap_type_converters.h"
#include "ui/arc/notification/arc_custom_notification_view.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"

namespace arc {

namespace {

constexpr char kNotifierId[] = "ARC_NOTIFICATION";

class ArcNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit ArcNotificationDelegate(ArcCustomNotificationItem* item)
      : item_(item) {}

  std::unique_ptr<views::View> CreateCustomContent() override {
    return base::MakeUnique<ArcCustomNotificationView>(item_);
  }

 private:
  // The destructor is private since this class is ref-counted.
  ~ArcNotificationDelegate() override {}

  ArcCustomNotificationItem* const item_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationDelegate);
};

}  // namespace

ArcCustomNotificationItem::ArcCustomNotificationItem(
    ArcNotificationManager* manager,
    message_center::MessageCenter* message_center,
    const std::string& notification_key,
    const AccountId& profile_id)
    : ArcNotificationItem(manager,
                          message_center,
                          notification_key,
                          profile_id) {
}

ArcCustomNotificationItem::~ArcCustomNotificationItem() {
  FOR_EACH_OBSERVER(Observer, observers_, OnItemDestroying());
}

void ArcCustomNotificationItem::UpdateWithArcNotificationData(
    const mojom::ArcNotificationData& data) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(notification_key(), data.key);

  if (CacheArcNotificationData(data))
    return;

  message_center::RichNotificationData rich_data;
  rich_data.pinned = (data.no_clear || data.ongoing_event);
  rich_data.priority = ConvertAndroidPriority(data.priority);
  rich_data.small_image = ConvertAndroidSmallIcon(data.small_icon);

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT, kNotifierId);
  notifier_id.profile_id = profile_id().GetUserEmail();

  DCHECK(!data.title.is_null());
  DCHECK(!data.message.is_null());
  SetNotification(base::MakeUnique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_id(),
      base::UTF8ToUTF16(data.title.get()),
      base::UTF8ToUTF16(data.message.get()), gfx::Image(),
      base::UTF8ToUTF16("arc"),  // display source
      GURL(),                    // empty origin url, for system component
      notifier_id, rich_data, new ArcNotificationDelegate(this)));

  pinned_ = rich_data.pinned;

  if (data.snapshot_image.is_null()) {
    snapshot_ = gfx::ImageSkia();
  } else {
    snapshot_ = gfx::ImageSkia(gfx::ImageSkiaRep(
        data.snapshot_image.To<SkBitmap>(), data.snapshot_image_scale));
  }

  FOR_EACH_OBSERVER(Observer, observers_, OnItemUpdated());

  AddToMessageCenter();
}

void ArcCustomNotificationItem::CloseFromCloseButton() {
  // Needs to manually remove notification from MessageCenter because
  // the floating close button is not part of MessageCenter.
  message_center()->RemoveNotification(notification_id(), true);
  Close(true);
}

void ArcCustomNotificationItem::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ArcCustomNotificationItem::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ArcCustomNotificationItem::IncrementWindowRefCount() {
  ++window_ref_count_;
  if (window_ref_count_ == 1)
    manager()->CreateNotificationWindow(notification_key());
}

void ArcCustomNotificationItem::DecrementWindowRefCount() {
  DCHECK_GT(window_ref_count_, 0);
  --window_ref_count_;
  if (window_ref_count_ == 0)
    manager()->CloseNotificationWindow(notification_key());
}

}  // namespace arc
