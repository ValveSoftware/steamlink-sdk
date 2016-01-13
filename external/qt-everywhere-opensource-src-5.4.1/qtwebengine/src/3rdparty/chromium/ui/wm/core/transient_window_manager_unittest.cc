// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/transient_window_manager.h"

#include "ui/aura/client/visibility_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/wm/core/transient_window_observer.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/core/wm_state.h"

using aura::Window;

using aura::test::ChildWindowIDsAsString;
using aura::test::CreateTestWindowWithId;

namespace wm {

class TestTransientWindowObserver : public TransientWindowObserver {
 public:
  TestTransientWindowObserver() : add_count_(0), remove_count_(0) {
  }

  virtual ~TestTransientWindowObserver() {
  }

  int add_count() const { return add_count_; }
  int remove_count() const { return remove_count_; }

  // TransientWindowObserver overrides:
  virtual void OnTransientChildAdded(Window* window,
                                     Window* transient) OVERRIDE {
    add_count_++;
  }
  virtual void OnTransientChildRemoved(Window* window,
                                       Window* transient) OVERRIDE {
    remove_count_++;
  }

 private:
  int add_count_;
  int remove_count_;

  DISALLOW_COPY_AND_ASSIGN(TestTransientWindowObserver);
};

class TransientWindowManagerTest : public aura::test::AuraTestBase {
 public:
  TransientWindowManagerTest() {}
  virtual ~TransientWindowManagerTest() {}

  virtual void SetUp() OVERRIDE {
    AuraTestBase::SetUp();
    wm_state_.reset(new wm::WMState);
  }

  virtual void TearDown() OVERRIDE {
    wm_state_.reset();
    AuraTestBase::TearDown();
  }

 protected:
  // Creates a transient window that is transient to |parent|.
  Window* CreateTransientChild(int id, Window* parent) {
    Window* window = new Window(NULL);
    window->set_id(id);
    window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    window->Init(aura::WINDOW_LAYER_TEXTURED);
    AddTransientChild(parent, window);
    aura::client::ParentWindowWithContext(window, root_window(), gfx::Rect());
    return window;
  }

 private:
  scoped_ptr<wm::WMState> wm_state_;

  DISALLOW_COPY_AND_ASSIGN(TransientWindowManagerTest);
};

// Various assertions for transient children.
TEST_F(TransientWindowManagerTest, TransientChildren) {
  scoped_ptr<Window> parent(CreateTestWindowWithId(0, root_window()));
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, parent.get()));
  scoped_ptr<Window> w3(CreateTestWindowWithId(3, parent.get()));
  Window* w2 = CreateTestWindowWithId(2, parent.get());
  // w2 is now owned by w1.
  AddTransientChild(w1.get(), w2);
  // Stack w1 at the top (end), this should force w2 to be last (on top of w1).
  parent->StackChildAtTop(w1.get());
  ASSERT_EQ(3u, parent->children().size());
  EXPECT_EQ(w2, parent->children().back());

  // Destroy w1, which should also destroy w3 (since it's a transient child).
  w1.reset();
  w2 = NULL;
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(w3.get(), parent->children()[0]);

  w1.reset(CreateTestWindowWithId(4, parent.get()));
  w2 = CreateTestWindowWithId(5, w3.get());
  AddTransientChild(w1.get(), w2);
  parent->StackChildAtTop(w3.get());
  // Stack w1 at the top (end), this shouldn't affect w2 since it has a
  // different parent.
  parent->StackChildAtTop(w1.get());
  ASSERT_EQ(2u, parent->children().size());
  EXPECT_EQ(w3.get(), parent->children()[0]);
  EXPECT_EQ(w1.get(), parent->children()[1]);

  // Hiding parent should hide transient children.
  EXPECT_TRUE(w2->IsVisible());
  w1->Hide();
  EXPECT_FALSE(w2->IsVisible());
}

// Tests that transient children are stacked as a unit when using stack above.
TEST_F(TransientWindowManagerTest, TransientChildrenGroupAbove) {
  scoped_ptr<Window> parent(CreateTestWindowWithId(0, root_window()));
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, parent.get()));
  Window* w11 = CreateTestWindowWithId(11, parent.get());
  scoped_ptr<Window> w2(CreateTestWindowWithId(2, parent.get()));
  Window* w21 = CreateTestWindowWithId(21, parent.get());
  Window* w211 = CreateTestWindowWithId(211, parent.get());
  Window* w212 = CreateTestWindowWithId(212, parent.get());
  Window* w213 = CreateTestWindowWithId(213, parent.get());
  Window* w22 = CreateTestWindowWithId(22, parent.get());
  ASSERT_EQ(8u, parent->children().size());

  // w11 is now owned by w1.
  AddTransientChild(w1.get(), w11);
  // w21 is now owned by w2.
  AddTransientChild(w2.get(), w21);
  // w22 is now owned by w2.
  AddTransientChild(w2.get(), w22);
  // w211 is now owned by w21.
  AddTransientChild(w21, w211);
  // w212 is now owned by w21.
  AddTransientChild(w21, w212);
  // w213 is now owned by w21.
  AddTransientChild(w21, w213);
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  // Stack w1 at the top (end), this should force w11 to be last (on top of w1).
  parent->StackChildAtTop(w1.get());
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  // This tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '22' would be following '21'.
  parent->StackChildAtTop(w2.get());
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAbove(w11, w2.get());
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAbove(w21, w1.get());
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAbove(w21, w22);
  EXPECT_EQ(w213, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 211 212 213", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAbove(w11, w21);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 211 212 213 1 11", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAbove(w213, w21);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  // No change when stacking a transient parent above its transient child.
  parent->StackChildAbove(w21, w211);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  // This tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '22' would be following '21'.
  parent->StackChildAbove(w2.get(), w1.get());
  EXPECT_EQ(w212, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 213 211 212", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAbove(w11, w213);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));
}

// Tests that transient children are stacked as a unit when using stack below.
TEST_F(TransientWindowManagerTest, TransientChildrenGroupBelow) {
  scoped_ptr<Window> parent(CreateTestWindowWithId(0, root_window()));
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, parent.get()));
  Window* w11 = CreateTestWindowWithId(11, parent.get());
  scoped_ptr<Window> w2(CreateTestWindowWithId(2, parent.get()));
  Window* w21 = CreateTestWindowWithId(21, parent.get());
  Window* w211 = CreateTestWindowWithId(211, parent.get());
  Window* w212 = CreateTestWindowWithId(212, parent.get());
  Window* w213 = CreateTestWindowWithId(213, parent.get());
  Window* w22 = CreateTestWindowWithId(22, parent.get());
  ASSERT_EQ(8u, parent->children().size());

  // w11 is now owned by w1.
  AddTransientChild(w1.get(), w11);
  // w21 is now owned by w2.
  AddTransientChild(w2.get(), w21);
  // w22 is now owned by w2.
  AddTransientChild(w2.get(), w22);
  // w211 is now owned by w21.
  AddTransientChild(w21, w211);
  // w212 is now owned by w21.
  AddTransientChild(w21, w212);
  // w213 is now owned by w21.
  AddTransientChild(w21, w213);
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  // Stack w2 at the bottom, this should force w11 to be last (on top of w1).
  // This also tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '22' would be following '21'.
  parent->StackChildAtBottom(w2.get());
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAtBottom(w1.get());
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w21, w1.get());
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w11, w2.get());
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w22, w21);
  EXPECT_EQ(w213, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 211 212 213", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w21, w11);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 211 212 213 1 11", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w213, w211);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  // No change when stacking a transient parent below its transient child.
  parent->StackChildBelow(w21, w211);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w1.get(), w2.get());
  EXPECT_EQ(w212, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 213 211 212", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w213, w11);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));
}

// Tests that transient windows are stacked properly when created.
TEST_F(TransientWindowManagerTest, StackUponCreation) {
  scoped_ptr<Window> window0(CreateTestWindowWithId(0, root_window()));
  scoped_ptr<Window> window1(CreateTestWindowWithId(1, root_window()));

  scoped_ptr<Window> window2(CreateTransientChild(2, window0.get()));
  EXPECT_EQ("0 2 1", ChildWindowIDsAsString(root_window()));
}

// Tests that windows are restacked properly after a call to AddTransientChild()
// or RemoveTransientChild().
TEST_F(TransientWindowManagerTest, RestackUponAddOrRemoveTransientChild) {
  scoped_ptr<Window> windows[4];
  for (int i = 0; i < 4; i++)
    windows[i].reset(CreateTestWindowWithId(i, root_window()));
  EXPECT_EQ("0 1 2 3", ChildWindowIDsAsString(root_window()));

  AddTransientChild(windows[0].get(), windows[2].get());
  EXPECT_EQ("0 2 1 3", ChildWindowIDsAsString(root_window()));

  AddTransientChild(windows[0].get(), windows[3].get());
  EXPECT_EQ("0 2 3 1", ChildWindowIDsAsString(root_window()));

  RemoveTransientChild(windows[0].get(), windows[2].get());
  EXPECT_EQ("0 3 2 1", ChildWindowIDsAsString(root_window()));

  RemoveTransientChild(windows[0].get(), windows[3].get());
  EXPECT_EQ("0 3 2 1", ChildWindowIDsAsString(root_window()));
}

namespace {

// Used by NotifyDelegateAfterDeletingTransients. Adds a string to a vector when
// OnWindowDestroyed() is invoked so that destruction order can be verified.
class DestroyedTrackingDelegate : public aura::test::TestWindowDelegate {
 public:
  explicit DestroyedTrackingDelegate(const std::string& name,
                                     std::vector<std::string>* results)
      : name_(name),
        results_(results) {}

  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {
    results_->push_back(name_);
  }

 private:
  const std::string name_;
  std::vector<std::string>* results_;

  DISALLOW_COPY_AND_ASSIGN(DestroyedTrackingDelegate);
};

}  // namespace

// Verifies the delegate is notified of destruction after transients are
// destroyed.
TEST_F(TransientWindowManagerTest, NotifyDelegateAfterDeletingTransients) {
  std::vector<std::string> destruction_order;

  DestroyedTrackingDelegate parent_delegate("parent", &destruction_order);
  scoped_ptr<Window> parent(new Window(&parent_delegate));
  parent->Init(aura::WINDOW_LAYER_NOT_DRAWN);

  DestroyedTrackingDelegate transient_delegate("transient", &destruction_order);
  Window* transient = new Window(&transient_delegate);  // Owned by |parent|.
  transient->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  AddTransientChild(parent.get(), transient);
  parent.reset();

  ASSERT_EQ(2u, destruction_order.size());
  EXPECT_EQ("transient", destruction_order[0]);
  EXPECT_EQ("parent", destruction_order[1]);
}

TEST_F(TransientWindowManagerTest, StackTransientsWhoseLayersHaveNoDelegate) {
  // Create a window with several transients, then a couple windows on top.
  scoped_ptr<Window> window1(CreateTestWindowWithId(1, root_window()));
  scoped_ptr<Window> window11(CreateTransientChild(11, window1.get()));
  scoped_ptr<Window> window12(CreateTransientChild(12, window1.get()));
  scoped_ptr<Window> window13(CreateTransientChild(13, window1.get()));
  scoped_ptr<Window> window2(CreateTestWindowWithId(2, root_window()));
  scoped_ptr<Window> window3(CreateTestWindowWithId(3, root_window()));

  EXPECT_EQ("1 11 12 13 2 3", ChildWindowIDsAsString(root_window()));

  // Remove the delegates of a couple of transients, as if they are closing
  // and animating out.
  window11->layer()->set_delegate(NULL);
  window13->layer()->set_delegate(NULL);

  // Move window1 to the front.  All transients should move with it, and their
  // order should be preserved.
  root_window()->StackChildAtTop(window1.get());

  EXPECT_EQ("2 3 1 11 12 13", ChildWindowIDsAsString(root_window()));
}

TEST_F(TransientWindowManagerTest,
       StackTransientsLayersRelativeToOtherTransients) {
  // Create a window with several transients, then a couple windows on top.
  scoped_ptr<Window> window1(CreateTestWindowWithId(1, root_window()));
  scoped_ptr<Window> window11(CreateTransientChild(11, window1.get()));
  scoped_ptr<Window> window12(CreateTransientChild(12, window1.get()));
  scoped_ptr<Window> window13(CreateTransientChild(13, window1.get()));

  EXPECT_EQ("1 11 12 13", ChildWindowIDsAsString(root_window()));

  // Stack 11 above 12.
  root_window()->StackChildAbove(window11.get(), window12.get());
  EXPECT_EQ("1 12 11 13", ChildWindowIDsAsString(root_window()));

  // Stack 13 below 12.
  root_window()->StackChildBelow(window13.get(), window12.get());
  EXPECT_EQ("1 13 12 11", ChildWindowIDsAsString(root_window()));

  // Stack 11 above 1.
  root_window()->StackChildAbove(window11.get(), window1.get());
  EXPECT_EQ("1 11 13 12", ChildWindowIDsAsString(root_window()));

  // Stack 12 below 13.
  root_window()->StackChildBelow(window12.get(), window13.get());
  EXPECT_EQ("1 11 12 13", ChildWindowIDsAsString(root_window()));
}

TEST_F(TransientWindowManagerTest,
       StackTransientsLayersRelativeToOtherTransientsNoLayerDelegate) {
  // Create a window with several transients, then a couple windows on top.
  scoped_ptr<Window> window1(CreateTestWindowWithId(1, root_window()));
  scoped_ptr<Window> window11(CreateTransientChild(11, window1.get()));
  scoped_ptr<Window> window12(CreateTransientChild(12, window1.get()));
  scoped_ptr<Window> window13(CreateTransientChild(13, window1.get()));
  scoped_ptr<Window> window2(CreateTestWindowWithId(2, root_window()));
  scoped_ptr<Window> window3(CreateTestWindowWithId(3, root_window()));

  EXPECT_EQ("1 11 12 13 2 3", ChildWindowIDsAsString(root_window()));

  window1->layer()->set_delegate(NULL);

  // Stack 1 at top.
  root_window()->StackChildAtTop(window1.get());
  EXPECT_EQ("2 3 1 11 12 13", ChildWindowIDsAsString(root_window()));
}

class StackingMadrigalLayoutManager : public aura::LayoutManager {
 public:
  explicit StackingMadrigalLayoutManager(Window* root_window)
      : root_window_(root_window) {
    root_window_->SetLayoutManager(this);
  }
  virtual ~StackingMadrigalLayoutManager() {
  }

 private:
  // Overridden from LayoutManager:
  virtual void OnWindowResized() OVERRIDE {}
  virtual void OnWindowAddedToLayout(Window* child) OVERRIDE {}
  virtual void OnWillRemoveWindowFromLayout(Window* child) OVERRIDE {}
  virtual void OnWindowRemovedFromLayout(Window* child) OVERRIDE {}
  virtual void OnChildWindowVisibilityChanged(Window* child,
                                              bool visible) OVERRIDE {
    Window::Windows::const_iterator it = root_window_->children().begin();
    Window* last_window = NULL;
    for (; it != root_window_->children().end(); ++it) {
      if (*it == child && last_window) {
        if (!visible)
          root_window_->StackChildAbove(last_window, *it);
        else
          root_window_->StackChildAbove(*it, last_window);
        break;
      }
      last_window = *it;
    }
  }
  virtual void SetChildBounds(Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
    SetChildBoundsDirect(child, requested_bounds);
  }

  Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(StackingMadrigalLayoutManager);
};

class StackingMadrigalVisibilityClient : public aura::client::VisibilityClient {
 public:
  explicit StackingMadrigalVisibilityClient(Window* root_window)
      : ignored_window_(NULL) {
    aura::client::SetVisibilityClient(root_window, this);
  }
  virtual ~StackingMadrigalVisibilityClient() {
  }

  void set_ignored_window(Window* ignored_window) {
    ignored_window_ = ignored_window;
  }

 private:
  // Overridden from client::VisibilityClient:
  virtual void UpdateLayerVisibility(Window* window, bool visible) OVERRIDE {
    if (!visible) {
      if (window == ignored_window_)
        window->layer()->set_delegate(NULL);
      else
        window->layer()->SetVisible(visible);
    } else {
      window->layer()->SetVisible(visible);
    }
  }

  Window* ignored_window_;

  DISALLOW_COPY_AND_ASSIGN(StackingMadrigalVisibilityClient);
};

// This test attempts to reconstruct a circumstance that can happen when the
// aura client attempts to manipulate the visibility and delegate of a layer
// independent of window visibility.
// A use case is where the client attempts to keep a window visible onscreen
// even after code has called Hide() on the window. The use case for this would
// be that window hides are animated (e.g. the window fades out). To prevent
// spurious updating the client code may also clear window's layer's delegate,
// so that the window cannot attempt to paint or update it further. The window
// uses the presence of a NULL layer delegate as a signal in stacking to note
// that the window is being manipulated by such a use case and its stacking
// should not be adjusted.
// One issue that can arise when a window opens two transient children, and the
// first is hidden. Subsequent attempts to activate the transient parent can
// result in the transient parent being stacked above the second transient
// child. A fix is made to Window::StackAbove to prevent this, and this test
// verifies this fix.
TEST_F(TransientWindowManagerTest, StackingMadrigal) {
  new StackingMadrigalLayoutManager(root_window());
  StackingMadrigalVisibilityClient visibility_client(root_window());

  scoped_ptr<Window> window1(CreateTestWindowWithId(1, root_window()));
  scoped_ptr<Window> window11(CreateTransientChild(11, window1.get()));

  visibility_client.set_ignored_window(window11.get());

  window11->Show();
  window11->Hide();

  // As a transient, window11 should still be stacked above window1, even when
  // hidden.
  EXPECT_TRUE(aura::test::WindowIsAbove(window11.get(), window1.get()));
  EXPECT_TRUE(aura::test::LayerIsAbove(window11.get(), window1.get()));

  // A new transient should still be above window1.  It will appear behind
  // window11 because we don't stack windows on top of targets with NULL
  // delegates.
  scoped_ptr<Window> window12(CreateTransientChild(12, window1.get()));
  window12->Show();

  EXPECT_TRUE(aura::test::WindowIsAbove(window12.get(), window1.get()));
  EXPECT_TRUE(aura::test::LayerIsAbove(window12.get(), window1.get()));

  // In earlier versions of the StackChildAbove() method, attempting to stack
  // window1 above window12 at this point would actually restack the layers
  // resulting in window12's layer being below window1's layer (though the
  // windows themselves would still be correctly stacked, so events would pass
  // through.)
  root_window()->StackChildAbove(window1.get(), window12.get());

  // Both window12 and its layer should be stacked above window1.
  EXPECT_TRUE(aura::test::WindowIsAbove(window12.get(), window1.get()));
  EXPECT_TRUE(aura::test::LayerIsAbove(window12.get(), window1.get()));
}

// Test for an issue where attempting to stack a primary window on top of a
// transient with a NULL layer delegate causes that primary window to be moved,
// but the layer order not changed to match.  http://crbug.com/112562
TEST_F(TransientWindowManagerTest, StackOverClosingTransient) {
  scoped_ptr<Window> window1(CreateTestWindowWithId(1, root_window()));
  scoped_ptr<Window> transient1(CreateTransientChild(11, window1.get()));
  scoped_ptr<Window> window2(CreateTestWindowWithId(2, root_window()));
  scoped_ptr<Window> transient2(CreateTransientChild(21, window2.get()));

  // Both windows and layers are stacked in creation order.
  Window* root = root_window();
  ASSERT_EQ(4u, root->children().size());
  EXPECT_EQ(root->children()[0], window1.get());
  EXPECT_EQ(root->children()[1], transient1.get());
  EXPECT_EQ(root->children()[2], window2.get());
  EXPECT_EQ(root->children()[3], transient2.get());
  ASSERT_EQ(4u, root->layer()->children().size());
  EXPECT_EQ(root->layer()->children()[0], window1->layer());
  EXPECT_EQ(root->layer()->children()[1], transient1->layer());
  EXPECT_EQ(root->layer()->children()[2], window2->layer());
  EXPECT_EQ(root->layer()->children()[3], transient2->layer());
  EXPECT_EQ("1 11 2 21", ChildWindowIDsAsString(root_window()));

  // This brings window1 and its transient to the front.
  root->StackChildAtTop(window1.get());
  EXPECT_EQ("2 21 1 11", ChildWindowIDsAsString(root_window()));

  EXPECT_EQ(root->children()[0], window2.get());
  EXPECT_EQ(root->children()[1], transient2.get());
  EXPECT_EQ(root->children()[2], window1.get());
  EXPECT_EQ(root->children()[3], transient1.get());
  EXPECT_EQ(root->layer()->children()[0], window2->layer());
  EXPECT_EQ(root->layer()->children()[1], transient2->layer());
  EXPECT_EQ(root->layer()->children()[2], window1->layer());
  EXPECT_EQ(root->layer()->children()[3], transient1->layer());

  // Pretend we're closing the top-most transient, then bring window2 to the
  // front.  This mimics activating a browser window while the status bubble
  // is fading out.  The transient should stay topmost.
  transient1->layer()->set_delegate(NULL);
  root->StackChildAtTop(window2.get());

  EXPECT_EQ(root->children()[0], window1.get());
  EXPECT_EQ(root->children()[1], window2.get());
  EXPECT_EQ(root->children()[2], transient2.get());
  EXPECT_EQ(root->children()[3], transient1.get());
  EXPECT_EQ(root->layer()->children()[0], window1->layer());
  EXPECT_EQ(root->layer()->children()[1], window2->layer());
  EXPECT_EQ(root->layer()->children()[2], transient2->layer());
  EXPECT_EQ(root->layer()->children()[3], transient1->layer());

  // Close the transient.  Remaining windows are stable.
  transient1.reset();

  ASSERT_EQ(3u, root->children().size());
  EXPECT_EQ(root->children()[0], window1.get());
  EXPECT_EQ(root->children()[1], window2.get());
  EXPECT_EQ(root->children()[2], transient2.get());
  ASSERT_EQ(3u, root->layer()->children().size());
  EXPECT_EQ(root->layer()->children()[0], window1->layer());
  EXPECT_EQ(root->layer()->children()[1], window2->layer());
  EXPECT_EQ(root->layer()->children()[2], transient2->layer());

  // Open another window on top.
  scoped_ptr<Window> window3(CreateTestWindowWithId(3, root_window()));

  ASSERT_EQ(4u, root->children().size());
  EXPECT_EQ(root->children()[0], window1.get());
  EXPECT_EQ(root->children()[1], window2.get());
  EXPECT_EQ(root->children()[2], transient2.get());
  EXPECT_EQ(root->children()[3], window3.get());
  ASSERT_EQ(4u, root->layer()->children().size());
  EXPECT_EQ(root->layer()->children()[0], window1->layer());
  EXPECT_EQ(root->layer()->children()[1], window2->layer());
  EXPECT_EQ(root->layer()->children()[2], transient2->layer());
  EXPECT_EQ(root->layer()->children()[3], window3->layer());

  // Pretend we're closing the topmost non-transient window, then bring
  // window2 to the top.  It should not move.
  window3->layer()->set_delegate(NULL);
  root->StackChildAtTop(window2.get());

  ASSERT_EQ(4u, root->children().size());
  EXPECT_EQ(root->children()[0], window1.get());
  EXPECT_EQ(root->children()[1], window2.get());
  EXPECT_EQ(root->children()[2], transient2.get());
  EXPECT_EQ(root->children()[3], window3.get());
  ASSERT_EQ(4u, root->layer()->children().size());
  EXPECT_EQ(root->layer()->children()[0], window1->layer());
  EXPECT_EQ(root->layer()->children()[1], window2->layer());
  EXPECT_EQ(root->layer()->children()[2], transient2->layer());
  EXPECT_EQ(root->layer()->children()[3], window3->layer());

  // Bring window1 to the top.  It should move ahead of window2, but not
  // ahead of window3 (with NULL delegate).
  root->StackChildAtTop(window1.get());

  ASSERT_EQ(4u, root->children().size());
  EXPECT_EQ(root->children()[0], window2.get());
  EXPECT_EQ(root->children()[1], transient2.get());
  EXPECT_EQ(root->children()[2], window1.get());
  EXPECT_EQ(root->children()[3], window3.get());
  ASSERT_EQ(4u, root->layer()->children().size());
  EXPECT_EQ(root->layer()->children()[0], window2->layer());
  EXPECT_EQ(root->layer()->children()[1], transient2->layer());
  EXPECT_EQ(root->layer()->children()[2], window1->layer());
  EXPECT_EQ(root->layer()->children()[3], window3->layer());
}

// Verifies TransientWindowObserver is notified appropriately.
TEST_F(TransientWindowManagerTest, TransientWindowObserverNotified) {
  scoped_ptr<Window> parent(CreateTestWindowWithId(0, root_window()));
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, parent.get()));

  TestTransientWindowObserver test_observer;
  TransientWindowManager::Get(parent.get())->AddObserver(&test_observer);

  AddTransientChild(parent.get(), w1.get());
  EXPECT_EQ(1, test_observer.add_count());
  EXPECT_EQ(0, test_observer.remove_count());

  RemoveTransientChild(parent.get(), w1.get());
  EXPECT_EQ(1, test_observer.add_count());
  EXPECT_EQ(1, test_observer.remove_count());

  TransientWindowManager::Get(parent.get())->RemoveObserver(&test_observer);
}

}  // namespace wm
