// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/settings_controller.h"

#include "base/strings/utf_string_conversions.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"
#include "ui/message_center/fake_notifier_settings_provider.h"

@implementation MCSettingsController (TestingInterface)
- (NSInteger)profileSwitcherListCount {
  // Subtract the dummy item.
  return [self groupDropDownButton]
                 ? [[self groupDropDownButton] numberOfItems] - 1
                 : 0;
}

- (NSUInteger)scrollViewItemCount {
  return [[[[self scrollView] documentView] subviews] count];
}

- (MCSettingsEntryView*)bottomMostButton {
  // The checkboxes are created bottom-to-top, so the first object is the
  // bottom-most.
  return [[[[self scrollView] documentView] subviews] objectAtIndex:0];
}
@end

namespace message_center {

using ui::CocoaTest;

namespace {

NotifierGroup* NewGroup(const std::string& name,
                        const std::string& login_info) {
  return new NotifierGroup(gfx::Image(),
                           base::UTF8ToUTF16(name),
                           base::UTF8ToUTF16(login_info));
}

Notifier* NewNotifier(const std::string& id,
                      const std::string& title,
                      bool enabled) {
  NotifierId notifier_id(NotifierId::APPLICATION, id);
  return new Notifier(notifier_id, base::UTF8ToUTF16(title), enabled);
}

}  // namespace

TEST_F(CocoaTest, Basic) {
  // Notifiers are owned by settings controller.
  std::vector<Notifier*> notifiers;
  notifiers.push_back(NewNotifier("id", "title", /*enabled=*/ true));
  notifiers.push_back(NewNotifier("id2", "other title", /*enabled=*/ false));

  FakeNotifierSettingsProvider provider(notifiers);

  base::scoped_nsobject<MCSettingsController> controller(
      [[MCSettingsController alloc] initWithProvider:&provider
                                  trayViewController:nil]);
  [controller view];

  EXPECT_EQ(notifiers.size(), [controller scrollViewItemCount]);
}

TEST_F(CocoaTest, Toggle) {
  // Notifiers are owned by settings controller.
  std::vector<Notifier*> notifiers;
  notifiers.push_back(NewNotifier("id", "title", /*enabled=*/ true));
  notifiers.push_back(NewNotifier("id2", "other title", /*enabled=*/ false));

  FakeNotifierSettingsProvider provider(notifiers);

  base::scoped_nsobject<MCSettingsController> controller(
      [[MCSettingsController alloc] initWithProvider:&provider
                                  trayViewController:nil]);
  [controller view];

  MCSettingsEntryView* toggleView = [controller bottomMostButton];
  NSButton* toggleSecond = [toggleView checkbox];

  [toggleSecond performClick:nil];
  EXPECT_TRUE(provider.WasEnabled(*notifiers.back()));

  [toggleSecond performClick:nil];
  EXPECT_FALSE(provider.WasEnabled(*notifiers.back()));

  EXPECT_EQ(0, provider.closed_called_count());
  controller.reset();
  EXPECT_EQ(1, provider.closed_called_count());
}

TEST_F(CocoaTest, SingleProfile) {
  // Notifiers are owned by settings controller.
  std::vector<Notifier*> notifiers;
  notifiers.push_back(NewNotifier("id", "title", /*enabled=*/ true));
  notifiers.push_back(NewNotifier("id2", "other title", /*enabled=*/ false));

  FakeNotifierSettingsProvider provider(notifiers);

  base::scoped_nsobject<MCSettingsController> controller(
      [[MCSettingsController alloc] initWithProvider:&provider
                                  trayViewController:nil]);
  [controller view];

  EXPECT_EQ(0, [controller profileSwitcherListCount]);
}

TEST_F(CocoaTest, MultiProfile) {
  FakeNotifierSettingsProvider provider;
  std::vector<Notifier*> group1_notifiers;
  group1_notifiers.push_back(NewNotifier("id", "title", /*enabled=*/ true));
  group1_notifiers.push_back(NewNotifier("id2", "title2", /*enabled=*/ false));
  provider.AddGroup(NewGroup("Group1", "GroupId1"), group1_notifiers);
  std::vector<Notifier*> group2_notifiers;
  group2_notifiers.push_back(NewNotifier("id3", "title3", /*enabled=*/ true));
  group2_notifiers.push_back(NewNotifier("id4", "title4", /*enabled=*/ false));
  group2_notifiers.push_back(NewNotifier("id5", "title5", /*enabled=*/ false));
  provider.AddGroup(NewGroup("Group2", "GroupId2"), group2_notifiers);

  base::scoped_nsobject<MCSettingsController> controller(
      [[MCSettingsController alloc] initWithProvider:&provider
                                  trayViewController:nil]);
  [controller view];

  EXPECT_EQ(2, [controller profileSwitcherListCount]);
}

TEST_F(CocoaTest, LearnMoreButton) {
  std::vector<Notifier*> notifiers;
  notifiers.push_back(NewNotifier("id", "title", /*enabled=*/ true));
  notifiers.push_back(NewNotifier("id2", "title2", /*enabled=*/ false));

  FakeNotifierSettingsProvider provider(notifiers);
  EXPECT_EQ(0u, provider.settings_requested_count());
  NotifierId has_settings_handler_notifier =
      NotifierId(NotifierId::APPLICATION, "id2");
  provider.SetNotifierHasAdvancedSettings(has_settings_handler_notifier);

  base::scoped_nsobject<MCSettingsController> controller(
      [[MCSettingsController alloc] initWithProvider:&provider
                                  trayViewController:nil]);
  [controller view];

  [[controller bottomMostButton] clickLearnMore];

  EXPECT_EQ(1u, provider.settings_requested_count());
}

}  // namespace message_center
