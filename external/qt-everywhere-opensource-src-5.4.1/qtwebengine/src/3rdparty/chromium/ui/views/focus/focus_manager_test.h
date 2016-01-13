// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_FOCUS_FOCUS_MANAGER_TEST_H_
#define UI_VIEWS_FOCUS_FOCUS_MANAGER_TEST_H_

#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

class FocusChangeListener;

class FocusManagerTest : public ViewsTestBase, public WidgetDelegate {
 public:
  FocusManagerTest();
  virtual ~FocusManagerTest();

  // Convenience to obtain the focus manager for the test's hosting widget.
  FocusManager* GetFocusManager();

  // Overridden from ViewsTestBase:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Overridden from WidgetDelegate:
  virtual View* GetContentsView() OVERRIDE;
  virtual Widget* GetWidget() OVERRIDE;
  virtual const Widget* GetWidget() const OVERRIDE;
  virtual void GetAccessiblePanes(std::vector<View*>* panes) OVERRIDE;

 protected:
  // Called after the Widget is initialized and the content view is added.
  // Override to add controls to the layout.
  virtual void InitContentView();

  void AddFocusChangeListener(FocusChangeListener* listener);
  void AddWidgetFocusChangeListener(WidgetFocusChangeListener* listener);

  // For testing FocusManager::RotatePaneFocus().
  void SetAccessiblePanes(const std::vector<View*>& panes);

 private:
  View* contents_view_;
  FocusChangeListener* focus_change_listener_;
  WidgetFocusChangeListener* widget_focus_change_listener_;
  std::vector<View*> accessible_panes_;

  DISALLOW_COPY_AND_ASSIGN(FocusManagerTest);
};

typedef std::pair<View*, View*> ViewPair;

// Use to record focus change notifications.
class TestFocusChangeListener : public FocusChangeListener {
 public:
  TestFocusChangeListener();
  virtual ~TestFocusChangeListener();

  const std::vector<ViewPair>& focus_changes() const { return focus_changes_; }
  void ClearFocusChanges();

  // Overridden from FocusChangeListener:
  virtual void OnWillChangeFocus(View* focused_before,
                                 View* focused_now) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before,
                                View* focused_now) OVERRIDE;

 private:
  // A vector of which views lost/gained focus.
  std::vector<ViewPair> focus_changes_;

  DISALLOW_COPY_AND_ASSIGN(TestFocusChangeListener);
};

typedef std::pair<gfx::NativeView, gfx::NativeView> NativeViewPair;

// Use to record widget focus change notifications.
class TestWidgetFocusChangeListener : public WidgetFocusChangeListener {
 public:
  TestWidgetFocusChangeListener();
  virtual ~TestWidgetFocusChangeListener();

  const std::vector<NativeViewPair>& focus_changes() const {
    return focus_changes_;
  }
  void ClearFocusChanges();

  // Overridden from WidgetFocusChangeListener:
  virtual void OnNativeFocusChange(gfx::NativeView focused_before,
                                   gfx::NativeView focused_now) OVERRIDE;

 private:
  // Pairs of (focused_before, focused_now) parameters we've received via calls
  // to OnNativeFocusChange(), in oldest-to-newest-received order.
  std::vector<NativeViewPair> focus_changes_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetFocusChangeListener);
};

}  // namespace views

#endif  // UI_VIEWS_FOCUS_FOCUS_MANAGER_TEST_H_
