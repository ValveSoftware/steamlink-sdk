// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

class NativeViewHostTest : public ViewsTestBase {
 public:
  NativeViewHostTest() {
  }

  virtual void SetUp() OVERRIDE {
    ViewsTestBase::SetUp();

    // Create the top level widget.
    toplevel_.reset(new Widget);
    Widget::InitParams toplevel_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    toplevel_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    toplevel_->Init(toplevel_params);
  }

  // Create a child widget whose native parent is |native_parent_view|, uses
  // |contents_view|, and is attached to |host| which is added as a child to
  // |parent_view|.
  Widget* CreateChildForHost(gfx::NativeView native_parent_view,
                             View* parent_view,
                             View* contents_view,
                             NativeViewHost* host) {
    Widget* child = new Widget;
    Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
    child_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    child_params.parent = native_parent_view;
    child->Init(child_params);
    child->SetContentsView(contents_view);

    // Owned by |parent_view|.
    parent_view->AddChildView(host);
    host->Attach(child->GetNativeView());

    return child;
  }

  Widget* toplevel() {
    return toplevel_.get();
  }

 private:
  scoped_ptr<Widget> toplevel_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostTest);
};

namespace {

// View implementation used by NativeViewHierarchyChanged to count number of
// times NativeViewHierarchyChanged() is invoked.
class NativeViewHierarchyChangedTestView : public View {
 public:
  NativeViewHierarchyChangedTestView() : notification_count_(0) {
  }

  void ResetCount() {
    notification_count_ = 0;
  }

  int notification_count() const { return notification_count_; }

  // Overriden from View:
  virtual void NativeViewHierarchyChanged() OVERRIDE {
    ++notification_count_;
    View::NativeViewHierarchyChanged();
  }

 private:
  int notification_count_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHierarchyChangedTestView);
};

aura::Window* GetNativeParent(aura::Window* window) {
  return window->parent();
}

class ViewHierarchyChangedTestHost : public NativeViewHost {
 public:
  ViewHierarchyChangedTestHost()
      : num_parent_changes_(0) {
  }

  void ResetParentChanges() {
    num_parent_changes_ = 0;
  }

  int num_parent_changes() const {
    return num_parent_changes_;
  }

  // Overriden from NativeViewHost:
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE {
    gfx::NativeView parent_before = native_view() ?
        GetNativeParent(native_view()) : NULL;
    NativeViewHost::ViewHierarchyChanged(details);
    gfx::NativeView parent_after = native_view() ?
        GetNativeParent(native_view()) : NULL;
    if (parent_before != parent_after)
      ++num_parent_changes_;
  }

 private:
  int num_parent_changes_;

  DISALLOW_COPY_AND_ASSIGN(ViewHierarchyChangedTestHost);
};

}  // namespace

// Verifies NativeViewHierarchyChanged is sent.
TEST_F(NativeViewHostTest, NativeViewHierarchyChanged) {
  // Create a child widget.
  NativeViewHierarchyChangedTestView* test_view =
      new NativeViewHierarchyChangedTestView;
  NativeViewHost* host = new NativeViewHost;
  scoped_ptr<Widget> child(CreateChildForHost(toplevel()->GetNativeView(),
                                              toplevel()->GetRootView(),
                                              test_view,
                                              host));

  EXPECT_EQ(0, test_view->notification_count());
  test_view->ResetCount();

  // Detaching should send a NativeViewHierarchyChanged() notification and
  // change the parent.
  host->Detach();
  EXPECT_EQ(1, test_view->notification_count());
  EXPECT_NE(toplevel()->GetNativeView(),
            GetNativeParent(child->GetNativeView()));
  test_view->ResetCount();

  // Attaching should send a NativeViewHierarchyChanged() notification and
  // reset the parent.
  host->Attach(child->GetNativeView());
  EXPECT_EQ(1, test_view->notification_count());
  EXPECT_EQ(toplevel()->GetNativeView(),
            GetNativeParent(child->GetNativeView()));
}

// Verifies ViewHierarchyChanged handles NativeViewHost remove, add and move
// (reparent) operations with correct parent changes.
// This exercises the non-recursive code paths in
// View::PropagateRemoveNotifications() and View::PropagateAddNotifications().
TEST_F(NativeViewHostTest, ViewHierarchyChangedForHost) {
  // Original tree:
  // toplevel
  // +-- host0 (NativeViewHost)
  //     +-- child0 (Widget, attached to host0)
  //     +-- test_host (ViewHierarchyChangedTestHost)
  //         +-- test_child (Widget, attached to test_host)
  // +-- host1 (NativeViewHost)
  //     +-- child1 (Widget, attached to host1)

  // Add two children widgets attached to a NativeViewHost, and a test
  // grandchild as child widget of host0.
  NativeViewHost* host0 = new NativeViewHost;
  scoped_ptr<Widget> child0(CreateChildForHost(toplevel()->GetNativeView(),
                                               toplevel()->GetRootView(),
                                               new View,
                                               host0));
  NativeViewHost* host1 = new NativeViewHost;
  scoped_ptr<Widget> child1(CreateChildForHost(toplevel()->GetNativeView(),
                                               toplevel()->GetRootView(),
                                               new View,
                                               host1));
  ViewHierarchyChangedTestHost* test_host = new ViewHierarchyChangedTestHost;
  scoped_ptr<Widget> test_child(CreateChildForHost(host0->native_view(),
                                                   host0,
                                                   new View,
                                                   test_host));

  // Remove test_host from host0, expect 1 parent change.
  test_host->ResetParentChanges();
  EXPECT_EQ(0, test_host->num_parent_changes());
  host0->RemoveChildView(test_host);
  EXPECT_EQ(1, test_host->num_parent_changes());

  // Add test_host back to host0, expect 1 parent change.
  test_host->ResetParentChanges();
  EXPECT_EQ(0, test_host->num_parent_changes());
  host0->AddChildView(test_host);
  EXPECT_EQ(1, test_host->num_parent_changes());

  // Reparent test_host to host1, expect no parent change because the old and
  // new parents, host0 and host1, belong to the same toplevel widget.
  test_host->ResetParentChanges();
  EXPECT_EQ(0, test_host->num_parent_changes());
  host1->AddChildView(test_host);
  EXPECT_EQ(0, test_host->num_parent_changes());

  // Reparent test_host to contents view of child0, expect 2 parent changes
  // because the old parent belongs to the toplevel widget whereas the new
  // parent belongs to the child0.
  test_host->ResetParentChanges();
  EXPECT_EQ(0, test_host->num_parent_changes());
  child0->GetContentsView()->AddChildView(test_host);
  EXPECT_EQ(2, test_host->num_parent_changes());
}

// Verifies ViewHierarchyChanged handles NativeViewHost's parent remove, add and
// move (reparent) operations with correct parent changes.
// This exercises the recursive code paths in
// View::PropagateRemoveNotifications() and View::PropagateAddNotifications().
TEST_F(NativeViewHostTest, ViewHierarchyChangedForHostParent) {
  // Original tree:
  // toplevel
  // +-- view0 (View)
  //     +-- host0 (NativeViewHierarchyChangedTestHost)
  //         +-- child0 (Widget, attached to host0)
  // +-- view1 (View)
  //     +-- host1 (NativeViewHierarchyChangedTestHost)
  //         +-- child1 (Widget, attached to host1)

  // Add two children views.
  View* view0 = new View;
  toplevel()->GetRootView()->AddChildView(view0);
  View* view1 = new View;
  toplevel()->GetRootView()->AddChildView(view1);

  // To each child view, add a child widget.
  ViewHierarchyChangedTestHost* host0 = new ViewHierarchyChangedTestHost;
  scoped_ptr<Widget> child0(CreateChildForHost(toplevel()->GetNativeView(),
                                               view0,
                                               new View,
                                               host0));
  ViewHierarchyChangedTestHost* host1 = new ViewHierarchyChangedTestHost;
  scoped_ptr<Widget> child1(CreateChildForHost(toplevel()->GetNativeView(),
                                               view1,
                                               new View,
                                               host1));

  // Remove view0 from top level, expect 1 parent change.
  host0->ResetParentChanges();
  EXPECT_EQ(0, host0->num_parent_changes());
  toplevel()->GetRootView()->RemoveChildView(view0);
  EXPECT_EQ(1, host0->num_parent_changes());

  // Add view0 back to top level, expect 1 parent change.
  host0->ResetParentChanges();
  EXPECT_EQ(0, host0->num_parent_changes());
  toplevel()->GetRootView()->AddChildView(view0);
  EXPECT_EQ(1, host0->num_parent_changes());

  // Reparent view0 to view1, expect no parent change because the old and new
  // parents of both view0 and view1 belong to the same toplevel widget.
  host0->ResetParentChanges();
  host1->ResetParentChanges();
  EXPECT_EQ(0, host0->num_parent_changes());
  EXPECT_EQ(0, host1->num_parent_changes());
  view1->AddChildView(view0);
  EXPECT_EQ(0, host0->num_parent_changes());
  EXPECT_EQ(0, host1->num_parent_changes());

  // Restore original view hierarchy by adding back view0 to top level.
  // Then, reparent view1 to contents view of child0.
  // Expect 2 parent changes because the old parent belongs to the toplevel
  // widget whereas the new parent belongs to the 1st child widget.
  toplevel()->GetRootView()->AddChildView(view0);
  host0->ResetParentChanges();
  host1->ResetParentChanges();
  EXPECT_EQ(0, host0->num_parent_changes());
  EXPECT_EQ(0, host1->num_parent_changes());
  child0->GetContentsView()->AddChildView(view1);
  EXPECT_EQ(0, host0->num_parent_changes());
  EXPECT_EQ(2, host1->num_parent_changes());
}

}  // namespace views
