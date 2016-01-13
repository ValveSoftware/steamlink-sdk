// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessible_pane_view.h"

#include "ui/base/accelerators/accelerator.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

// TODO(alicet): bring pane rotation into views and add tests.
//               See browser_view.cc for details.

typedef ViewsTestBase AccessiblePaneViewTest;

class TestBarView : public AccessiblePaneView,
                    public ButtonListener {
 public:
  TestBarView();
  virtual ~TestBarView();

  virtual void ButtonPressed(Button* sender,
                             const ui::Event& event) OVERRIDE;
  LabelButton* child_button() const { return child_button_.get(); }
  LabelButton* second_child_button() const {
    return second_child_button_.get();
  }
  LabelButton* third_child_button() const { return third_child_button_.get(); }
  LabelButton* not_child_button() const { return not_child_button_.get(); }

  virtual View* GetDefaultFocusableChild() OVERRIDE;

 private:
  void Init();

  scoped_ptr<LabelButton> child_button_;
  scoped_ptr<LabelButton> second_child_button_;
  scoped_ptr<LabelButton> third_child_button_;
  scoped_ptr<LabelButton> not_child_button_;

  DISALLOW_COPY_AND_ASSIGN(TestBarView);
};

TestBarView::TestBarView() {
  Init();
  set_allow_deactivate_on_esc(true);
}

TestBarView::~TestBarView() {}

void TestBarView::ButtonPressed(Button* sender, const ui::Event& event) {
}

void TestBarView::Init() {
  SetLayoutManager(new FillLayout());
  base::string16 label;
  child_button_.reset(new LabelButton(this, label));
  AddChildView(child_button_.get());
  second_child_button_.reset(new LabelButton(this, label));
  AddChildView(second_child_button_.get());
  third_child_button_.reset(new LabelButton(this, label));
  AddChildView(third_child_button_.get());
  not_child_button_.reset(new LabelButton(this, label));
}

View* TestBarView::GetDefaultFocusableChild() {
  return child_button_.get();
}

TEST_F(AccessiblePaneViewTest, SimpleSetPaneFocus) {
  TestBarView* test_view = new TestBarView();
  scoped_ptr<Widget> widget(new Widget());
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  View* root = widget->GetRootView();
  root->AddChildView(test_view);
  widget->Show();
  widget->Activate();

  // Set pane focus succeeds, focus on child.
  EXPECT_TRUE(test_view->SetPaneFocusAndFocusDefault());
  EXPECT_EQ(test_view, test_view->GetPaneFocusTraversable());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());

  // Set focus on non child view, focus failed, stays on pane.
  EXPECT_TRUE(test_view->SetPaneFocus(test_view->not_child_button()));
  EXPECT_FALSE(test_view->not_child_button() ==
               test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  widget->CloseNow();
  widget.reset();
}

// This test will not work properly in Windows because it uses ::GetNextWindow
// on deactivate which is rather unpredictable where the focus will land.
TEST_F(AccessiblePaneViewTest, SetPaneFocusAndRestore) {
  View* test_view_main = new View();
  scoped_ptr<Widget> widget_main(new Widget());
  Widget::InitParams params_main = CreateParams(Widget::InitParams::TYPE_POPUP);
  params_main.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params_main.bounds = gfx::Rect(0, 0, 20, 20);
  widget_main->Init(params_main);
  View* root_main = widget_main->GetRootView();
  root_main->AddChildView(test_view_main);
  widget_main->Activate();
  test_view_main->GetFocusManager()->SetFocusedView(test_view_main);
  EXPECT_TRUE(widget_main->IsActive());
  EXPECT_TRUE(test_view_main->HasFocus());

  TestBarView* test_view_bar = new TestBarView();
  scoped_ptr<Widget> widget_bar(new Widget());
  Widget::InitParams params_bar = CreateParams(Widget::InitParams::TYPE_POPUP);
  params_bar.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params_bar.bounds = gfx::Rect(50, 50, 650, 650);
  widget_bar->Init(params_bar);
  View* root_bar = widget_bar->GetRootView();
  root_bar->AddChildView(test_view_bar);
  widget_bar->Show();
  widget_bar->Activate();

  // Set pane focus succeeds, focus on child.
  EXPECT_TRUE(test_view_bar->SetPaneFocusAndFocusDefault());
  EXPECT_FALSE(test_view_main->HasFocus());
  EXPECT_FALSE(widget_main->IsActive());
  EXPECT_EQ(test_view_bar, test_view_bar->GetPaneFocusTraversable());
  EXPECT_EQ(test_view_bar->child_button(),
            test_view_bar->GetWidget()->GetFocusManager()->GetFocusedView());

  test_view_bar->AcceleratorPressed(test_view_bar->escape_key());
  EXPECT_TRUE(widget_main->IsActive());
  EXPECT_FALSE(widget_bar->IsActive());

  widget_bar->CloseNow();
  widget_bar.reset();

  widget_main->CloseNow();
  widget_main.reset();
}

TEST_F(AccessiblePaneViewTest, TwoSetPaneFocus) {
  TestBarView* test_view = new TestBarView();
  TestBarView* test_view_2 = new TestBarView();
  scoped_ptr<Widget> widget(new Widget());
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  View* root = widget->GetRootView();
  root->AddChildView(test_view);
  root->AddChildView(test_view_2);
  widget->Show();
  widget->Activate();

  // Set pane focus succeeds, focus on child.
  EXPECT_TRUE(test_view->SetPaneFocusAndFocusDefault());
  EXPECT_EQ(test_view, test_view->GetPaneFocusTraversable());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());

  // Set focus on another test_view, focus move to that pane.
  EXPECT_TRUE(test_view_2->SetPaneFocus(test_view_2->second_child_button()));
  EXPECT_FALSE(test_view->child_button() ==
               test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(test_view_2->second_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  widget->CloseNow();
  widget.reset();
}

TEST_F(AccessiblePaneViewTest, PaneFocusTraversal) {
  TestBarView* test_view = new TestBarView();
  TestBarView* original_test_view = new TestBarView();
  scoped_ptr<Widget> widget(new Widget());
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  View* root = widget->GetRootView();
  root->AddChildView(original_test_view);
  root->AddChildView(test_view);
  widget->Show();
  widget->Activate();

  // Set pane focus on first view.
  EXPECT_TRUE(original_test_view->SetPaneFocus(
      original_test_view->third_child_button()));

  // Test travesal in second view.
  // Set pane focus on second child.
  EXPECT_TRUE(test_view->SetPaneFocus(test_view->second_child_button()));
  // home
  test_view->AcceleratorPressed(test_view->home_key());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  // end
  test_view->AcceleratorPressed(test_view->end_key());
  EXPECT_EQ(test_view->third_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  // left
  test_view->AcceleratorPressed(test_view->left_key());
  EXPECT_EQ(test_view->second_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  // right, right
  test_view->AcceleratorPressed(test_view->right_key());
  test_view->AcceleratorPressed(test_view->right_key());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());

  // ESC
  test_view->AcceleratorPressed(test_view->escape_key());
  EXPECT_EQ(original_test_view->third_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  widget->CloseNow();
  widget.reset();
}
}  // namespace views
