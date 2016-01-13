// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/window_slider.h"

#include "base/bind.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/frame_time.h"

namespace content {

void DispatchEventDuringScrollCallback(ui::EventProcessor* dispatcher,
                                       ui::Event* event,
                                       ui::EventType type,
                                       const gfx::Vector2dF& delta) {
  if (type != ui::ET_GESTURE_SCROLL_UPDATE)
    return;
  ui::EventDispatchDetails details = dispatcher->OnEventFromSource(event);
  CHECK(!details.dispatcher_destroyed);
}

void ChangeSliderOwnerDuringScrollCallback(scoped_ptr<aura::Window>* window,
                                           WindowSlider* slider,
                                           ui::EventType type,
                                           const gfx::Vector2dF& delta) {
  if (type != ui::ET_GESTURE_SCROLL_UPDATE)
    return;
  aura::Window* new_window = new aura::Window(NULL);
  new_window->Init(aura::WINDOW_LAYER_TEXTURED);
  new_window->Show();
  slider->ChangeOwner(new_window);
  (*window)->parent()->AddChild(new_window);
  window->reset(new_window);
}

void ConfirmSlideDuringScrollCallback(WindowSlider* slider,
                                      ui::EventType type,
                                      const gfx::Vector2dF& delta) {
  static float total_delta_x = 0;
  if (type == ui::ET_GESTURE_SCROLL_BEGIN)
    total_delta_x = 0;

  if (type == ui::ET_GESTURE_SCROLL_UPDATE) {
    total_delta_x += delta.x();
    if (total_delta_x >= 70)
      EXPECT_TRUE(slider->IsSlideInProgress());
  } else {
    EXPECT_FALSE(slider->IsSlideInProgress());
  }
}

void ConfirmNoSlideDuringScrollCallback(WindowSlider* slider,
                                        ui::EventType type,
                                        const gfx::Vector2dF& delta) {
  EXPECT_FALSE(slider->IsSlideInProgress());
}

// The window delegate does not receive any events.
class NoEventWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  NoEventWindowDelegate() {
  }
  virtual ~NoEventWindowDelegate() {}

 private:
  // Overridden from aura::WindowDelegate:
  virtual bool HasHitTestMask() const OVERRIDE { return true; }

  DISALLOW_COPY_AND_ASSIGN(NoEventWindowDelegate);
};

class WindowSliderDelegateTest : public WindowSlider::Delegate {
 public:
  WindowSliderDelegateTest()
      : can_create_layer_(true),
        created_back_layer_(false),
        created_front_layer_(false),
        slide_completing_(false),
        slide_completed_(false),
        slide_aborted_(false),
        slider_destroyed_(false) {
  }
  virtual ~WindowSliderDelegateTest() {
    // Make sure slide_completed() gets called if slide_completing() was called.
    CHECK(!slide_completing_ || slide_completed_);
  }

  void Reset() {
    can_create_layer_ = true;
    created_back_layer_ = false;
    created_front_layer_ = false;
    slide_completing_ = false;
    slide_completed_ = false;
    slide_aborted_ = false;
    slider_destroyed_ = false;
  }

  void SetCanCreateLayer(bool can_create_layer) {
    can_create_layer_ = can_create_layer;
  }

  bool created_back_layer() const { return created_back_layer_; }
  bool created_front_layer() const { return created_front_layer_; }
  bool slide_completing() const { return slide_completing_; }
  bool slide_completed() const { return slide_completed_; }
  bool slide_aborted() const { return slide_aborted_; }
  bool slider_destroyed() const { return slider_destroyed_; }

 protected:
  ui::Layer* CreateLayerForTest() {
    CHECK(can_create_layer_);
    ui::Layer* layer = new ui::Layer(ui::LAYER_SOLID_COLOR);
    layer->SetColor(SK_ColorRED);
    return layer;
  }

  // Overridden from WindowSlider::Delegate:
  virtual ui::Layer* CreateBackLayer() OVERRIDE {
    if (!can_create_layer_)
      return NULL;
    created_back_layer_ = true;
    return CreateLayerForTest();
  }

  virtual ui::Layer* CreateFrontLayer() OVERRIDE {
    if (!can_create_layer_)
      return NULL;
    created_front_layer_ = true;
    return CreateLayerForTest();
  }

  virtual void OnWindowSlideCompleted(scoped_ptr<ui::Layer> layer) OVERRIDE {
    slide_completed_ = true;
  }

  virtual void OnWindowSlideCompleting() OVERRIDE {
    slide_completing_ = true;
  }

  virtual void OnWindowSlideAborted() OVERRIDE {
    slide_aborted_ = true;
  }

  virtual void OnWindowSliderDestroyed() OVERRIDE {
    slider_destroyed_ = true;
  }

 private:
  bool can_create_layer_;
  bool created_back_layer_;
  bool created_front_layer_;
  bool slide_completing_;
  bool slide_completed_;
  bool slide_aborted_;
  bool slider_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(WindowSliderDelegateTest);
};

// This delegate destroys the owner window when the slider is destroyed.
class WindowSliderDeleteOwnerOnDestroy : public WindowSliderDelegateTest {
 public:
  explicit WindowSliderDeleteOwnerOnDestroy(aura::Window* owner)
      : owner_(owner) {
  }
  virtual ~WindowSliderDeleteOwnerOnDestroy() {}

 private:
  // Overridden from WindowSlider::Delegate:
  virtual void OnWindowSliderDestroyed() OVERRIDE {
    WindowSliderDelegateTest::OnWindowSliderDestroyed();
    delete owner_;
  }

  aura::Window* owner_;
  DISALLOW_COPY_AND_ASSIGN(WindowSliderDeleteOwnerOnDestroy);
};

// This delegate destroyes the owner window when a slide is completed.
class WindowSliderDeleteOwnerOnComplete : public WindowSliderDelegateTest {
 public:
  explicit WindowSliderDeleteOwnerOnComplete(aura::Window* owner)
      : owner_(owner) {
  }
  virtual ~WindowSliderDeleteOwnerOnComplete() {}

 private:
  // Overridden from WindowSlider::Delegate:
  virtual void OnWindowSlideCompleted(scoped_ptr<ui::Layer> layer) OVERRIDE {
    WindowSliderDelegateTest::OnWindowSlideCompleted(layer.Pass());
    delete owner_;
  }

  aura::Window* owner_;
  DISALLOW_COPY_AND_ASSIGN(WindowSliderDeleteOwnerOnComplete);
};

typedef aura::test::AuraTestBase WindowSliderTest;

TEST_F(WindowSliderTest, WindowSlideUsingGesture) {
  scoped_ptr<aura::Window> window(CreateNormalWindow(0, root_window(), NULL));
  window->SetBounds(gfx::Rect(0, 0, 400, 400));
  WindowSliderDelegateTest slider_delegate;

  aura::test::EventGenerator generator(root_window());

  // Generate a horizontal overscroll.
  WindowSlider* slider =
      new WindowSlider(&slider_delegate, root_window(), window.get());
  generator.GestureScrollSequenceWithCallback(
      gfx::Point(10, 10),
      gfx::Point(180, 10),
      base::TimeDelta::FromMilliseconds(10),
      10,
      base::Bind(&ConfirmSlideDuringScrollCallback, slider));
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_completing());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
  EXPECT_FALSE(slider->IsSlideInProgress());
  slider_delegate.Reset();
  window->SetTransform(gfx::Transform());

  // Generate a horizontal overscroll in the reverse direction.
  generator.GestureScrollSequenceWithCallback(
      gfx::Point(180, 10),
      gfx::Point(10, 10),
      base::TimeDelta::FromMilliseconds(10),
      10,
      base::Bind(&ConfirmSlideDuringScrollCallback, slider));
  EXPECT_TRUE(slider_delegate.created_front_layer());
  EXPECT_TRUE(slider_delegate.slide_completing());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_back_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
  EXPECT_FALSE(slider->IsSlideInProgress());
  slider_delegate.Reset();

  // Generate a vertical overscroll.
  generator.GestureScrollSequenceWithCallback(
      gfx::Point(10, 10),
      gfx::Point(10, 80),
      base::TimeDelta::FromMilliseconds(10),
      10,
      base::Bind(&ConfirmNoSlideDuringScrollCallback, slider));
  EXPECT_FALSE(slider_delegate.created_back_layer());
  EXPECT_FALSE(slider_delegate.slide_completing());
  EXPECT_FALSE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider->IsSlideInProgress());
  slider_delegate.Reset();

  // Generate a horizontal scroll that starts overscroll, but doesn't scroll
  // enough to complete it.
  generator.GestureScrollSequenceWithCallback(
      gfx::Point(10, 10),
      gfx::Point(80, 10),
      base::TimeDelta::FromMilliseconds(10),
      10,
      base::Bind(&ConfirmSlideDuringScrollCallback, slider));
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_completing());
  EXPECT_FALSE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
  EXPECT_FALSE(slider->IsSlideInProgress());
  slider_delegate.Reset();

  // Destroy the window. This should destroy the slider.
  window.reset();
  EXPECT_TRUE(slider_delegate.slider_destroyed());
}

// Tests that the window slide is interrupted when a different type of event
// happens.
TEST_F(WindowSliderTest, WindowSlideIsCancelledOnEvent) {
  scoped_ptr<aura::Window> window(CreateNormalWindow(0, root_window(), NULL));
  window->SetBounds(gfx::Rect(0, 0, 400, 400));
  WindowSliderDelegateTest slider_delegate;

  ui::Event* events[] = {
    new ui::MouseEvent(ui::ET_MOUSE_MOVED,
                       gfx::Point(55, 10),
                       gfx::Point(55, 10),
                       0, 0),
    new ui::KeyEvent(ui::ET_KEY_PRESSED,
                     ui::VKEY_A,
                     0,
                     true),
    NULL
  };

  new WindowSlider(&slider_delegate, root_window(), window.get());
  for (int i = 0; events[i]; ++i) {
    // Generate a horizontal overscroll.
    aura::test::EventGenerator generator(root_window());
    generator.GestureScrollSequenceWithCallback(
        gfx::Point(10, 10),
        gfx::Point(80, 10),
        base::TimeDelta::FromMilliseconds(10),
        1,
        base::Bind(&DispatchEventDuringScrollCallback,
                   root_window()->GetHost()->event_processor(),
                   base::Owned(events[i])));
    EXPECT_TRUE(slider_delegate.created_back_layer());
    EXPECT_TRUE(slider_delegate.slide_aborted());
    EXPECT_FALSE(slider_delegate.created_front_layer());
    EXPECT_FALSE(slider_delegate.slide_completing());
    EXPECT_FALSE(slider_delegate.slide_completed());
    EXPECT_FALSE(slider_delegate.slider_destroyed());
    slider_delegate.Reset();
  }
  window.reset();
  EXPECT_TRUE(slider_delegate.slider_destroyed());
}

// Tests that the window slide can continue after it is interrupted by another
// event if the user continues scrolling.
TEST_F(WindowSliderTest, WindowSlideInterruptedThenContinues) {
  scoped_ptr<aura::Window> window(CreateNormalWindow(0, root_window(), NULL));
  window->SetBounds(gfx::Rect(0, 0, 400, 400));
  WindowSliderDelegateTest slider_delegate;

  ui::ScopedAnimationDurationScaleMode normal_duration_(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  animator->set_disable_timer_for_test(true);
  ui::LayerAnimatorTestController test_controller(animator);

  WindowSlider* slider =
      new WindowSlider(&slider_delegate, root_window(), window.get());

  ui::MouseEvent interrupt_event(ui::ET_MOUSE_MOVED,
                                 gfx::Point(55, 10),
                                 gfx::Point(55, 10),
                                 0, 0);

  aura::test::EventGenerator generator(root_window());

  // Start the scroll sequence. Scroll forward so that |window|'s layer is the
  // one animating.
  const int kTouchId = 5;
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED,
                       gfx::Point(10, 10),
                       kTouchId,
                       ui::EventTimeForNow());
  generator.Dispatch(&press);

  // First scroll event of the sequence.
  ui::TouchEvent move1(ui::ET_TOUCH_MOVED,
                       gfx::Point(100, 10),
                       kTouchId,
                       ui::EventTimeForNow());
  generator.Dispatch(&move1);
  EXPECT_TRUE(slider->IsSlideInProgress());
  EXPECT_FALSE(animator->is_animating());
  // Dispatch the event after the first scroll and confirm it interrupts the
  // scroll and starts  the "reset slide" animation.
  generator.Dispatch(&interrupt_event);
  EXPECT_TRUE(slider->IsSlideInProgress());
  EXPECT_TRUE(animator->is_animating());
  EXPECT_TRUE(slider_delegate.created_back_layer());
  // slide_aborted() should be false because the 'reset slide' animation
  // hasn't completed yet.
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_completing());
  EXPECT_FALSE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
  slider_delegate.Reset();

  // Second scroll event of the sequence.
  ui::TouchEvent move2(ui::ET_TOUCH_MOVED,
                       gfx::Point(200, 10),
                       kTouchId,
                       ui::EventTimeForNow());
  generator.Dispatch(&move2);
  // The second scroll should instantly cause the animation to complete.
  EXPECT_FALSE(animator->is_animating());
  EXPECT_FALSE(slider_delegate.created_back_layer());
  // The ResetScroll() animation was completed, so now slide_aborted()
  // should be true.
  EXPECT_TRUE(slider_delegate.slide_aborted());

  // Third scroll event of the sequence.
  ui::TouchEvent move3(ui::ET_TOUCH_MOVED,
                       gfx::Point(300, 10),
                       kTouchId,
                       ui::EventTimeForNow());
  generator.Dispatch(&move3);
  // The third scroll should re-start the sliding.
  EXPECT_TRUE(slider->IsSlideInProgress());
  EXPECT_TRUE(slider_delegate.created_back_layer());

  // Generate the release event, finishing the scroll sequence.
  ui::TouchEvent release(ui::ET_TOUCH_RELEASED,
                         gfx::Point(300, 10),
                         kTouchId,
                         ui::EventTimeForNow());
  generator.Dispatch(&release);
  // When the scroll gesture ends, the slide animation should start.
  EXPECT_TRUE(slider->IsSlideInProgress());
  EXPECT_TRUE(animator->is_animating());
  EXPECT_TRUE(slider_delegate.slide_completing());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.slider_destroyed());

  // Progress the animator to complete the slide animation.
  ui::ScopedLayerAnimationSettings settings(animator);
  base::TimeDelta duration = settings.GetTransitionDuration();
  test_controller.StartThreadedAnimationsIfNeeded();
  animator->Step(gfx::FrameTime::Now() + duration);

  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.slider_destroyed());

  window.reset();
  EXPECT_TRUE(slider_delegate.slider_destroyed());
}

// Tests that the slide works correctly when the owner of the window changes
// during the duration of the slide.
TEST_F(WindowSliderTest, OwnerWindowChangesDuringWindowSlide) {
  scoped_ptr<aura::Window> parent(CreateNormalWindow(0, root_window(), NULL));

  NoEventWindowDelegate window_delegate;
  window_delegate.set_window_component(HTNOWHERE);
  scoped_ptr<aura::Window> window(CreateNormalWindow(1, parent.get(),
                                                     &window_delegate));

  WindowSliderDelegateTest slider_delegate;
  scoped_ptr<WindowSlider> slider(
      new WindowSlider(&slider_delegate, parent.get(), window.get()));

  // Generate a horizontal scroll, and change the owner in the middle of the
  // scroll.
  aura::test::EventGenerator generator(root_window());
  aura::Window* old_window = window.get();
  generator.GestureScrollSequenceWithCallback(
      gfx::Point(10, 10),
      gfx::Point(80, 10),
      base::TimeDelta::FromMilliseconds(10),
      1,
      base::Bind(&ChangeSliderOwnerDuringScrollCallback,
                 base::Unretained(&window),
                 slider.get()));
  aura::Window* new_window = window.get();
  EXPECT_NE(old_window, new_window);

  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_completing());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
}

// If the delegate doesn't create the layer to show while sliding, WindowSlider
// shouldn't start the slide or change delegate's state in any way in response
// to user input.
TEST_F(WindowSliderTest, NoSlideWhenLayerCantBeCreated) {
  scoped_ptr<aura::Window> window(CreateNormalWindow(0, root_window(), NULL));
  window->SetBounds(gfx::Rect(0, 0, 400, 400));
  WindowSliderDelegateTest slider_delegate;
  slider_delegate.SetCanCreateLayer(false);
  WindowSlider* slider =
      new WindowSlider(&slider_delegate, root_window(), window.get());

  aura::test::EventGenerator generator(root_window());

  // No slide in progress should be reported during scroll since the layer
  // wasn't created.
  generator.GestureScrollSequenceWithCallback(
      gfx::Point(10, 10),
      gfx::Point(180, 10),
      base::TimeDelta::FromMilliseconds(10),
      1,
      base::Bind(&ConfirmNoSlideDuringScrollCallback, slider));

  EXPECT_FALSE(slider_delegate.created_back_layer());
  EXPECT_FALSE(slider_delegate.slide_completing());
  EXPECT_FALSE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
  window->SetTransform(gfx::Transform());

  slider_delegate.SetCanCreateLayer(true);
  generator.GestureScrollSequenceWithCallback(
      gfx::Point(10, 10),
      gfx::Point(180, 10),
      base::TimeDelta::FromMilliseconds(10),
      10,
      base::Bind(&ConfirmSlideDuringScrollCallback, slider));
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_completing());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.slider_destroyed());

  window.reset();
  EXPECT_TRUE(slider_delegate.slider_destroyed());
}

// Tests that the owner window can be destroyed from |OnWindowSliderDestroyed()|
// delegate callback without causing a crash.
TEST_F(WindowSliderTest, OwnerIsDestroyedOnSliderDestroy) {
  size_t child_windows = root_window()->children().size();
  aura::Window* window = CreateNormalWindow(0, root_window(), NULL);
  window->SetBounds(gfx::Rect(0, 0, 400, 400));
  EXPECT_EQ(child_windows + 1, root_window()->children().size());

  WindowSliderDeleteOwnerOnDestroy slider_delegate(window);
  aura::test::EventGenerator generator(root_window());

  // Generate a horizontal overscroll.
  scoped_ptr<WindowSlider> slider(
      new WindowSlider(&slider_delegate, root_window(), window));
  generator.GestureScrollSequence(gfx::Point(10, 10),
                                  gfx::Point(180, 10),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_completing());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.slider_destroyed());

  slider.reset();
  // Destroying the slider would have destroyed |window| too. So |window| should
  // not need to be destroyed here.
  EXPECT_EQ(child_windows, root_window()->children().size());
}

// Tests that the owner window can be destroyed from |OnWindowSlideComplete()|
// delegate callback without causing a crash.
TEST_F(WindowSliderTest, OwnerIsDestroyedOnSlideComplete) {
  size_t child_windows = root_window()->children().size();
  aura::Window* window = CreateNormalWindow(0, root_window(), NULL);
  window->SetBounds(gfx::Rect(0, 0, 400, 400));
  EXPECT_EQ(child_windows + 1, root_window()->children().size());

  WindowSliderDeleteOwnerOnComplete slider_delegate(window);
  aura::test::EventGenerator generator(root_window());

  // Generate a horizontal overscroll.
  new WindowSlider(&slider_delegate, root_window(), window);
  generator.GestureScrollSequence(gfx::Point(10, 10),
                                  gfx::Point(180, 10),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_completing());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_TRUE(slider_delegate.slider_destroyed());

  // Destroying the slider would have destroyed |window| too. So |window| should
  // not need to be destroyed here.
  EXPECT_EQ(child_windows, root_window()->children().size());
}

// Test the scenario when two swipe gesture occur quickly one after another so
// that the second swipe occurs while the transition animation triggered by the
// first swipe is in progress.
// The second swipe is supposed to instantly complete the animation caused by
// the first swipe, ask the delegate to create a new layer, and animate it.
TEST_F(WindowSliderTest, SwipeDuringSwipeAnimation) {
  scoped_ptr<aura::Window> window(CreateNormalWindow(0, root_window(), NULL));
  window->SetBounds(gfx::Rect(0, 0, 400, 400));
  WindowSliderDelegateTest slider_delegate;
  new WindowSlider(&slider_delegate, root_window(), window.get());

  ui::ScopedAnimationDurationScaleMode normal_duration_(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  animator->set_disable_timer_for_test(true);
  ui::LayerAnimatorTestController test_controller(animator);

  aura::test::EventGenerator generator(root_window());

  // Swipe forward so that |window|'s layer is the one animating.
  generator.GestureScrollSequence(
      gfx::Point(10, 10),
      gfx::Point(180, 10),
      base::TimeDelta::FromMilliseconds(10),
      2);
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_TRUE(slider_delegate.slide_completing());
  EXPECT_FALSE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
  ui::ScopedLayerAnimationSettings settings(animator);
  base::TimeDelta duration = settings.GetTransitionDuration();
  test_controller.StartThreadedAnimationsIfNeeded();
  base::TimeTicks start_time1 =  gfx::FrameTime::Now();

  animator->Step(start_time1 + duration / 2);
  EXPECT_FALSE(slider_delegate.slide_completed());
  slider_delegate.Reset();
  // Generate another horizontal swipe while the animation from the previous
  // swipe is in progress.
  generator.GestureScrollSequence(
      gfx::Point(10, 10),
      gfx::Point(180, 10),
      base::TimeDelta::FromMilliseconds(10),
      2);
  // Performing the second swipe should instantly complete the slide started
  // by the first swipe and create a new layer.
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_TRUE(slider_delegate.slide_completing());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
  test_controller.StartThreadedAnimationsIfNeeded();
  base::TimeTicks start_time2 =  gfx::FrameTime::Now();
  slider_delegate.Reset();
  animator->Step(start_time2 + duration);
  // The animation for the second slide should now be completed.
  EXPECT_TRUE(slider_delegate.slide_completed());
  slider_delegate.Reset();

  window.reset();
  EXPECT_TRUE(slider_delegate.slider_destroyed());
}

}  // namespace content
