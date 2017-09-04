// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_FAKE_NOTIFIER_SETTINGS_PROVIDER_H_
#define UI_MESSAGE_CENTER_FAKE_NOTIFIER_SETTINGS_PROVIDER_H_

#include <stddef.h>

#include <memory>

#include "ui/message_center/notifier_settings.h"

namespace message_center {

// A NotifierSettingsProvider that returns a configurable, fixed set of
// notifiers and records which callbacks were called. For use in tests.
class FakeNotifierSettingsProvider : public NotifierSettingsProvider {
 public:
  FakeNotifierSettingsProvider();
  explicit FakeNotifierSettingsProvider(
      const std::vector<Notifier*>& notifiers);
  ~FakeNotifierSettingsProvider() override;

  size_t GetNotifierGroupCount() const override;
  const NotifierGroup& GetNotifierGroupAt(size_t index) const override;
  bool IsNotifierGroupActiveAt(size_t index) const override;
  void SwitchToNotifierGroup(size_t index) override;
  const NotifierGroup& GetActiveNotifierGroup() const override;

  void GetNotifierList(std::vector<Notifier*>* notifiers) override;

  void SetNotifierEnabled(const Notifier& notifier, bool enabled) override;

  void OnNotifierSettingsClosing() override;
  bool NotifierHasAdvancedSettings(
      const NotifierId& notifier_id) const override;
  void OnNotifierAdvancedSettingsRequested(
      const NotifierId& notifier_id,
      const std::string* notification_id) override;
  void AddObserver(NotifierSettingsObserver* observer) override;
  void RemoveObserver(NotifierSettingsObserver* observer) override;

  bool WasEnabled(const Notifier& notifier);
  int closed_called_count();

  void AddGroup(NotifierGroup* group, const std::vector<Notifier*>& notifiers);

  // For testing, this method can be used to specify a notifier that has a learn
  // more button. This class only saves the last one that was set.
  void SetNotifierHasAdvancedSettings(const NotifierId& notifier_id);
  size_t settings_requested_count() const;

 private:
  struct NotifierGroupItem {
    NotifierGroup* group;
    std::vector<Notifier*> notifiers;

    NotifierGroupItem();
    NotifierGroupItem(const NotifierGroupItem& other);
    ~NotifierGroupItem();
  };

  std::map<const Notifier*, bool> enabled_;
  std::vector<NotifierGroupItem> items_;
  int closed_called_count_;
  size_t active_item_index_;
  std::unique_ptr<NotifierId> notifier_id_with_settings_handler_;
  size_t notifier_settings_requested_count_;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_FAKE_NOTIFIER_SETTINGS_PROVIDER_H_
