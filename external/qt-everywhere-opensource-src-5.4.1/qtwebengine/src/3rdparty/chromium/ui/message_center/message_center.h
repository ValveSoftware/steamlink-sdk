// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_list.h"

namespace base {
class DictionaryValue;
}

// Interface to manage the NotificationList. The client (e.g. Chrome) calls
// [Add|Remove|Update]Notification to create and update notifications in the
// list. It also sends those changes to its observers when a notification
// is shown, closed, or clicked on.
//
// MessageCenter is agnostic of profiles; it uses the string returned by
// message_center::Notification::id() to uniquely identify a notification. It is
// the caller's responsibility to formulate the id so that 2 different
// notification should have different ids. For example, if the caller supports
// multiple profiles, then caller should encode both profile characteristics and
// notification front end's notification id into a new id and set it into the
// notification instance before passing that in. Consequently the id passed to
// observers will be this unique id, which can be used with MessageCenter
// interface but probably not higher level interfaces.

namespace message_center {

namespace test {
class MessagePopupCollectionTest;
}

class MessageCenterObserver;
class NotificationBlocker;
class NotifierSettingsProvider;

class MESSAGE_CENTER_EXPORT MessageCenter {
 public:
  // Creates the global message center object.
  static void Initialize();

  // Returns the global message center object. Returns NULL if Initialize is not
  // called.
  static MessageCenter* Get();

  // Destroys the global message_center object.
  static void Shutdown();

  // Management of the observer list.
  virtual void AddObserver(MessageCenterObserver* observer) = 0;
  virtual void RemoveObserver(MessageCenterObserver* observer) = 0;

  // Queries of current notification list status.
  virtual size_t NotificationCount() const = 0;
  virtual size_t UnreadNotificationCount() const = 0;
  virtual bool HasPopupNotifications() const = 0;
  virtual bool IsQuietMode() const = 0;
  virtual bool HasClickedListener(const std::string& id) = 0;

  // Find the notification with the corresponding id. Returns NULL if not found.
  // The returned instance is owned by the message center.
  virtual message_center::Notification* FindVisibleNotificationById(
      const std::string& id) = 0;

  // Gets all notifications to be shown to the user in the message center.  Note
  // that queued changes due to the message center being open are not reflected
  // in this list.
  virtual const NotificationList::Notifications& GetVisibleNotifications() = 0;

  // Gets all notifications being shown as popups.  This should not be affected
  // by the change queue since notifications are not held up while the state is
  // VISIBILITY_TRANSIENT or VISIBILITY_SETTINGS.
  virtual NotificationList::PopupNotifications GetPopupNotifications() = 0;

  // Management of NotificationBlockers.
  virtual void AddNotificationBlocker(NotificationBlocker* blocker) = 0;
  virtual void RemoveNotificationBlocker(NotificationBlocker* blocker) = 0;

  // Basic operations of notification: add/remove/update.

  // Adds a new notification.
  virtual void AddNotification(scoped_ptr<Notification> notification) = 0;

  // Updates an existing notification with id = old_id and set its id to new_id.
  virtual void UpdateNotification(
      const std::string& old_id,
      scoped_ptr<Notification> new_notification) = 0;

  // Removes an existing notification.
  virtual void RemoveNotification(const std::string& id, bool by_user) = 0;
  virtual void RemoveAllNotifications(bool by_user) = 0;
  virtual void RemoveAllVisibleNotifications(bool by_user) = 0;

  // Sets the icon image. Icon appears at the top-left of the notification.
  virtual void SetNotificationIcon(const std::string& notification_id,
                                   const gfx::Image& image) = 0;

  // Sets the large image for the notifications of type == TYPE_IMAGE. Specified
  // image will appear below of the notification.
  virtual void SetNotificationImage(const std::string& notification_id,
                                    const gfx::Image& image) = 0;

  // Sets the image for the icon of the specific action button.
  virtual void SetNotificationButtonIcon(const std::string& notification_id,
                                         int button_index,
                                         const gfx::Image& image) = 0;

  // Operations happening especially from GUIs: click, disable, and settings.
  // Searches through the notifications and disables any that match the
  // extension id given.
  virtual void DisableNotificationsByNotifier(
      const NotifierId& notifier_id) = 0;

  // This should be called by UI classes when a notification is clicked to
  // trigger the notification's delegate callback and also update the message
  // center observers.
  virtual void ClickOnNotification(const std::string& id) = 0;

  // This should be called by UI classes when a notification button is clicked
  // to trigger the notification's delegate callback and also update the message
  // center observers.
  virtual void ClickOnNotificationButton(const std::string& id,
                                         int button_index) = 0;

  // This should be called by UI classes after a visible notification popup
  // closes, indicating that the notification has been shown to the user.
  // |mark_notification_as_read|, if false, will unset the read bit on a
  // notification, increasing the unread count of the center.
  virtual void MarkSinglePopupAsShown(const std::string& id,
                                      bool mark_notification_as_read) = 0;

  // This should be called by UI classes when a notification is first displayed
  // to the user, in order to decrement the unread_count for the tray, and to
  // notify observers that the notification is visible.
  virtual void DisplayedNotification(
      const std::string& id,
      const DisplaySource source) = 0;

  // Setter/getter of notifier settings provider. This will be a weak reference.
  // This should be set at the initialization process. The getter may return
  // NULL for tests.
  virtual void SetNotifierSettingsProvider(
      NotifierSettingsProvider* provider) = 0;
  virtual NotifierSettingsProvider* GetNotifierSettingsProvider() = 0;

  // This can be called to change the quiet mode state (without a timeout).
  virtual void SetQuietMode(bool in_quiet_mode) = 0;

  // Temporarily enables quiet mode for |expires_in| time.
  virtual void EnterQuietModeWithExpire(const base::TimeDelta& expires_in) = 0;

  // Informs the notification list whether the message center is visible.
  // This affects whether or not a message has been "read".
  virtual void SetVisibility(Visibility visible) = 0;

  // Allows querying the visibility of the center.
  virtual bool IsMessageCenterVisible() const = 0;

  // UI classes should call this when there is cause to leave popups visible for
  // longer than the default (for example, when the mouse hovers over a popup).
  virtual void PausePopupTimers() = 0;

  // UI classes should call this when the popup timers should restart (for
  // example, after the mouse leaves the popup.)
  virtual void RestartPopupTimers() = 0;

 protected:
  friend class TrayViewControllerTest;
  friend class test::MessagePopupCollectionTest;
  virtual void DisableTimersForTest() = 0;

  MessageCenter();
  virtual ~MessageCenter();

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageCenter);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_H_
