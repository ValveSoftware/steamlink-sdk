// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_list.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/notification_types.h"

namespace message_center {

namespace {

bool ShouldShowNotificationAsPopup(
    const Notification& notification,
    const NotificationBlockers& blockers) {
  for (const auto& blocker : blockers) {
    if (!blocker->ShouldShowNotificationAsPopup(notification))
      return false;
  }
  return true;
}

}  // namespace

bool ComparePriorityTimestampSerial::operator()(Notification* n1,
                                                Notification* n2) {
  if (n1->priority() > n2->priority())  // Higher pri go first.
    return true;
  if (n1->priority() < n2->priority())
    return false;
  return CompareTimestampSerial()(n1, n2);
}

bool CompareTimestampSerial::operator()(Notification* n1, Notification* n2) {
  if (n1->timestamp() > n2->timestamp())  // Newer come first.
    return true;
  if (n1->timestamp() < n2->timestamp())
    return false;
  if (n1->serial_number() > n2->serial_number())  // Newer come first.
    return true;
  if (n1->serial_number() < n2->serial_number())
    return false;
  return false;
}

NotificationList::NotificationList(MessageCenter* message_center)
    : message_center_(message_center),
      quiet_mode_(false) {
}

NotificationList::~NotificationList() {
}

void NotificationList::SetNotificationsShown(
    const NotificationBlockers& blockers,
    std::set<std::string>* updated_ids) {
  Notifications notifications = GetVisibleNotifications(blockers);

  for (auto iter = notifications.begin(); iter != notifications.end(); ++iter) {
    Notification* notification = *iter;
    bool was_popup = notification->shown_as_popup();
    bool was_read = notification->IsRead();
    if (notification->priority() < SYSTEM_PRIORITY)
      notification->set_shown_as_popup(true);
    notification->set_is_read(true);
    if (updated_ids && !(was_popup && was_read))
      updated_ids->insert(notification->id());
  }
}

void NotificationList::AddNotification(
    std::unique_ptr<Notification> notification) {
  PushNotification(std::move(notification));
}

void NotificationList::UpdateNotificationMessage(
    const std::string& old_id,
    std::unique_ptr<Notification> new_notification) {
  auto iter = GetNotification(old_id);
  if (iter == notifications_.end())
    return;

  new_notification->CopyState(iter->get());

  // Handles priority promotion. If the notification is already dismissed but
  // the updated notification has higher priority, it should re-appear as a
  // toast. Notifications coming from websites through the Web Notification API
  // will always re-appear on update.
  if ((*iter)->priority() < new_notification->priority() ||
      new_notification->notifier_id().type == NotifierId::WEB_PAGE) {
    new_notification->set_is_read(false);
    new_notification->set_shown_as_popup(false);
  }

  // Do not use EraseNotification and PushNotification, since we don't want to
  // change unread counts nor to update is_read/shown_as_popup states.
  notifications_.erase(iter);

  // We really don't want duplicate IDs.
  DCHECK(GetNotification(new_notification->id()) == notifications_.end());
  notifications_.insert(std::move(new_notification));
}

void NotificationList::RemoveNotification(const std::string& id) {
  EraseNotification(GetNotification(id));
}

NotificationList::Notifications NotificationList::GetNotificationsByNotifierId(
        const NotifierId& notifier_id) {
  Notifications notifications;
  for (const auto& notification : notifications_) {
    if (notification->notifier_id() == notifier_id)
      notifications.insert(notification.get());
  }
  return notifications;
}

bool NotificationList::SetNotificationIcon(const std::string& notification_id,
                                           const gfx::Image& image) {
  auto iter = GetNotification(notification_id);
  if (iter == notifications_.end())
    return false;
  (*iter)->set_icon(image);
  return true;
}

bool NotificationList::SetNotificationImage(const std::string& notification_id,
                                            const gfx::Image& image) {
  auto iter = GetNotification(notification_id);
  if (iter == notifications_.end())
    return false;
  (*iter)->set_image(image);
  return true;
}

bool NotificationList::SetNotificationButtonIcon(
    const std::string& notification_id, int button_index,
    const gfx::Image& image) {
  auto iter = GetNotification(notification_id);
  if (iter == notifications_.end())
    return false;
  (*iter)->SetButtonIcon(button_index, image);
  return true;
}

bool NotificationList::HasNotificationOfType(const std::string& id,
                                             const NotificationType type) {
  auto iter = GetNotification(id);
  if (iter == notifications_.end())
    return false;

  return (*iter)->type() == type;
}

bool NotificationList::HasPopupNotifications(
    const NotificationBlockers& blockers) {
  for (const auto& notification : notifications_) {
    if (notification->priority() < DEFAULT_PRIORITY)
      break;
    if (!ShouldShowNotificationAsPopup(*notification.get(), blockers))
      continue;
    if (!notification->shown_as_popup())
      return true;
  }
  return false;
}

NotificationList::PopupNotifications NotificationList::GetPopupNotifications(
    const NotificationBlockers& blockers,
    std::list<std::string>* blocked_ids) {
  PopupNotifications result;
  size_t default_priority_popup_count = 0;

  // Collect notifications that should be shown as popups. Start from oldest.
  for (auto iter = notifications_.rbegin(); iter != notifications_.rend();
       iter++) {
    Notification* notification = iter->get();
    if (notification->shown_as_popup())
      continue;

    // No popups for LOW/MIN priority.
    if (notification->priority() < DEFAULT_PRIORITY)
      continue;

    if (!ShouldShowNotificationAsPopup(*notification, blockers)) {
      if (blocked_ids)
        blocked_ids->push_back(notification->id());
      continue;
    }

    // Checking limits. No limits for HIGH/MAX priority. DEFAULT priority
    // will return at most kMaxVisiblePopupNotifications entries. If the
    // popup entries are more, older entries are used. see crbug.com/165768
    if (notification->priority() == DEFAULT_PRIORITY &&
        default_priority_popup_count++ >= kMaxVisiblePopupNotifications) {
      continue;
    }

    result.insert(notification);
  }
  return result;
}

void NotificationList::MarkSinglePopupAsShown(
    const std::string& id, bool mark_notification_as_read) {
  auto iter = GetNotification(id);
  DCHECK(iter != notifications_.end());

  if ((*iter)->shown_as_popup())
    return;

  // System notification is marked as shown only when marked as read.
  if ((*iter)->priority() != SYSTEM_PRIORITY || mark_notification_as_read)
    (*iter)->set_shown_as_popup(true);

  // The popup notification is already marked as read when it's displayed.
  // Set the is_read() back to false if necessary.
  if (!mark_notification_as_read)
    (*iter)->set_is_read(false);
}

void NotificationList::MarkSinglePopupAsDisplayed(const std::string& id) {
  auto iter = GetNotification(id);
  if (iter == notifications_.end())
    return;

  if ((*iter)->shown_as_popup())
    return;

  if (!(*iter)->IsRead())
    (*iter)->set_is_read(true);
}

NotificationDelegate* NotificationList::GetNotificationDelegate(
    const std::string& id) {
  auto iter = GetNotification(id);
  if (iter == notifications_.end())
    return nullptr;
  return (*iter)->delegate();
}

void NotificationList::SetQuietMode(bool quiet_mode) {
  quiet_mode_ = quiet_mode;
  if (quiet_mode_) {
    for (auto& notification : notifications_)
      notification->set_shown_as_popup(true);
  }
}

Notification* NotificationList::GetNotificationById(const std::string& id) {
  auto iter = GetNotification(id);
  if (iter != notifications_.end())
    return iter->get();
  return nullptr;
}

NotificationList::Notifications NotificationList::GetVisibleNotifications(
    const NotificationBlockers& blockers) const {
  Notifications result;
  for (const auto& notification : notifications_) {
    bool should_show = true;
    for (size_t i = 0; i < blockers.size(); ++i) {
      if (!blockers[i]->ShouldShowNotification(*notification.get())) {
        should_show = false;
        break;
      }
    }
    if (should_show)
      result.insert(notification.get());
  }

  return result;
}

size_t NotificationList::NotificationCount(
    const NotificationBlockers& blockers) const {
  return GetVisibleNotifications(blockers).size();
}

size_t NotificationList::UnreadCount(
    const NotificationBlockers& blockers) const {
  Notifications notifications = GetVisibleNotifications(blockers);
  size_t unread_count = 0;
  for (Notifications::const_iterator iter = notifications.begin();
       iter != notifications.end(); ++iter) {
    if (!(*iter)->IsRead())
      ++unread_count;
  }
  return unread_count;
}

NotificationList::OwnedNotifications::iterator
NotificationList::GetNotification(const std::string& id) {
  for (auto iter = notifications_.begin(); iter != notifications_.end();
       ++iter) {
    if ((*iter)->id() == id)
      return iter;
  }
  return notifications_.end();
}

void NotificationList::EraseNotification(OwnedNotifications::iterator iter) {
  notifications_.erase(iter);
}

void NotificationList::PushNotification(
    std::unique_ptr<Notification> notification) {
  // Ensure that notification.id is unique by erasing any existing
  // notification with the same id (shouldn't normally happen).
  auto iter = GetNotification(notification->id());
  bool state_inherited = false;
  if (iter != notifications_.end()) {
    notification->CopyState(iter->get());
    state_inherited = true;
    EraseNotification(iter);
  }
  // Add the notification to the the list and mark it unread and unshown.
  if (!state_inherited) {
    // TODO(mukai): needs to distinguish if a notification is dismissed by
    // the quiet mode or user operation.
    notification->set_is_read(false);
    notification->set_shown_as_popup(message_center_->IsMessageCenterVisible()
                                     || quiet_mode_
                                     || notification->shown_as_popup());
  }
  // Take ownership. The notification can only be removed from the list
  // in EraseNotification(), which will delete it.
  notifications_.insert(std::move(notification));
}

}  // namespace message_center
