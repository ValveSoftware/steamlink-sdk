// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_H_
#define UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_H_

#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"

namespace message_center {

class NotificationDelegate;

// MessageCenter implementation of doing nothing. Useful for tests.
class FakeMessageCenter : public MessageCenter {
 public:
  FakeMessageCenter();
  virtual ~FakeMessageCenter();

  // Overridden from FakeMessageCenter.
  virtual void AddObserver(MessageCenterObserver* observer) OVERRIDE;
  virtual void RemoveObserver(MessageCenterObserver* observer) OVERRIDE;
  virtual void AddNotificationBlocker(NotificationBlocker* blocker) OVERRIDE;
  virtual void RemoveNotificationBlocker(NotificationBlocker* blocker) OVERRIDE;
  virtual size_t NotificationCount() const OVERRIDE;
  virtual size_t UnreadNotificationCount() const OVERRIDE;
  virtual bool HasPopupNotifications() const OVERRIDE;
  virtual bool IsQuietMode() const OVERRIDE;
  virtual bool HasClickedListener(const std::string& id) OVERRIDE;
  virtual message_center::Notification* FindVisibleNotificationById(
      const std::string& id) OVERRIDE;
  virtual const NotificationList::Notifications& GetVisibleNotifications()
      OVERRIDE;
  virtual NotificationList::PopupNotifications GetPopupNotifications() OVERRIDE;
  virtual void AddNotification(scoped_ptr<Notification> notification) OVERRIDE;
  virtual void UpdateNotification(const std::string& old_id,
                                  scoped_ptr<Notification> new_notification)
      OVERRIDE;

  virtual void RemoveNotification(const std::string& id, bool by_user) OVERRIDE;
  virtual void RemoveAllNotifications(bool by_user) OVERRIDE;
  virtual void RemoveAllVisibleNotifications(bool by_user) OVERRIDE;
  virtual void SetNotificationIcon(const std::string& notification_id,
                                   const gfx::Image& image) OVERRIDE;

  virtual void SetNotificationImage(const std::string& notification_id,
                                    const gfx::Image& image) OVERRIDE;

  virtual void SetNotificationButtonIcon(const std::string& notification_id,
                                         int button_index,
                                         const gfx::Image& image) OVERRIDE;
  virtual void DisableNotificationsByNotifier(
      const NotifierId& notifier_id) OVERRIDE;
  virtual void ClickOnNotification(const std::string& id) OVERRIDE;
  virtual void ClickOnNotificationButton(const std::string& id,
                                         int button_index) OVERRIDE;
  virtual void MarkSinglePopupAsShown(const std::string& id,
                                      bool mark_notification_as_read) OVERRIDE;
  virtual void DisplayedNotification(
      const std::string& id,
      const DisplaySource source) OVERRIDE;
  virtual void SetNotifierSettingsProvider(
      NotifierSettingsProvider* provider) OVERRIDE;
  virtual NotifierSettingsProvider* GetNotifierSettingsProvider() OVERRIDE;
  virtual void SetQuietMode(bool in_quiet_mode) OVERRIDE;
  virtual void EnterQuietModeWithExpire(
      const base::TimeDelta& expires_in) OVERRIDE;
  virtual void SetVisibility(Visibility visible) OVERRIDE;
  virtual bool IsMessageCenterVisible() const OVERRIDE;
  virtual void RestartPopupTimers() OVERRIDE;
  virtual void PausePopupTimers() OVERRIDE;

 protected:
  virtual void DisableTimersForTest() OVERRIDE;

 private:
  const NotificationList::Notifications empty_notifications_;

  DISALLOW_COPY_AND_ASSIGN(FakeMessageCenter);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_H_
