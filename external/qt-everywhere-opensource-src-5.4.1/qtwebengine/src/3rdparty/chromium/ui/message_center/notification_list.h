// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_LIST_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_LIST_H_

#include <list>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/notification_types.h"

namespace base {
class DictionaryValue;
class TimeDelta;
}

namespace gfx {
class Image;
}

namespace message_center {

namespace test {
class NotificationListTest;
}

class Notification;
class NotificationDelegate;
struct NotifierId;

// Comparers used to auto-sort the lists of Notifications.
struct MESSAGE_CENTER_EXPORT ComparePriorityTimestampSerial {
  bool operator()(Notification* n1, Notification* n2);
};

struct CompareTimestampSerial {
  bool operator()(Notification* n1, Notification* n2);
};

// A helper class to manage the list of notifications.
class MESSAGE_CENTER_EXPORT NotificationList {
 public:
  // Auto-sorted set. Matches the order in which Notifications are shown in
  // Notification Center.
  typedef std::set<Notification*, ComparePriorityTimestampSerial> Notifications;

  // Auto-sorted set used to return the Notifications to be shown as popup
  // toasts.
  typedef std::set<Notification*, CompareTimestampSerial> PopupNotifications;

  explicit NotificationList();
  virtual ~NotificationList();

  // Affects whether or not a message has been "read". Collects the set of
  // ids whose state have changed and set to |udpated_ids|. NULL if updated
  // ids don't matter.
  void SetMessageCenterVisible(bool visible,
                               std::set<std::string>* updated_ids);

  void AddNotification(scoped_ptr<Notification> notification);

  void UpdateNotificationMessage(const std::string& old_id,
                                 scoped_ptr<Notification> new_notification);

  void RemoveNotification(const std::string& id);

  Notifications GetNotificationsByNotifierId(const NotifierId& notifier_id);

  // Returns true if the notification exists and was updated.
  bool SetNotificationIcon(const std::string& notification_id,
                           const gfx::Image& image);

  // Returns true if the notification exists and was updated.
  bool SetNotificationImage(const std::string& notification_id,
                            const gfx::Image& image);

  // Returns true if the notification and button exist and were updated.
  bool SetNotificationButtonIcon(const std::string& notification_id,
                                 int button_index,
                                 const gfx::Image& image);

  // Returns true if |id| matches a notification in the list and that
  // notification's type matches the given type.
  bool HasNotificationOfType(const std::string& id,
                             const NotificationType type);

  // Returns false if the first notification has been shown as a popup (which
  // means that all notifications have been shown).
  bool HasPopupNotifications(const NotificationBlockers& blockers);

  // Returns the recent notifications of the priority higher then LOW,
  // that have not been shown as a popup. kMaxVisiblePopupNotifications are
  // used to limit the number of notifications for the DEFAULT priority.
  // It also stores the list of notification ids which is blocked by |blockers|
  // to |blocked_ids|. |blocked_ids| can be NULL if the caller doesn't care
  // which notifications are blocked.
  PopupNotifications GetPopupNotifications(
      const NotificationBlockers& blockers,
      std::list<std::string>* blocked_ids);

  // Marks a specific popup item as shown. Set |mark_notification_as_read| to
  // true in case marking the notification as read too.
  void MarkSinglePopupAsShown(const std::string& id,
                              bool mark_notification_as_read);

  // Marks a specific popup item as displayed.
  void MarkSinglePopupAsDisplayed(const std::string& id);

  NotificationDelegate* GetNotificationDelegate(const std::string& id);

  bool quiet_mode() const { return quiet_mode_; }

  // Sets the current quiet mode status to |quiet_mode|.
  void SetQuietMode(bool quiet_mode);

  // Sets the current quiet mode to true. The quiet mode will expire in the
  // specified time-delta from now.
  void EnterQuietModeWithExpire(const base::TimeDelta& expires_in);

  // Returns the notification with the corresponding id. If not found, returns
  // NULL. Notification instance is owned by this list.
  Notification* GetNotificationById(const std::string& id);

  // Returns all visible notifications, in a (priority-timestamp) order.
  // Suitable for rendering notifications in a MessageCenter.
  Notifications GetVisibleNotifications(
      const NotificationBlockers& blockers) const;
  size_t NotificationCount(const NotificationBlockers& blockers) const;
  size_t UnreadCount(const NotificationBlockers& blockers) const;

  bool is_message_center_visible() const { return message_center_visible_; }

 private:
  friend class NotificationListTest;
  FRIEND_TEST_ALL_PREFIXES(NotificationListTest,
                           TestPushingShownNotification);

  // Iterates through the list and returns the first notification matching |id|.
  Notifications::iterator GetNotification(const std::string& id);

  void EraseNotification(Notifications::iterator iter);

  void PushNotification(scoped_ptr<Notification> notification);

  Notifications notifications_;
  bool message_center_visible_;
  bool quiet_mode_;

  DISALLOW_COPY_AND_ASSIGN(NotificationList);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_NOTIFICATION_LIST_H_
