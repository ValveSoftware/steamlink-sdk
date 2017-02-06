// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_item.h"

#include <algorithm>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "components/arc/bitmap/bitmap_type_converters.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"

namespace arc {

namespace {

constexpr char kNotifierId[] = "ARC_NOTIFICATION";
constexpr char kNotificationIdPrefix[] = "ARC_NOTIFICATION_";

SkBitmap DecodeImage(const std::vector<uint8_t>& data) {
  DCHECK(base::WorkerPool::RunsTasksOnCurrentThread());
  DCHECK(!data.empty());  // empty string should be handled in caller.

  // We may decode an image in the browser process since it has been generated
  // in NotificationListerService in Android and should be safe.
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(&data[0], data.size(), &bitmap);
  return bitmap;
}

// Crops the image to proper size for Chrome Notification. It accepts only
// specified aspect ratio. Otherwise, it might be letterboxed.
SkBitmap CropImage(const SkBitmap& original_bitmap) {
  DCHECK_NE(0, original_bitmap.width());
  DCHECK_NE(0, original_bitmap.height());

  const SkSize container_size = SkSize::Make(
      message_center::kNotificationPreferredImageWidth,
      message_center::kNotificationPreferredImageHeight);
  const float container_aspect_ratio =
      static_cast<float>(message_center::kNotificationPreferredImageWidth) /
      message_center::kNotificationPreferredImageHeight;
  const float image_aspect_ratio =
      static_cast<float>(original_bitmap.width()) / original_bitmap.height();

  SkRect source_rect;
  if (image_aspect_ratio > container_aspect_ratio) {
    float width = original_bitmap.height() * container_aspect_ratio;
    source_rect = SkRect::MakeXYWH((original_bitmap.width() - width) / 2,
                                   0,
                                   width,
                                   original_bitmap.height());
  } else {
    float height = original_bitmap.width() / container_aspect_ratio;
    source_rect = SkRect::MakeXYWH(0,
                                   (original_bitmap.height() - height) / 2,
                                   original_bitmap.width(),
                                   height);
  }

  SkBitmap container_bitmap;
  container_bitmap.allocN32Pixels(container_size.width(),
                                  container_size.height());
  SkPaint paint;
  paint.setFilterQuality(kHigh_SkFilterQuality);
  SkCanvas container_image(container_bitmap);
  container_image.drawColor(message_center::kImageBackgroundColor);
  container_image.drawBitmapRect(
      original_bitmap, source_rect, SkRect::MakeSize(container_size), &paint);

  return container_bitmap;
}

class ArcNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit ArcNotificationDelegate(base::WeakPtr<ArcNotificationItem> item)
      : item_(item) {}

  void Close(bool by_user) override {
    if (item_)
      item_->Close(by_user);
  }

  // Indicates all notifications have a click handler. This changes the mouse
  // cursor on hover.
  // TODO(yoshiki): Return the correct value according to the content intent
  // and the flags.
  bool HasClickedListener() override { return true; }

  void Click() override {
    if (item_)
      item_->Click();
  }

  void ButtonClick(int button_index) override {
    if (item_)
      item_->ButtonClick(button_index);
  }

 private:
  // The destructor is private since this class is ref-counted.
  ~ArcNotificationDelegate() override {}

  base::WeakPtr<ArcNotificationItem> item_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationDelegate);
};

}  // anonymous namespace

ArcNotificationItem::ArcNotificationItem(
    ArcNotificationManager* manager,
    message_center::MessageCenter* message_center,
    const std::string& notification_key,
    const AccountId& profile_id)
    : manager_(manager),
      message_center_(message_center),
      profile_id_(profile_id),
      notification_key_(notification_key),
      notification_id_(kNotificationIdPrefix + notification_key_),
      weak_ptr_factory_(this) {}

void ArcNotificationItem::UpdateWithArcNotificationData(
    const mojom::ArcNotificationData& data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(notification_key_ == data.key);

  // Check if a decode task is on-going or not. If |notification_| is non-null,
  // a decode task is on-going asynchronously. Otherwise, there is no task.
  // TODO(yoshiki): Refactor and remove this check by omitting image decoding
  // from here.
  if (CacheArcNotificationData(data))
    return;

  message_center::RichNotificationData rich_data;
  message_center::NotificationType type;

  switch (data.type) {
    case mojom::ArcNotificationType::BASIC:
      type = message_center::NOTIFICATION_TYPE_BASE_FORMAT;
      break;
    case mojom::ArcNotificationType::LIST:
      type = message_center::NOTIFICATION_TYPE_MULTIPLE;

      if (data.texts.is_null())
        break;

      for (size_t i = 0;
           i < std::min(data.texts.size(),
                        message_center::kNotificationMaximumItems - 1);
           i++) {
        rich_data.items.emplace_back(
            base::string16(), base::UTF8ToUTF16(data.texts.at(i).get()));
      }

      if (data.texts.size() > message_center::kNotificationMaximumItems) {
        // Show an elipsis as the 5th item if there are more than 5 items.
        rich_data.items.emplace_back(base::string16(), gfx::kEllipsisUTF16);
      } else if (data.texts.size() ==
                 message_center::kNotificationMaximumItems) {
        // Show the 5th item if there are exact 5 items.
        rich_data.items.emplace_back(
            base::string16(),
            base::UTF8ToUTF16(data.texts.at(data.texts.size() - 1).get()));
      }
      break;
    case mojom::ArcNotificationType::IMAGE:
      type = message_center::NOTIFICATION_TYPE_IMAGE;

      if (!data.big_picture.is_null()) {
        rich_data.image = gfx::Image::CreateFrom1xBitmap(
            CropImage(data.big_picture.To<SkBitmap>()));
      }
      break;
    case mojom::ArcNotificationType::PROGRESS:
      type = message_center::NOTIFICATION_TYPE_PROGRESS;
      rich_data.timestamp = base::Time::UnixEpoch() +
                            base::TimeDelta::FromMilliseconds(data.time);
      rich_data.progress = std::max(
          0, std::min(100, static_cast<int>(std::round(
                               static_cast<float>(data.progress_current) /
                               data.progress_max * 100))));
      break;
  }
  DCHECK(IsKnownEnumValue(data.type)) << "Unsupported notification type: "
                                      << data.type;

  for (size_t i = 0; i < data.buttons.size(); i++) {
    rich_data.buttons.emplace_back(
        base::UTF8ToUTF16(data.buttons.at(i)->label.get()));
  }

  // If the client is old (version < 1), both |no_clear| and |ongoing_event|
  // are false.
  rich_data.pinned = (data.no_clear || data.ongoing_event);

  rich_data.priority = ConvertAndroidPriority(data.priority);
  rich_data.small_image = ConvertAndroidSmallIcon(data.small_icon);

  // The identifier of the notifier, which is used to distinguish the notifiers
  // in the message center.
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT, kNotifierId);
  notifier_id.profile_id = profile_id_.GetUserEmail();

  DCHECK(!data.title.is_null());
  DCHECK(!data.message.is_null());
  SetNotification(base::WrapUnique(new message_center::Notification(
      type, notification_id_, base::UTF8ToUTF16(data.title.get()),
      base::UTF8ToUTF16(data.message.get()),
      gfx::Image(),              // icon image: Will be overriden later.
      base::UTF8ToUTF16("arc"),  // display source
      GURL(),                    // empty origin url, for system component
      notifier_id, rich_data,
      new ArcNotificationDelegate(weak_ptr_factory_.GetWeakPtr()))));

  DCHECK(!data.icon_data.is_null());
  if (data.icon_data.size() == 0) {
    OnImageDecoded(SkBitmap());  // Pass an empty bitmap.
    return;
  }

  // TODO(yoshiki): Remove decoding by passing a bitmap directly from Android.
  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true).get(), FROM_HERE,
      base::Bind(&DecodeImage, data.icon_data.storage()),
      base::Bind(&ArcNotificationItem::OnImageDecoded,
                 weak_ptr_factory_.GetWeakPtr()));
}

ArcNotificationItem::~ArcNotificationItem() {}

void ArcNotificationItem::OnClosedFromAndroid(bool by_user) {
  being_removed_by_manager_ = true;  // Closing is initiated by the manager.
  message_center_->RemoveNotification(notification_id_, by_user);
}

void ArcNotificationItem::Close(bool by_user) {
  if (being_removed_by_manager_) {
    // Closing is caused by the manager, so we don't need to nofify a close
    // event to the manager.
    return;
  }

  // Do not touch its any members afterwards, because this instance will be
  // destroyed in the following call
  manager_->SendNotificationRemovedFromChrome(notification_key_);
}

void ArcNotificationItem::Click() {
  manager_->SendNotificationClickedOnChrome(notification_key_);
}

void ArcNotificationItem::ButtonClick(int button_index) {
  manager_->SendNotificationButtonClickedOnChrome(
      notification_key_, button_index);
}

// Converts from Android notification priority to Chrome notification priority.
// On Android, PRIORITY_DEFAULT does not pop up, so this maps PRIORITY_DEFAULT
// to Chrome's -1 to adapt that behavior. Also, this maps PRIORITY_LOW and _HIGH
// to -2 and 0 respectively to adjust the value with keeping the order among
// _LOW, _DEFAULT and _HIGH.
// static
int ArcNotificationItem::ConvertAndroidPriority(int android_priority) {
  switch (android_priority) {
    case -2:  // PRIORITY_MIN
    case -1:  // PRIORITY_LOW
      return -2;
    case 0:  // PRIORITY_DEFAULT
      return -1;
    case 1:  // PRIORITY_HIGH
      return 0;
    case 2:  // PRIORITY_MAX
      return 2;
    default:
      NOTREACHED() << "Invalid Priority: " << android_priority;
      return 0;
  }
}

// static
gfx::Image ArcNotificationItem::ConvertAndroidSmallIcon(
    const mojom::ArcBitmapPtr& arc_bitmap) {
  if (arc_bitmap.is_null())
    return gfx::Image();

  return gfx::Image::CreateFrom1xBitmap(arc_bitmap.To<SkBitmap>());
}

bool ArcNotificationItem::CacheArcNotificationData(
    const mojom::ArcNotificationData& data) {
  if (!notification_)
    return false;

  // Store the latest data to the |newer_data_| property if there is a pending
  // |notification_|.
  // If old |newer_data_| has been stored, discard the old one.
  newer_data_ = data.Clone();
  return true;
}

void ArcNotificationItem::SetNotification(
    std::unique_ptr<message_center::Notification> notification) {
  notification_ = std::move(notification);
}

void ArcNotificationItem::AddToMessageCenter() {
  DCHECK(notification_);
  message_center_->AddNotification(std::move(notification_));

  if (newer_data_) {
    // There is the newer data, so updates again.
    mojom::ArcNotificationDataPtr data(std::move(newer_data_));
    UpdateWithArcNotificationData(*data);
  }
}

bool ArcNotificationItem::CalledOnValidThread() const {
  return thread_checker_.CalledOnValidThread();
}

void ArcNotificationItem::OnImageDecoded(const SkBitmap& bitmap) {
  DCHECK(thread_checker_.CalledOnValidThread());

  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);
  notification_->set_icon(image);
  AddToMessageCenter();
}

}  // namespace arc
