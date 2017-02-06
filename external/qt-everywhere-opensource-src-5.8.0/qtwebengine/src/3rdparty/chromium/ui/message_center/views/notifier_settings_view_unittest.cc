// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_notifier_settings_provider.h"
#include "ui/message_center/views/notifier_settings_view.h"
#include "ui/views/test/views_test_base.h"

namespace message_center {

namespace {

Notifier* NewNotifier(const std::string& id,
                      const std::string& title,
                      bool enabled) {
  NotifierId notifier_id(NotifierId::APPLICATION, id);
  return new Notifier(notifier_id, base::UTF8ToUTF16(title), enabled);
}

// A class used by NotifierSettingsView to integrate with a setting system
// for the clients of this module.
class TestingNotifierSettingsProvider
    : public FakeNotifierSettingsProvider {
 public:
  TestingNotifierSettingsProvider(const std::vector<Notifier*>& notifiers,
                                  const NotifierId& settings_handler_id)
      : FakeNotifierSettingsProvider(notifiers),
        settings_handler_id_(settings_handler_id),
        request_count_(0u) {}
  ~TestingNotifierSettingsProvider() override {}

  bool NotifierHasAdvancedSettings(
      const NotifierId& notifier_id) const override {
    return notifier_id == settings_handler_id_;
  }

  void OnNotifierAdvancedSettingsRequested(
      const NotifierId& notifier_id,
      const std::string* notification_id) override {
    request_count_++;
    last_notifier_id_settings_requested_.reset(new NotifierId(notifier_id));
  }

  size_t request_count() const { return request_count_; }
  const NotifierId* last_requested_notifier_id() const {
    return last_notifier_id_settings_requested_.get();
  }

 private:
  NotifierId settings_handler_id_;
  size_t request_count_;
  std::unique_ptr<NotifierId> last_notifier_id_settings_requested_;
};

}  // namespace

class NotifierSettingsViewTest : public views::ViewsTestBase {
 public:
  NotifierSettingsViewTest();
  ~NotifierSettingsViewTest() override;

  void SetUp() override;
  void TearDown() override;

  NotifierSettingsView* GetView() const;
  TestingNotifierSettingsProvider* settings_provider() const {
    return settings_provider_.get();
  }

 private:
  std::unique_ptr<TestingNotifierSettingsProvider> settings_provider_;
  std::unique_ptr<NotifierSettingsView> notifier_settings_view_;

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsViewTest);
};

NotifierSettingsViewTest::NotifierSettingsViewTest() {}

NotifierSettingsViewTest::~NotifierSettingsViewTest() {}

void NotifierSettingsViewTest::SetUp() {
  views::ViewsTestBase::SetUp();
  std::vector<Notifier*> notifiers;
  notifiers.push_back(NewNotifier("id", "title", /*enabled=*/true));
  notifiers.push_back(NewNotifier("id2", "other title", /*enabled=*/false));
  settings_provider_.reset(new TestingNotifierSettingsProvider(
      notifiers, NotifierId(NotifierId::APPLICATION, "id")));
  notifier_settings_view_.reset(
      new NotifierSettingsView(settings_provider_.get()));
}

void NotifierSettingsViewTest::TearDown() {
  notifier_settings_view_.reset();
  settings_provider_.reset();
  views::ViewsTestBase::TearDown();
}

NotifierSettingsView* NotifierSettingsViewTest::GetView() const {
  return notifier_settings_view_.get();
}

TEST_F(NotifierSettingsViewTest, TestLearnMoreButton) {
  const std::set<NotifierSettingsView::NotifierButton*> buttons =
      GetView()->buttons_;
  EXPECT_EQ(2u, buttons.size());
  size_t number_of_settings_buttons = 0;
  std::set<NotifierSettingsView::NotifierButton*>::iterator iter =
      buttons.begin();
  for (; iter != buttons.end(); ++iter) {
    if ((*iter)->has_learn_more()) {
      ++number_of_settings_buttons;
      (*iter)->SendLearnMorePressedForTest();
    }
  }

  EXPECT_EQ(1u, number_of_settings_buttons);
  EXPECT_EQ(1u, settings_provider()->request_count());
  const NotifierId* last_settings_button_id =
      settings_provider()->last_requested_notifier_id();
  ASSERT_FALSE(last_settings_button_id == NULL);
  EXPECT_EQ(NotifierId(NotifierId::APPLICATION, "id"),
            *last_settings_button_id);
}

}  // namespace message_center
