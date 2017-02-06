// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox.h"

#include <set>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/test/combobox_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

namespace views {

using test::ComboboxTestApi;

namespace {

// A wrapper of Combobox to intercept the result of OnKeyPressed() and
// OnKeyReleased() methods.
class TestCombobox : public Combobox {
 public:
  TestCombobox(ui::ComboboxModel* model, Combobox::Style style)
      : Combobox(model, style), key_handled_(false), key_received_(false) {}

  bool OnKeyPressed(const ui::KeyEvent& e) override {
    key_received_ = true;
    key_handled_ = Combobox::OnKeyPressed(e);
    return key_handled_;
  }

  bool OnKeyReleased(const ui::KeyEvent& e) override {
    key_received_ = true;
    key_handled_ = Combobox::OnKeyReleased(e);
    return key_handled_;
  }

  bool key_handled() const { return key_handled_; }
  bool key_received() const { return key_received_; }

  void clear() {
    key_received_ = key_handled_ = false;
  }

 private:
  bool key_handled_;
  bool key_received_;

  DISALLOW_COPY_AND_ASSIGN(TestCombobox);
};

// A concrete class is needed to test the combobox.
class TestComboboxModel : public ui::ComboboxModel {
 public:
  TestComboboxModel() {}
  ~TestComboboxModel() override {}

  enum { kItemCount = 10 };

  // ui::ComboboxModel:
  int GetItemCount() const override { return item_count_; }
  base::string16 GetItemAt(int index) override {
    if (IsItemSeparatorAt(index)) {
      NOTREACHED();
      return ASCIIToUTF16("SEPARATOR");
    }
    return ASCIIToUTF16(index % 2 == 0 ? "PEANUT BUTTER" : "JELLY");
  }
  bool IsItemSeparatorAt(int index) override {
    return separators_.find(index) != separators_.end();
  }

  int GetDefaultIndex() const override {
    // Return the first index that is not a separator.
    for (int index = 0; index < kItemCount; ++index) {
      if (separators_.find(index) == separators_.end())
        return index;
    }
    NOTREACHED();
    return 0;
  }

  void SetSeparators(const std::set<int>& separators) {
    separators_ = separators;
  }

  void set_item_count(int item_count) { item_count_ = item_count; }

 private:
  std::set<int> separators_;
  int item_count_ = kItemCount;

  DISALLOW_COPY_AND_ASSIGN(TestComboboxModel);
};

// A combobox model which refers to a vector.
class VectorComboboxModel : public ui::ComboboxModel {
 public:
  explicit VectorComboboxModel(std::vector<std::string>* values)
      : values_(values) {}
  ~VectorComboboxModel() override {}

  void set_default_index(int default_index) { default_index_ = default_index; }

  // ui::ComboboxModel:
  int GetItemCount() const override { return (int)values_->size(); }
  base::string16 GetItemAt(int index) override {
    return ASCIIToUTF16(values_->at(index));
  }
  bool IsItemSeparatorAt(int index) override { return false; }
  int GetDefaultIndex() const override { return default_index_; }

 private:
  std::vector<std::string>* values_;
  int default_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(VectorComboboxModel);
};

class EvilListener : public ComboboxListener {
 public:
  EvilListener() : deleted_(false) {}
  ~EvilListener() override{};

  // ComboboxListener:
  void OnPerformAction(Combobox* combobox) override {
    delete combobox;
    deleted_ = true;
  }

  bool deleted() const { return deleted_; }

 private:
  bool deleted_;

  DISALLOW_COPY_AND_ASSIGN(EvilListener);
};

class TestComboboxListener : public views::ComboboxListener {
 public:
  TestComboboxListener() : perform_action_index_(-1), actions_performed_(0) {}
  ~TestComboboxListener() override {}

  void OnPerformAction(views::Combobox* combobox) override {
    perform_action_index_ = combobox->selected_index();
    actions_performed_++;
  }

  int perform_action_index() const {
    return perform_action_index_;
  }

  bool on_perform_action_called() const {
    return actions_performed_ > 0;
  }

  int actions_performed() const {
    return actions_performed_;
  }

 private:
  int perform_action_index_;
  int actions_performed_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestComboboxListener);
};

}  // namespace

class ComboboxTest : public ViewsTestBase {
 public:
  ComboboxTest() : widget_(NULL), combobox_(NULL) {}

  void TearDown() override {
    if (widget_)
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  void InitCombobox(const std::set<int>* separators, Combobox::Style style) {
    model_.reset(new TestComboboxModel());

    if (separators)
      model_->SetSeparators(*separators);

    ASSERT_FALSE(combobox_);
    combobox_ = new TestCombobox(model_.get(), style);
    test_api_.reset(new ComboboxTestApi(combobox_));
    combobox_->set_id(1);

    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(200, 200, 200, 200);
    widget_->Init(params);
    View* container = new View();
    widget_->SetContentsView(container);
    container->AddChildView(combobox_);

    combobox_->RequestFocus();
    combobox_->SizeToPreferredSize();
  }

 protected:
  void SendKeyEvent(ui::KeyboardCode key_code) {
    SendKeyEventWithType(key_code, ui::ET_KEY_PRESSED);
  }

  void SendKeyEventWithType(ui::KeyboardCode key_code, ui::EventType type) {
    ui::KeyEvent event(type, key_code, ui::EF_NONE);
    FocusManager* focus_manager = widget_->GetFocusManager();
    widget_->OnKeyEvent(&event);
    if (!event.handled() && focus_manager)
      focus_manager->OnKeyEvent(event);
  }

  View* GetFocusedView() {
    return widget_->GetFocusManager()->GetFocusedView();
  }

  void PerformClick(const gfx::Point& point) {
    ui::MouseEvent pressed_event = ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget_->OnMouseEvent(&pressed_event);
    ui::MouseEvent released_event = ui::MouseEvent(
        ui::ET_MOUSE_RELEASED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget_->OnMouseEvent(&released_event);
  }

  // We need widget to populate wrapper class.
  Widget* widget_;

  // |combobox_| will be allocated InitCombobox() and then owned by |widget_|.
  TestCombobox* combobox_;
  std::unique_ptr<ComboboxTestApi> test_api_;

  // Combobox does not take ownership of the model, hence it needs to be scoped.
  std::unique_ptr<TestComboboxModel> model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ComboboxTest);
};

TEST_F(ComboboxTest, KeyTest) {
  InitCombobox(nullptr, Combobox::STYLE_NORMAL);
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(combobox_->selected_index() + 1, model_->GetItemCount());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(combobox_->selected_index(), 0);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(combobox_->selected_index(), 2);
  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_EQ(combobox_->selected_index(), 2);
  SendKeyEvent(ui::VKEY_LEFT);
  EXPECT_EQ(combobox_->selected_index(), 2);
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(combobox_->selected_index(), 1);
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(combobox_->selected_index(), 0);
  SendKeyEvent(ui::VKEY_NEXT);
  EXPECT_EQ(combobox_->selected_index(), model_->GetItemCount() - 1);
}

// Check that if a combobox is disabled before it has a native wrapper, then the
// native wrapper inherits the disabled state when it gets created.
TEST_F(ComboboxTest, DisabilityTest) {
  model_.reset(new TestComboboxModel());

  ASSERT_FALSE(combobox_);
  combobox_ = new TestCombobox(model_.get(), Combobox::STYLE_NORMAL);
  combobox_->SetEnabled(false);

  widget_ = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget_->Init(params);
  View* container = new View();
  widget_->SetContentsView(container);
  container->AddChildView(combobox_);
  EXPECT_FALSE(combobox_->enabled());
}

// Verifies that we don't select a separator line in combobox when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipSeparatorSimple) {
  std::set<int> separators;
  separators.insert(2);
  InitCombobox(&separators, Combobox::STYLE_NORMAL);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->selected_index());
}

// Verifies that we never select the separator that is in the beginning of the
// combobox list when navigating through keyboard.
TEST_F(ComboboxTest, SkipSeparatorBeginning) {
  std::set<int> separators;
  separators.insert(0);
  InitCombobox(&separators, Combobox::STYLE_NORMAL);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(2, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(2, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->selected_index());
}

// Verifies that we never select the separator that is in the end of the
// combobox list when navigating through keyboard.
TEST_F(ComboboxTest, SkipSeparatorEnd) {
  std::set<int> separators;
  separators.insert(TestComboboxModel::kItemCount - 1);
  InitCombobox(&separators, Combobox::STYLE_NORMAL);
  combobox_->SetSelectedIndex(8);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(8, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(7, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(8, combobox_->selected_index());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the beginning of the combobox list when
// navigating through keyboard.
TEST_F(ComboboxTest, SkipMultipleSeparatorsAtBeginning) {
  std::set<int> separators;
  separators.insert(0);
  separators.insert(1);
  separators.insert(2);
  InitCombobox(&separators, Combobox::STYLE_NORMAL);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(4, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_NEXT);
  EXPECT_EQ(9, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(3, combobox_->selected_index());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the middle of the combobox list when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipMultipleAdjacentSeparatorsAtMiddle) {
  std::set<int> separators;
  separators.insert(4);
  separators.insert(5);
  separators.insert(6);
  InitCombobox(&separators, Combobox::STYLE_NORMAL);
  combobox_->SetSelectedIndex(3);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(7, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(3, combobox_->selected_index());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the end of the combobox list when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipMultipleSeparatorsAtEnd) {
  std::set<int> separators;
  separators.insert(7);
  separators.insert(8);
  separators.insert(9);
  InitCombobox(&separators, Combobox::STYLE_NORMAL);
  combobox_->SetSelectedIndex(6);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(6, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(5, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_NEXT);
  EXPECT_EQ(6, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(6, combobox_->selected_index());
}

TEST_F(ComboboxTest, GetTextForRowTest) {
  std::set<int> separators;
  separators.insert(0);
  separators.insert(1);
  separators.insert(9);
  InitCombobox(&separators, Combobox::STYLE_NORMAL);
  for (int i = 0; i < combobox_->GetRowCount(); ++i) {
    if (separators.count(i) != 0) {
      EXPECT_TRUE(combobox_->GetTextForRow(i).empty()) << i;
    } else {
      EXPECT_EQ(ASCIIToUTF16(i % 2 == 0 ? "PEANUT BUTTER" : "JELLY"),
                combobox_->GetTextForRow(i)) << i;
    }
  }
}

// Verifies selecting the first matching value (and returning whether found).
TEST_F(ComboboxTest, SelectValue) {
  InitCombobox(nullptr, Combobox::STYLE_NORMAL);
  ASSERT_EQ(model_->GetDefaultIndex(), combobox_->selected_index());
  EXPECT_TRUE(combobox_->SelectValue(ASCIIToUTF16("PEANUT BUTTER")));
  EXPECT_EQ(0, combobox_->selected_index());
  EXPECT_TRUE(combobox_->SelectValue(ASCIIToUTF16("JELLY")));
  EXPECT_EQ(1, combobox_->selected_index());
  EXPECT_FALSE(combobox_->SelectValue(ASCIIToUTF16("BANANAS")));
  EXPECT_EQ(1, combobox_->selected_index());
}

TEST_F(ComboboxTest, SelectValueActionStyle) {
  // With the action style, the selected index is always 0.
  InitCombobox(nullptr, Combobox::STYLE_ACTION);
  EXPECT_FALSE(combobox_->SelectValue(ASCIIToUTF16("PEANUT BUTTER")));
  EXPECT_EQ(0, combobox_->selected_index());
  EXPECT_FALSE(combobox_->SelectValue(ASCIIToUTF16("JELLY")));
  EXPECT_EQ(0, combobox_->selected_index());
  EXPECT_FALSE(combobox_->SelectValue(ASCIIToUTF16("BANANAS")));
  EXPECT_EQ(0, combobox_->selected_index());
}

TEST_F(ComboboxTest, SelectIndexActionStyle) {
  InitCombobox(nullptr, Combobox::STYLE_ACTION);

  // With the action style, the selected index is always 0.
  combobox_->SetSelectedIndex(1);
  EXPECT_EQ(0, combobox_->selected_index());
  combobox_->SetSelectedIndex(2);
  EXPECT_EQ(0, combobox_->selected_index());
  combobox_->SetSelectedIndex(3);
  EXPECT_EQ(0, combobox_->selected_index());
}

TEST_F(ComboboxTest, ListenerHandlesDelete) {
  TestComboboxModel model;

  // |combobox| will be deleted on change.
  TestCombobox* combobox = new TestCombobox(&model, Combobox::STYLE_NORMAL);
  std::unique_ptr<EvilListener> evil_listener(new EvilListener());
  combobox->set_listener(evil_listener.get());
  ASSERT_NO_FATAL_FAILURE(ComboboxTestApi(combobox).PerformActionAt(2));
  EXPECT_TRUE(evil_listener->deleted());

  // With STYLE_ACTION
  // |combobox| will be deleted on change.
  combobox = new TestCombobox(&model, Combobox::STYLE_ACTION);
  evil_listener.reset(new EvilListener());
  combobox->set_listener(evil_listener.get());
  ASSERT_NO_FATAL_FAILURE(ComboboxTestApi(combobox).PerformActionAt(2));
  EXPECT_TRUE(evil_listener->deleted());
}

TEST_F(ComboboxTest, Click) {
  InitCombobox(nullptr, Combobox::STYLE_NORMAL);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  combobox_->Layout();
  int menu_show_count = 0;
  test_api_->InstallTestMenuRunner(&menu_show_count);

  // Click the left side. The menu is shown.
  EXPECT_EQ(0, menu_show_count);
  PerformClick(gfx::Point(combobox_->x() + 1,
                          combobox_->y() + combobox_->height() / 2));
  EXPECT_FALSE(listener.on_perform_action_called());
  EXPECT_EQ(1, menu_show_count);
}

TEST_F(ComboboxTest, ClickButDisabled) {
  InitCombobox(nullptr, Combobox::STYLE_NORMAL);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  combobox_->Layout();
  combobox_->SetEnabled(false);

  // Click the left side, but nothing happens since the combobox is disabled.
  int menu_show_count = 0;
  test_api_->InstallTestMenuRunner(&menu_show_count);
  PerformClick(gfx::Point(combobox_->x() + 1,
                          combobox_->y() + combobox_->height() / 2));
  EXPECT_FALSE(listener.on_perform_action_called());
  EXPECT_EQ(0, menu_show_count);
}

TEST_F(ComboboxTest, NotifyOnClickWithReturnKey) {
  InitCombobox(nullptr, Combobox::STYLE_NORMAL);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  // With STYLE_NORMAL, the click event is ignored.
  SendKeyEvent(ui::VKEY_RETURN);
  EXPECT_FALSE(listener.on_perform_action_called());
}

TEST_F(ComboboxTest, NotifyOnClickWithReturnKeyActionStyle) {
  InitCombobox(nullptr, Combobox::STYLE_ACTION);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  // With STYLE_ACTION, the click event is notified.
  SendKeyEvent(ui::VKEY_RETURN);
  EXPECT_TRUE(listener.on_perform_action_called());
  EXPECT_EQ(0, listener.perform_action_index());
}

TEST_F(ComboboxTest, NotifyOnClickWithSpaceKey) {
  InitCombobox(nullptr, Combobox::STYLE_NORMAL);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  // With STYLE_NORMAL, the click event is ignored.
  SendKeyEvent(ui::VKEY_SPACE);
  EXPECT_FALSE(listener.on_perform_action_called());
  SendKeyEventWithType(ui::VKEY_SPACE, ui::ET_KEY_RELEASED);
  EXPECT_FALSE(listener.on_perform_action_called());
}

TEST_F(ComboboxTest, NotifyOnClickWithSpaceKeyActionStyle) {
  InitCombobox(nullptr, Combobox::STYLE_ACTION);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  // With STYLE_ACTION, the click event is notified after releasing.
  SendKeyEvent(ui::VKEY_SPACE);
  EXPECT_FALSE(listener.on_perform_action_called());
  SendKeyEventWithType(ui::VKEY_SPACE, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(listener.on_perform_action_called());
  EXPECT_EQ(0, listener.perform_action_index());
}

TEST_F(ComboboxTest, NotifyOnClickWithMouse) {
  InitCombobox(nullptr, Combobox::STYLE_ACTION);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  combobox_->Layout();

  // Click the right side (arrow button). The menu is shown.
  int menu_show_count = 0;
  test_api_->InstallTestMenuRunner(&menu_show_count);
  EXPECT_EQ(0, menu_show_count);
  PerformClick(gfx::Point(combobox_->x() + combobox_->width() - 1,
                          combobox_->y() + combobox_->height() / 2));
  EXPECT_FALSE(listener.on_perform_action_called());
  EXPECT_EQ(1, menu_show_count);

  // Click the left side (text button). The click event is notified.
  test_api_->InstallTestMenuRunner(&menu_show_count);
  PerformClick(gfx::Point(combobox_->x() + 1,
                          combobox_->y() + combobox_->height() / 2));
  EXPECT_TRUE(listener.on_perform_action_called());
  EXPECT_EQ(1, menu_show_count);  // Unchanged.
  EXPECT_EQ(0, listener.perform_action_index());
}

TEST_F(ComboboxTest, ConsumingPressKeyEvents) {
  InitCombobox(nullptr, Combobox::STYLE_NORMAL);

  EXPECT_FALSE(combobox_->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE)));
  EXPECT_FALSE(combobox_->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE)));
}

TEST_F(ComboboxTest, ConsumingKeyPressEventsActionStyle) {
  // When the combobox's style is STYLE_ACTION, pressing events of a space key
  // or an enter key will be consumed.
  InitCombobox(nullptr, Combobox::STYLE_ACTION);
  EXPECT_TRUE(combobox_->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE)));
  EXPECT_TRUE(combobox_->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE)));
}

TEST_F(ComboboxTest, ContentWidth) {
  std::vector<std::string> values;
  VectorComboboxModel model(&values);
  TestCombobox combobox(&model, Combobox::STYLE_NORMAL);
  TestCombobox action_combobox(&model, Combobox::STYLE_ACTION);
  ComboboxTestApi test_api(&combobox);
  ComboboxTestApi action_test_api(&action_combobox);

  std::string long_item = "this is the long item";
  std::string short_item = "s";

  values.resize(1);
  values[0] = long_item;
  combobox.ModelChanged();
  action_combobox.ModelChanged();

  const int long_item_width = test_api.content_size().width();

  values[0] = short_item;
  combobox.ModelChanged();
  action_combobox.ModelChanged();

  const int short_item_width = test_api.content_size().width();

  values.resize(2);
  values[0] = short_item;
  values[1] = long_item;
  combobox.ModelChanged();
  action_combobox.ModelChanged();

  // When the style is STYLE_NORMAL, the width will fit with the longest item.
  EXPECT_EQ(long_item_width, test_api.content_size().width());

  // When the style is STYLE_ACTION, the width will fit with the selected item's
  // width.
  EXPECT_EQ(short_item_width, action_test_api.content_size().width());
}

// Test that model updates preserve the selected index, so long as it is in
// range.
TEST_F(ComboboxTest, ModelChanged) {
  InitCombobox(nullptr, Combobox::STYLE_NORMAL);

  EXPECT_EQ(0, combobox_->GetSelectedRow());
  EXPECT_EQ(10, combobox_->GetRowCount());

  combobox_->SetSelectedIndex(4);
  EXPECT_EQ(4, combobox_->GetSelectedRow());

  model_->set_item_count(5);
  combobox_->ModelChanged();
  EXPECT_EQ(5, combobox_->GetRowCount());
  EXPECT_EQ(4, combobox_->GetSelectedRow());  // Unchanged.

  model_->set_item_count(4);
  combobox_->ModelChanged();
  EXPECT_EQ(4, combobox_->GetRowCount());
  EXPECT_EQ(0, combobox_->GetSelectedRow());  // Resets.

  // Restore a non-zero selection.
  combobox_->SetSelectedIndex(2);
  EXPECT_EQ(2, combobox_->GetSelectedRow());

  // Make the selected index a separator.
  std::set<int> separators;
  separators.insert(2);
  model_->SetSeparators(separators);
  combobox_->ModelChanged();
  EXPECT_EQ(4, combobox_->GetRowCount());
  EXPECT_EQ(0, combobox_->GetSelectedRow());  // Resets.

  // Restore a non-zero selection.
  combobox_->SetSelectedIndex(1);
  EXPECT_EQ(1, combobox_->GetSelectedRow());

  // Test an empty model.
  model_->set_item_count(0);
  combobox_->ModelChanged();
  EXPECT_EQ(0, combobox_->GetRowCount());
  EXPECT_EQ(0, combobox_->GetSelectedRow());  // Resets.
}

TEST_F(ComboboxTest, TypingPrefixNotifiesListener) {
  InitCombobox(nullptr, Combobox::STYLE_NORMAL);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);
  ui::TextInputClient* input_client =
      widget_->GetInputMethod()->GetTextInputClient();

  // Type the first character of the second menu item ("JELLY").
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_J, ui::DomCode::US_J, 0,
                         ui::DomKey::FromCharacter('J'), ui::EventTimeForNow());

  input_client->InsertChar(key_event);
  EXPECT_EQ(1, listener.actions_performed());
  EXPECT_EQ(1, listener.perform_action_index());

  // Type the second character of "JELLY", item shouldn't change and
  // OnPerformAction() shouldn't be re-called.
  key_event =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_E, ui::DomCode::US_E, 0,
                   ui::DomKey::FromCharacter('E'), ui::EventTimeForNow());
  input_client->InsertChar(key_event);
  EXPECT_EQ(1, listener.actions_performed());
  EXPECT_EQ(1, listener.perform_action_index());

  // Clears the typed text.
  combobox_->OnBlur();
  combobox_->RequestFocus();

  // Type the first character of "PEANUT BUTTER", which should change the
  // selected index and perform an action.
  key_event =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_E, ui::DomCode::US_P, 0,
                   ui::DomKey::FromCharacter('P'), ui::EventTimeForNow());
  input_client->InsertChar(key_event);
  EXPECT_EQ(2, listener.actions_performed());
  EXPECT_EQ(2, listener.perform_action_index());
}

// Test properties on the Combobox menu model.
TEST_F(ComboboxTest, MenuModel) {
  const int kSeparatorIndex = 3;
  std::set<int> separators;
  separators.insert(kSeparatorIndex);
  InitCombobox(&separators, Combobox::STYLE_NORMAL);

  ui::MenuModel* menu_model = test_api_->menu_model();

  EXPECT_EQ(TestComboboxModel::kItemCount, menu_model->GetItemCount());
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
            menu_model->GetTypeAt(kSeparatorIndex));

#if defined(OS_MACOSX)
  // Comboboxes on Mac should have checkmarks, with the selected item checked,
  EXPECT_EQ(ui::MenuModel::TYPE_CHECK, menu_model->GetTypeAt(0));
  EXPECT_EQ(ui::MenuModel::TYPE_CHECK, menu_model->GetTypeAt(1));
  EXPECT_TRUE(menu_model->IsItemCheckedAt(0));
  EXPECT_FALSE(menu_model->IsItemCheckedAt(1));

  combobox_->SetSelectedIndex(1);
  EXPECT_FALSE(menu_model->IsItemCheckedAt(0));
  EXPECT_TRUE(menu_model->IsItemCheckedAt(1));
#else
  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND, menu_model->GetTypeAt(0));
  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND, menu_model->GetTypeAt(1));
#endif

  EXPECT_EQ(ASCIIToUTF16("PEANUT BUTTER"), menu_model->GetLabelAt(0));
  EXPECT_EQ(ASCIIToUTF16("JELLY"), menu_model->GetLabelAt(1));

  EXPECT_TRUE(menu_model->IsVisibleAt(0));
}

// Check that with STYLE_ACTION, the first item (only) is not shown.
TEST_F(ComboboxTest, MenuModelActionStyleMac) {
  const int kSeparatorIndex = 3;
  std::set<int> separators;
  separators.insert(kSeparatorIndex);
  InitCombobox(&separators, Combobox::STYLE_ACTION);

  ui::MenuModel* menu_model = test_api_->menu_model();

  EXPECT_EQ(TestComboboxModel::kItemCount, menu_model->GetItemCount());
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
            menu_model->GetTypeAt(kSeparatorIndex));

  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND, menu_model->GetTypeAt(0));
  EXPECT_EQ(ui::MenuModel::TYPE_COMMAND, menu_model->GetTypeAt(1));

  EXPECT_EQ(ASCIIToUTF16("PEANUT BUTTER"), menu_model->GetLabelAt(0));
  EXPECT_EQ(ASCIIToUTF16("JELLY"), menu_model->GetLabelAt(1));
  EXPECT_FALSE(menu_model->IsVisibleAt(0));
  EXPECT_TRUE(menu_model->IsVisibleAt(1));
}

}  // namespace views
