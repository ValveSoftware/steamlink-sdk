// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/views/accessibility/native_view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace test {

class NativeViewAccessibilityTest;

namespace {

class TestButton : public Button {
 public:
  TestButton() : Button(NULL) {}
};

}  // namespace

class NativeViewAccessibilityTest : public ViewsTestBase {
 public:
  NativeViewAccessibilityTest() {}
  ~NativeViewAccessibilityTest() override {}

  void SetUp() override {
    ViewsTestBase::SetUp();
    button_.reset(new TestButton());
    button_->SetSize(gfx::Size(20, 20));
    button_accessibility_ = NativeViewAccessibility::Create(button_.get());

    label_.reset(new Label);
    button_->AddChildView(label_.get());
    label_accessibility_ = NativeViewAccessibility::Create(label_.get());
  }

  void TearDown() override {
    button_accessibility_->Destroy();
    button_accessibility_ = NULL;
    label_accessibility_->Destroy();
    label_accessibility_ = NULL;
    ViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TestButton> button_;
  NativeViewAccessibility* button_accessibility_;
  std::unique_ptr<Label> label_;
  NativeViewAccessibility* label_accessibility_;
};

TEST_F(NativeViewAccessibilityTest, RoleShouldMatch) {
  EXPECT_EQ(ui::AX_ROLE_BUTTON, button_accessibility_->GetData().role);
  EXPECT_EQ(ui::AX_ROLE_STATIC_TEXT, label_accessibility_->GetData().role);
}

TEST_F(NativeViewAccessibilityTest, BoundsShouldMatch) {
  gfx::Rect bounds = button_accessibility_->GetData().location;
  bounds.Offset(button_accessibility_->GetGlobalCoordinateOffset());
  EXPECT_EQ(button_->GetBoundsInScreen(), bounds);
}

TEST_F(NativeViewAccessibilityTest, LabelIsChildOfButton) {
  EXPECT_EQ(1, button_accessibility_->GetChildCount());
  EXPECT_EQ(label_->GetNativeViewAccessible(),
            button_accessibility_->ChildAtIndex(0));
  EXPECT_EQ(button_->GetNativeViewAccessible(),
            label_accessibility_->GetParent());
}

// Subclass of NativeViewAccessibility that destroys itself when its
// parent widget is destroyed, for the purposes of making sure this
// doesn't lead to a crash.
class TestNativeViewAccessibility : public NativeViewAccessibility {
 public:
  explicit TestNativeViewAccessibility(View* view)
      : NativeViewAccessibility(view) {}

  void OnWidgetDestroying(Widget* widget) override {
    bool is_destroying_parent_widget = (parent_widget_ == widget);
    NativeViewAccessibility::OnWidgetDestroying(widget);
    if (is_destroying_parent_widget)
      delete this;
  }
};

TEST_F(NativeViewAccessibilityTest, CrashOnWidgetDestroyed) {
  std::unique_ptr<Widget> parent_widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  parent_widget->Init(params);

  std::unique_ptr<Widget> child_widget(new Widget);
  child_widget->Init(params);

  // Make sure that deleting the parent widget can't cause a crash
  // due to NativeViewAccessibility not unregistering itself as a
  // WidgetObserver. Note that TestNativeViewAccessibility is a subclass
  // defined above that destroys itself when its parent widget is destroyed.
  TestNativeViewAccessibility* child_accessible =
      new TestNativeViewAccessibility(child_widget->GetRootView());
  child_accessible->SetParentWidget(parent_widget.get());
  parent_widget.reset();
}

}  // namespace test
}  // namespace views
