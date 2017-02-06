// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_recognizer_impl.h"

#include <stddef.h>

#include <limits>
#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_switches.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gestures/gesture_types.h"

namespace ui {

namespace {

template <typename T>
void TransferConsumer(GestureConsumer* current_consumer,
                      GestureConsumer* new_consumer,
                      std::map<GestureConsumer*, T>* map) {
  if (map->count(current_consumer)) {
    (*map)[new_consumer] = (*map)[current_consumer];
    (*map)[new_consumer]->set_gesture_consumer(new_consumer);
    map->erase(current_consumer);
  }
}

bool RemoveConsumerFromMap(GestureConsumer* consumer,
                           GestureRecognizerImpl::TouchIdToConsumerMap* map) {
  bool consumer_removed = false;
  for (GestureRecognizerImpl::TouchIdToConsumerMap::iterator i = map->begin();
       i != map->end();) {
    if (i->second == consumer) {
      map->erase(i++);
      consumer_removed = true;
    } else {
      ++i;
    }
  }
  return consumer_removed;
}

GestureProviderAura* CreateGestureProvider(GestureConsumer* consumer,
                                           GestureProviderAuraClient* client) {
  return new GestureProviderAura(consumer, client);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, public:

GestureRecognizerImpl::GestureRecognizerImpl() {
}

GestureRecognizerImpl::~GestureRecognizerImpl() {
  STLDeleteValues(&consumer_gesture_provider_);
}

// Checks if this finger is already down, if so, returns the current target.
// Otherwise, returns NULL.
GestureConsumer* GestureRecognizerImpl::GetTouchLockedTarget(
    const TouchEvent& event) {
  return touch_id_target_[event.touch_id()];
}

GestureConsumer* GestureRecognizerImpl::GetTargetForLocation(
    const gfx::PointF& location, int source_device_id) {
  const float max_distance =
      GestureConfiguration::GetInstance()
          ->max_separation_for_gesture_touches_in_pixels();

  gfx::PointF closest_point;
  int closest_touch_id = 0;
  double closest_distance_squared = std::numeric_limits<double>::infinity();

  std::map<GestureConsumer*, GestureProviderAura*>::iterator i;
  for (i = consumer_gesture_provider_.begin();
       i != consumer_gesture_provider_.end();
       ++i) {
    const MotionEventAura& pointer_state = i->second->pointer_state();
    for (size_t j = 0; j < pointer_state.GetPointerCount(); ++j) {
      if (source_device_id != pointer_state.GetSourceDeviceId(j))
        continue;
      gfx::PointF point(pointer_state.GetX(j), pointer_state.GetY(j));
      // Relative distance is all we need here, so LengthSquared() is
      // appropriate, and cheaper than Length().
      double distance_squared = (point - location).LengthSquared();
      if (distance_squared < closest_distance_squared) {
        closest_point = point;
        closest_touch_id = pointer_state.GetPointerId(j);
        closest_distance_squared = distance_squared;
      }
    }
  }

  if (closest_distance_squared < max_distance * max_distance)
    return touch_id_target_[closest_touch_id];
  return NULL;
}

void GestureRecognizerImpl::CancelActiveTouchesExcept(
    GestureConsumer* not_cancelled) {
  for (const auto& consumer_provider : consumer_gesture_provider_) {
    if (consumer_provider.first == not_cancelled)
      continue;
    CancelActiveTouches(consumer_provider.first);
  }
}

void GestureRecognizerImpl::TransferEventsTo(
    GestureConsumer* current_consumer,
    GestureConsumer* new_consumer,
    ShouldCancelTouches should_cancel_touches) {
  // This method transfers the gesture stream from |current_consumer| to
  // |new_consumer|. If |should_cancel_touches| is Cancel, it ensures that both
  // consumers retain a touch event stream which is reasonably valid. In order
  // to do this we
  // - record what pointers are currently down on |current_consumer|
  // - cancel touches on consumers other than |current_consumer|
  // - move the gesture provider from |current_consumer| to |new_consumer|
  // - if |should_cancel_touches|
  //     - synchronize the state of the new gesture provider associated with
  //       current_consumer with with the touch state of the consumer itself via
  //       OnTouchEnter.
  //     - synthesize touch cancels on |current_consumer|.
  // - retarget the pointers that were previously targeted to
  //   |current_consumer| to |new_consumer|.
  // NOTE: This currently doesn't synthesize touch press events on
  // |new_consumer|, so the event stream it sees is still invalid.
  DCHECK(current_consumer);
  DCHECK(new_consumer);
  GestureEventHelper* helper = FindDispatchHelperForConsumer(current_consumer);

  std::vector<int> touchids_targeted_at_current;

  for (const auto& touch_id_target: touch_id_target_) {
    if (touch_id_target.second == current_consumer)
      touchids_targeted_at_current.push_back(touch_id_target.first);
  }

  CancelActiveTouchesExcept(current_consumer);

  std::vector<std::unique_ptr<TouchEvent>> cancelling_touches =
      GetEventPerPointForConsumer(current_consumer, ET_TOUCH_CANCELLED);

  TransferConsumer(current_consumer, new_consumer, &consumer_gesture_provider_);

  // We're now in a situation where current_consumer has no gesture recognizer,
  // but has some pointers down which need cancelling. In order to ensure that
  // the GR sees a valid event stream, inform it of these pointers via
  // OnTouchEnter, and then synthesize a touch cancel per pointer.
  if (should_cancel_touches ==
          GestureRecognizer::ShouldCancelTouches::Cancel &&
      helper) {
    GestureProviderAura* gesture_provider =
        GetGestureProviderForConsumer(current_consumer);

    for (std::unique_ptr<TouchEvent>& event : cancelling_touches) {
      gesture_provider->OnTouchEnter(event->touch_id(), event->x(), event->y());
      helper->DispatchSyntheticTouchEvent(event.get());
    }
  }

  for (int touch_id : touchids_targeted_at_current)
    touch_id_target_[touch_id] = new_consumer;
}

bool GestureRecognizerImpl::GetLastTouchPointForTarget(
    GestureConsumer* consumer,
    gfx::PointF* point) {
    if (consumer_gesture_provider_.count(consumer) == 0)
      return false;
    const MotionEvent& pointer_state =
        consumer_gesture_provider_[consumer]->pointer_state();
    if (!pointer_state.GetPointerCount())
      return false;
    *point = gfx::PointF(pointer_state.GetX(), pointer_state.GetY());
    return true;
}

std::vector<std::unique_ptr<TouchEvent>>
GestureRecognizerImpl::GetEventPerPointForConsumer(GestureConsumer* consumer,
                                                   EventType type) {
  std::vector<std::unique_ptr<TouchEvent>> cancelling_touches;
  if (consumer_gesture_provider_.count(consumer) == 0)
    return cancelling_touches;
  const MotionEventAura& pointer_state =
      consumer_gesture_provider_[consumer]->pointer_state();
  if (pointer_state.GetPointerCount() == 0)
    return cancelling_touches;
  for (size_t i = 0; i < pointer_state.GetPointerCount(); ++i) {
    std::unique_ptr<TouchEvent> touch_event(new TouchEvent(
        type, gfx::Point(), EF_IS_SYNTHESIZED, pointer_state.GetPointerId(i),
        EventTimeForNow(), 0.0f, 0.0f, 0.0f, 0.0f));
    gfx::PointF point(pointer_state.GetX(i), pointer_state.GetY(i));
    touch_event->set_location_f(point);
    touch_event->set_root_location_f(point);
    cancelling_touches.push_back(std::move(touch_event));
  }
  return cancelling_touches;
}

bool GestureRecognizerImpl::CancelActiveTouches(GestureConsumer* consumer) {
  GestureEventHelper* helper =
      FindDispatchHelperForConsumer(consumer);

  if (!helper)
    return false;

  std::vector<std::unique_ptr<TouchEvent>> cancelling_touches =
      GetEventPerPointForConsumer(consumer, ET_TOUCH_CANCELLED);
  for (const std::unique_ptr<TouchEvent>& cancelling_touch : cancelling_touches)
    helper->DispatchSyntheticTouchEvent(cancelling_touch.get());
  return cancelling_touches.size() > 0U;
}

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, private:

GestureProviderAura* GestureRecognizerImpl::GetGestureProviderForConsumer(
    GestureConsumer* consumer) {
  GestureProviderAura* gesture_provider = consumer_gesture_provider_[consumer];
  if (!gesture_provider) {
    gesture_provider = CreateGestureProvider(consumer, this);
    consumer_gesture_provider_[consumer] = gesture_provider;
  }
  return gesture_provider;
}

void GestureRecognizerImpl::SetupTargets(const TouchEvent& event,
                                         GestureConsumer* target) {
  if (event.type() == ui::ET_TOUCH_RELEASED ||
      event.type() == ui::ET_TOUCH_CANCELLED) {
    touch_id_target_.erase(event.touch_id());
  } else if (event.type() == ui::ET_TOUCH_PRESSED) {
    touch_id_target_[event.touch_id()] = target;
  }
}

void GestureRecognizerImpl::DispatchGestureEvent(
    GestureConsumer* raw_input_consumer,
    GestureEvent* event) {
  if (raw_input_consumer) {
    GestureEventHelper* helper =
        FindDispatchHelperForConsumer(raw_input_consumer);
    if (helper)
      helper->DispatchGestureEvent(raw_input_consumer, event);
  }
}

bool GestureRecognizerImpl::ProcessTouchEventPreDispatch(
    TouchEvent* event,
    GestureConsumer* consumer) {
  SetupTargets(*event, consumer);

  if (event->result() & ER_CONSUMED)
    return false;

  GestureProviderAura* gesture_provider =
      GetGestureProviderForConsumer(consumer);
  return gesture_provider->OnTouchEvent(event);
}

GestureRecognizer::Gestures* GestureRecognizerImpl::AckTouchEvent(
    uint32_t unique_event_id,
    ui::EventResult result,
    GestureConsumer* consumer) {
  GestureProviderAura* gesture_provider =
      GetGestureProviderForConsumer(consumer);
  gesture_provider->OnTouchEventAck(unique_event_id, result != ER_UNHANDLED);
  return gesture_provider->GetAndResetPendingGestures();
}

bool GestureRecognizerImpl::CleanupStateForConsumer(
    GestureConsumer* consumer) {
  bool state_cleaned_up = false;

  if (consumer_gesture_provider_.count(consumer)) {
    state_cleaned_up = true;
    delete consumer_gesture_provider_[consumer];
    consumer_gesture_provider_.erase(consumer);
  }

  state_cleaned_up |= RemoveConsumerFromMap(consumer, &touch_id_target_);
  return state_cleaned_up;
}

void GestureRecognizerImpl::AddGestureEventHelper(GestureEventHelper* helper) {
  helpers_.push_back(helper);
}

void GestureRecognizerImpl::RemoveGestureEventHelper(
    GestureEventHelper* helper) {
  std::vector<GestureEventHelper*>::iterator it = std::find(helpers_.begin(),
      helpers_.end(), helper);
  if (it != helpers_.end())
    helpers_.erase(it);
}

void GestureRecognizerImpl::OnGestureEvent(GestureConsumer* raw_input_consumer,
                                           GestureEvent* event) {
  DispatchGestureEvent(raw_input_consumer, event);
}

GestureEventHelper* GestureRecognizerImpl::FindDispatchHelperForConsumer(
    GestureConsumer* consumer) {
  std::vector<GestureEventHelper*>::iterator it;
  for (it = helpers_.begin(); it != helpers_.end(); ++it) {
    if ((*it)->CanDispatchToConsumer(consumer))
      return (*it);
  }
  return NULL;
}

// GestureRecognizer, static
GestureRecognizer* GestureRecognizer::Create() {
  return new GestureRecognizerImpl();
}

static GestureRecognizerImpl* g_gesture_recognizer_instance = NULL;

// GestureRecognizer, static
GestureRecognizer* GestureRecognizer::Get() {
  if (!g_gesture_recognizer_instance)
    g_gesture_recognizer_instance = new GestureRecognizerImpl();
  return g_gesture_recognizer_instance;
}

// GestureRecognizer, static
void GestureRecognizer::Reset() {
  delete g_gesture_recognizer_instance;
  g_gesture_recognizer_instance = NULL;
}

void SetGestureRecognizerForTesting(GestureRecognizer* gesture_recognizer) {
  // Transfer helpers to the new GR.
  std::vector<GestureEventHelper*>& helpers =
      g_gesture_recognizer_instance->helpers();
  std::vector<GestureEventHelper*>::iterator it;
  for (it = helpers.begin(); it != helpers.end(); ++it)
    gesture_recognizer->AddGestureEventHelper(*it);

  helpers.clear();
  g_gesture_recognizer_instance =
      static_cast<GestureRecognizerImpl*>(gesture_recognizer);
}

}  // namespace ui
