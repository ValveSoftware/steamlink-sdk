// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_targeter.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/events/test/test_event_processor.h"
#include "ui/events/test/test_event_target.h"

typedef std::vector<std::string> HandlerSequenceRecorder;

namespace ui {
namespace test {

class EventProcessorTest : public testing::Test {
 public:
  EventProcessorTest() {}
  virtual ~EventProcessorTest() {}

  // testing::Test:
  virtual void SetUp() OVERRIDE {
    processor_.SetRoot(scoped_ptr<EventTarget>(new TestEventTarget()));
    root()->SetEventTargeter(make_scoped_ptr(new EventTargeter()));
  }

  TestEventTarget* root() {
    return static_cast<TestEventTarget*>(processor_.GetRootTarget());
  }

  void DispatchEvent(Event* event) {
    processor_.OnEventFromSource(event);
  }

 protected:
  TestEventProcessor processor_;

  DISALLOW_COPY_AND_ASSIGN(EventProcessorTest);
};

TEST_F(EventProcessorTest, Basic) {
  scoped_ptr<TestEventTarget> child(new TestEventTarget());
  root()->AddChild(child.Pass());

  MouseEvent mouse(ET_MOUSE_MOVED, gfx::Point(10, 10), gfx::Point(10, 10),
                   EF_NONE, EF_NONE);
  DispatchEvent(&mouse);
  EXPECT_TRUE(root()->child_at(0)->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(root()->DidReceiveEvent(ET_MOUSE_MOVED));

  root()->RemoveChild(root()->child_at(0));
  DispatchEvent(&mouse);
  EXPECT_TRUE(root()->DidReceiveEvent(ET_MOUSE_MOVED));
}

template<typename T>
class BoundsEventTargeter : public EventTargeter {
 public:
  virtual ~BoundsEventTargeter() {}

 protected:
  virtual bool SubtreeShouldBeExploredForEvent(
      EventTarget* target, const LocatedEvent& event) OVERRIDE {
    T* t = static_cast<T*>(target);
    return (t->bounds().Contains(event.location()));
  }
};

class BoundsTestTarget : public TestEventTarget {
 public:
  BoundsTestTarget() {}
  virtual ~BoundsTestTarget() {}

  void set_bounds(gfx::Rect rect) { bounds_ = rect; }
  gfx::Rect bounds() const { return bounds_; }

  static void ConvertPointToTarget(BoundsTestTarget* source,
                                   BoundsTestTarget* target,
                                   gfx::Point* location) {
    gfx::Vector2d vector;
    if (source->Contains(target)) {
      for (; target && target != source;
           target = static_cast<BoundsTestTarget*>(target->parent())) {
        vector += target->bounds().OffsetFromOrigin();
      }
      *location -= vector;
    } else if (target->Contains(source)) {
      for (; source && source != target;
           source = static_cast<BoundsTestTarget*>(source->parent())) {
        vector += source->bounds().OffsetFromOrigin();
      }
      *location += vector;
    } else {
      NOTREACHED();
    }
  }

 private:
  // EventTarget:
  virtual void ConvertEventToTarget(EventTarget* target,
                                    LocatedEvent* event) OVERRIDE {
    event->ConvertLocationToTarget(this,
                                   static_cast<BoundsTestTarget*>(target));
  }

  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(BoundsTestTarget);
};

TEST_F(EventProcessorTest, Bounds) {
  scoped_ptr<BoundsTestTarget> parent(new BoundsTestTarget());
  scoped_ptr<BoundsTestTarget> child(new BoundsTestTarget());
  scoped_ptr<BoundsTestTarget> grandchild(new BoundsTestTarget());

  parent->set_bounds(gfx::Rect(0, 0, 30, 30));
  child->set_bounds(gfx::Rect(5, 5, 20, 20));
  grandchild->set_bounds(gfx::Rect(5, 5, 5, 5));

  child->AddChild(scoped_ptr<TestEventTarget>(grandchild.Pass()));
  parent->AddChild(scoped_ptr<TestEventTarget>(child.Pass()));
  root()->AddChild(scoped_ptr<TestEventTarget>(parent.Pass()));

  ASSERT_EQ(1u, root()->child_count());
  ASSERT_EQ(1u, root()->child_at(0)->child_count());
  ASSERT_EQ(1u, root()->child_at(0)->child_at(0)->child_count());

  TestEventTarget* parent_r = root()->child_at(0);
  TestEventTarget* child_r = parent_r->child_at(0);
  TestEventTarget* grandchild_r = child_r->child_at(0);

  // Dispatch a mouse event that falls on the parent, but not on the child. When
  // the default event-targeter used, the event will still reach |grandchild|,
  // because the default targeter does not look at the bounds.
  MouseEvent mouse(ET_MOUSE_MOVED, gfx::Point(1, 1), gfx::Point(1, 1), EF_NONE,
                   EF_NONE);
  DispatchEvent(&mouse);
  EXPECT_FALSE(root()->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(parent_r->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(child_r->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_TRUE(grandchild_r->DidReceiveEvent(ET_MOUSE_MOVED));
  grandchild_r->ResetReceivedEvents();

  // Now install a targeter on the parent that looks at the bounds and makes
  // sure the event reaches the target only if the location of the event within
  // the bounds of the target.
  MouseEvent mouse2(ET_MOUSE_MOVED, gfx::Point(1, 1), gfx::Point(1, 1), EF_NONE,
                    EF_NONE);
  parent_r->SetEventTargeter(scoped_ptr<EventTargeter>(
      new BoundsEventTargeter<BoundsTestTarget>()));
  DispatchEvent(&mouse2);
  EXPECT_FALSE(root()->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_TRUE(parent_r->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(child_r->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(grandchild_r->DidReceiveEvent(ET_MOUSE_MOVED));
  parent_r->ResetReceivedEvents();

  MouseEvent second(ET_MOUSE_MOVED, gfx::Point(12, 12), gfx::Point(12, 12),
                    EF_NONE, EF_NONE);
  DispatchEvent(&second);
  EXPECT_FALSE(root()->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(parent_r->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(child_r->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_TRUE(grandchild_r->DidReceiveEvent(ET_MOUSE_MOVED));
}

class IgnoreEventTargeter : public EventTargeter {
 public:
  IgnoreEventTargeter() {}
  virtual ~IgnoreEventTargeter() {}

 private:
  // EventTargeter:
  virtual bool SubtreeShouldBeExploredForEvent(
      EventTarget* target, const LocatedEvent& event) OVERRIDE {
    return false;
  }
};

// Verifies that the EventTargeter installed on an EventTarget can dictate
// whether the target itself can process an event.
TEST_F(EventProcessorTest, TargeterChecksOwningEventTarget) {
  scoped_ptr<TestEventTarget> child(new TestEventTarget());
  root()->AddChild(child.Pass());

  MouseEvent mouse(ET_MOUSE_MOVED, gfx::Point(10, 10), gfx::Point(10, 10),
                   EF_NONE, EF_NONE);
  DispatchEvent(&mouse);
  EXPECT_TRUE(root()->child_at(0)->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(root()->DidReceiveEvent(ET_MOUSE_MOVED));
  root()->child_at(0)->ResetReceivedEvents();

  // Install an event handler on |child| which always prevents the target from
  // receiving event.
  root()->child_at(0)->SetEventTargeter(
      scoped_ptr<EventTargeter>(new IgnoreEventTargeter()));
  MouseEvent mouse2(ET_MOUSE_MOVED, gfx::Point(10, 10), gfx::Point(10, 10),
                    EF_NONE, EF_NONE);
  DispatchEvent(&mouse2);
  EXPECT_FALSE(root()->child_at(0)->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_TRUE(root()->DidReceiveEvent(ET_MOUSE_MOVED));
}

// An EventTargeter which is used to allow a bubbling behaviour in event
// dispatch: if an event is not handled after being dispatched to its
// initial target, the event is dispatched to the next-best target as
// specified by FindNextBestTarget().
class BubblingEventTargeter : public EventTargeter {
 public:
  explicit BubblingEventTargeter(TestEventTarget* initial_target)
    : initial_target_(initial_target) {}
  virtual ~BubblingEventTargeter() {}

 private:
  // EventTargeter:
  virtual EventTarget* FindTargetForEvent(EventTarget* root,
                                          Event* event) OVERRIDE {
    return initial_target_;
  }

  virtual EventTarget* FindNextBestTarget(EventTarget* previous_target,
                                          Event* event) OVERRIDE {
    return previous_target->GetParentTarget();
  }

  TestEventTarget* initial_target_;

  DISALLOW_COPY_AND_ASSIGN(BubblingEventTargeter);
};

// Tests that unhandled events are correctly dispatched to the next-best
// target as decided by the BubblingEventTargeter.
TEST_F(EventProcessorTest, DispatchToNextBestTarget) {
  scoped_ptr<TestEventTarget> child(new TestEventTarget());
  scoped_ptr<TestEventTarget> grandchild(new TestEventTarget());

  root()->SetEventTargeter(
      scoped_ptr<EventTargeter>(new BubblingEventTargeter(grandchild.get())));
  child->AddChild(grandchild.Pass());
  root()->AddChild(child.Pass());

  ASSERT_EQ(1u, root()->child_count());
  ASSERT_EQ(1u, root()->child_at(0)->child_count());
  ASSERT_EQ(0u, root()->child_at(0)->child_at(0)->child_count());

  TestEventTarget* child_r = root()->child_at(0);
  TestEventTarget* grandchild_r = child_r->child_at(0);

  // When the root has a BubblingEventTargeter installed, events targeted
  // at the grandchild target should be dispatched to all three targets.
  KeyEvent key_event(ET_KEY_PRESSED, VKEY_ESCAPE, 0, false);
  DispatchEvent(&key_event);
  EXPECT_TRUE(root()->DidReceiveEvent(ET_KEY_PRESSED));
  EXPECT_TRUE(child_r->DidReceiveEvent(ET_KEY_PRESSED));
  EXPECT_TRUE(grandchild_r->DidReceiveEvent(ET_KEY_PRESSED));
  root()->ResetReceivedEvents();
  child_r->ResetReceivedEvents();
  grandchild_r->ResetReceivedEvents();

  // Add a pre-target handler on the child of the root that will mark the event
  // as handled. No targets in the hierarchy should receive the event.
  TestEventHandler handler;
  child_r->AddPreTargetHandler(&handler);
  key_event = KeyEvent(ET_KEY_PRESSED, VKEY_ESCAPE, 0, false);
  DispatchEvent(&key_event);
  EXPECT_FALSE(root()->DidReceiveEvent(ET_KEY_PRESSED));
  EXPECT_FALSE(child_r->DidReceiveEvent(ET_KEY_PRESSED));
  EXPECT_FALSE(grandchild_r->DidReceiveEvent(ET_KEY_PRESSED));
  EXPECT_EQ(1, handler.num_key_events());
  handler.Reset();

  // Add a post-target handler on the child of the root that will mark the event
  // as handled. Only the grandchild (the initial target) should receive the
  // event.
  child_r->RemovePreTargetHandler(&handler);
  child_r->AddPostTargetHandler(&handler);
  key_event = KeyEvent(ET_KEY_PRESSED, VKEY_ESCAPE, 0, false);
  DispatchEvent(&key_event);
  EXPECT_FALSE(root()->DidReceiveEvent(ET_KEY_PRESSED));
  EXPECT_FALSE(child_r->DidReceiveEvent(ET_KEY_PRESSED));
  EXPECT_TRUE(grandchild_r->DidReceiveEvent(ET_KEY_PRESSED));
  EXPECT_EQ(1, handler.num_key_events());
  handler.Reset();
  grandchild_r->ResetReceivedEvents();
  child_r->RemovePostTargetHandler(&handler);

  // Mark the event as handled when it reaches the EP_TARGET phase of
  // dispatch at the child of the root. The child and grandchild
  // targets should both receive the event, but the root should not.
  child_r->set_mark_events_as_handled(true);
  key_event = KeyEvent(ET_KEY_PRESSED, VKEY_ESCAPE, 0, false);
  DispatchEvent(&key_event);
  EXPECT_FALSE(root()->DidReceiveEvent(ET_KEY_PRESSED));
  EXPECT_TRUE(child_r->DidReceiveEvent(ET_KEY_PRESSED));
  EXPECT_TRUE(grandchild_r->DidReceiveEvent(ET_KEY_PRESSED));
  root()->ResetReceivedEvents();
  child_r->ResetReceivedEvents();
  grandchild_r->ResetReceivedEvents();
  child_r->set_mark_events_as_handled(false);
}

// Tests that unhandled events are seen by the correct sequence of
// targets, pre-target handlers, and post-target handlers when
// a BubblingEventTargeter is installed on the root target.
TEST_F(EventProcessorTest, HandlerSequence) {
  scoped_ptr<TestEventTarget> child(new TestEventTarget());
  scoped_ptr<TestEventTarget> grandchild(new TestEventTarget());

  root()->SetEventTargeter(
      scoped_ptr<EventTargeter>(new BubblingEventTargeter(grandchild.get())));
  child->AddChild(grandchild.Pass());
  root()->AddChild(child.Pass());

  ASSERT_EQ(1u, root()->child_count());
  ASSERT_EQ(1u, root()->child_at(0)->child_count());
  ASSERT_EQ(0u, root()->child_at(0)->child_at(0)->child_count());

  TestEventTarget* child_r = root()->child_at(0);
  TestEventTarget* grandchild_r = child_r->child_at(0);

  HandlerSequenceRecorder recorder;
  root()->set_target_name("R");
  root()->set_recorder(&recorder);
  child_r->set_target_name("C");
  child_r->set_recorder(&recorder);
  grandchild_r->set_target_name("G");
  grandchild_r->set_recorder(&recorder);

  TestEventHandler pre_root;
  pre_root.set_handler_name("PreR");
  pre_root.set_recorder(&recorder);
  root()->AddPreTargetHandler(&pre_root);

  TestEventHandler pre_child;
  pre_child.set_handler_name("PreC");
  pre_child.set_recorder(&recorder);
  child_r->AddPreTargetHandler(&pre_child);

  TestEventHandler pre_grandchild;
  pre_grandchild.set_handler_name("PreG");
  pre_grandchild.set_recorder(&recorder);
  grandchild_r->AddPreTargetHandler(&pre_grandchild);

  TestEventHandler post_root;
  post_root.set_handler_name("PostR");
  post_root.set_recorder(&recorder);
  root()->AddPostTargetHandler(&post_root);

  TestEventHandler post_child;
  post_child.set_handler_name("PostC");
  post_child.set_recorder(&recorder);
  child_r->AddPostTargetHandler(&post_child);

  TestEventHandler post_grandchild;
  post_grandchild.set_handler_name("PostG");
  post_grandchild.set_recorder(&recorder);
  grandchild_r->AddPostTargetHandler(&post_grandchild);

  MouseEvent mouse(ET_MOUSE_MOVED, gfx::Point(10, 10), gfx::Point(10, 10),
                   EF_NONE, EF_NONE);
  DispatchEvent(&mouse);

  std::string expected[] = { "PreR", "PreC", "PreG", "G", "PostG", "PostC",
      "PostR", "PreR", "PreC", "C", "PostC", "PostR", "PreR", "R", "PostR" };
  EXPECT_EQ(std::vector<std::string>(
      expected, expected + arraysize(expected)), recorder);
}

}  // namespace test
}  // namespace ui
