// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/notification_list.h"

namespace message_center {

FakeMessageCenter::FakeMessageCenter() {
}

FakeMessageCenter::~FakeMessageCenter() {
}

void FakeMessageCenter::AddObserver(MessageCenterObserver* observer) {
}

void FakeMessageCenter::RemoveObserver(MessageCenterObserver* observer) {
}

void FakeMessageCenter::AddNotificationBlocker(NotificationBlocker* blocker) {
}

void FakeMessageCenter::RemoveNotificationBlocker(
    NotificationBlocker* blocker) {
}

size_t FakeMessageCenter::NotificationCount() const {
  return 0u;
}

size_t FakeMessageCenter::UnreadNotificationCount() const {
  return 0u;
}

bool FakeMessageCenter::HasPopupNotifications() const {
  return false;
}

bool FakeMessageCenter::IsQuietMode() const {
  return false;
}

bool FakeMessageCenter::IsLockedState() const {
  return false;
}

bool FakeMessageCenter::HasClickedListener(const std::string& id) {
  return false;
}

message_center::Notification* FakeMessageCenter::FindVisibleNotificationById(
    const std::string& id) {
  return NULL;
}

const NotificationList::Notifications&
FakeMessageCenter::GetVisibleNotifications() {
  return empty_notifications_;
}

NotificationList::PopupNotifications
    FakeMessageCenter::GetPopupNotifications() {
  return NotificationList::PopupNotifications();
}

void FakeMessageCenter::AddNotification(
    std::unique_ptr<Notification> notification) {}

void FakeMessageCenter::UpdateNotification(
    const std::string& old_id,
    std::unique_ptr<Notification> new_notification) {}

void FakeMessageCenter::RemoveNotification(const std::string& id,
                                           bool by_user) {
}

void FakeMessageCenter::RemoveAllNotifications(bool by_user, RemoveType type) {}

void FakeMessageCenter::SetNotificationIcon(const std::string& notification_id,
                                            const gfx::Image& image) {
}

void FakeMessageCenter::SetNotificationImage(const std::string& notification_id,
                                             const gfx::Image& image) {
}

void FakeMessageCenter::SetNotificationButtonIcon(
    const std::string& notification_id,
    int button_index,
    const gfx::Image& image) {
}

void FakeMessageCenter::DisableNotificationsByNotifier(
    const NotifierId& notifier_id) {
}

void FakeMessageCenter::ClickOnNotification(const std::string& id) {
}

void FakeMessageCenter::ClickOnNotificationButton(const std::string& id,
                                                  int button_index) {
}

void FakeMessageCenter::ClickOnSettingsButton(const std::string& id) {}

void FakeMessageCenter::MarkSinglePopupAsShown(const std::string& id,
                                               bool mark_notification_as_read) {
}

void FakeMessageCenter::DisplayedNotification(
    const std::string& id,
    const DisplaySource source) {
}

void FakeMessageCenter::SetNotifierSettingsProvider(
    NotifierSettingsProvider* provider) {
}

NotifierSettingsProvider* FakeMessageCenter::GetNotifierSettingsProvider() {
  return NULL;
}

void FakeMessageCenter::SetQuietMode(bool in_quiet_mode) {
}

void FakeMessageCenter::SetLockedState(bool locked) {}

void FakeMessageCenter::EnterQuietModeWithExpire(
    const base::TimeDelta& expires_in) {
}

void FakeMessageCenter::SetVisibility(Visibility visible) {
}

bool FakeMessageCenter::IsMessageCenterVisible() const {
  return false;
}

void FakeMessageCenter::RestartPopupTimers() {}

void FakeMessageCenter::PausePopupTimers() {}

void FakeMessageCenter::DisableTimersForTest() {}

void FakeMessageCenter::EnableChangeQueueForTest(bool enabled) {}

}  // namespace message_center
