// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_H_
#define UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_H_

#include <stddef.h>

#include "base/macros.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"

namespace message_center {

// MessageCenter implementation of doing nothing. Useful for tests.
class FakeMessageCenter : public MessageCenter {
 public:
  FakeMessageCenter();
  ~FakeMessageCenter() override;

  // Overridden from FakeMessageCenter.
  void AddObserver(MessageCenterObserver* observer) override;
  void RemoveObserver(MessageCenterObserver* observer) override;
  void AddNotificationBlocker(NotificationBlocker* blocker) override;
  void RemoveNotificationBlocker(NotificationBlocker* blocker) override;
  size_t NotificationCount() const override;
  size_t UnreadNotificationCount() const override;
  bool HasPopupNotifications() const override;
  bool IsQuietMode() const override;
  bool IsLockedState() const override;
  bool HasClickedListener(const std::string& id) override;
  message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override;
  const NotificationList::Notifications& GetVisibleNotifications() override;
  NotificationList::PopupNotifications GetPopupNotifications() override;
  void AddNotification(std::unique_ptr<Notification> notification) override;
  void UpdateNotification(
      const std::string& old_id,
      std::unique_ptr<Notification> new_notification) override;

  void RemoveNotification(const std::string& id, bool by_user) override;
  void RemoveAllNotifications(bool by_user, RemoveType type) override;
  void SetNotificationIcon(const std::string& notification_id,
                           const gfx::Image& image) override;

  void SetNotificationImage(const std::string& notification_id,
                            const gfx::Image& image) override;

  void SetNotificationButtonIcon(const std::string& notification_id,
                                 int button_index,
                                 const gfx::Image& image) override;
  void DisableNotificationsByNotifier(const NotifierId& notifier_id) override;
  void ClickOnNotification(const std::string& id) override;
  void ClickOnNotificationButton(const std::string& id,
                                 int button_index) override;
  void ClickOnSettingsButton(const std::string& id) override;
  void MarkSinglePopupAsShown(const std::string& id,
                              bool mark_notification_as_read) override;
  void DisplayedNotification(const std::string& id,
                             const DisplaySource source) override;
  void SetNotifierSettingsProvider(NotifierSettingsProvider* provider) override;
  NotifierSettingsProvider* GetNotifierSettingsProvider() override;
  void SetQuietMode(bool in_quiet_mode) override;
  void SetLockedState(bool locked) override;
  void EnterQuietModeWithExpire(const base::TimeDelta& expires_in) override;
  void SetVisibility(Visibility visible) override;
  bool IsMessageCenterVisible() const override;
  void RestartPopupTimers() override;
  void PausePopupTimers() override;

 protected:
  void DisableTimersForTest() override;
  void EnableChangeQueueForTest(bool enabled) override;

 private:
  const NotificationList::Notifications empty_notifications_;

  DISALLOW_COPY_AND_ASSIGN(FakeMessageCenter);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_H_
