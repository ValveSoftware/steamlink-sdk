// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_recognizer_impl.h"

#include <limits>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_switches.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_configuration.h"
#include "ui/events/gestures/gesture_sequence.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/events/gestures/unified_gesture_detector_enabled.h"

namespace ui {

namespace {

template <typename T>
void TransferConsumer(GestureConsumer* current_consumer,
                      GestureConsumer* new_consumer,
                      std::map<GestureConsumer*, T>* map) {
  if (map->count(current_consumer)) {
    (*map)[new_consumer] = (*map)[current_consumer];
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

void TransferTouchIdToConsumerMap(
    GestureConsumer* old_consumer,
    GestureConsumer* new_consumer,
    GestureRecognizerImpl::TouchIdToConsumerMap* map) {
  for (GestureRecognizerImpl::TouchIdToConsumerMap::iterator i = map->begin();
       i != map->end(); ++i) {
    if (i->second == old_consumer)
      i->second = new_consumer;
  }
}

GestureProviderAura* CreateGestureProvider(GestureProviderAuraClient* client) {
  return new GestureProviderAura(client);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, public:

GestureRecognizerImpl::GestureRecognizerImpl() {
  use_unified_gesture_detector_ = IsUnifiedGestureDetectorEnabled();
}

GestureRecognizerImpl::~GestureRecognizerImpl() {
  STLDeleteValues(&consumer_sequence_);
  STLDeleteValues(&consumer_gesture_provider_);
}

// Checks if this finger is already down, if so, returns the current target.
// Otherwise, returns NULL.
GestureConsumer* GestureRecognizerImpl::GetTouchLockedTarget(
    const TouchEvent& event) {
  return touch_id_target_[event.touch_id()];
}

GestureConsumer* GestureRecognizerImpl::GetTargetForGestureEvent(
    const GestureEvent& event) {
  GestureConsumer* target = NULL;
  int touch_id = event.GetLowestTouchId();
  target = touch_id_target_for_gestures_[touch_id];
  return target;
}

GestureConsumer* GestureRecognizerImpl::GetTargetForLocation(
    const gfx::PointF& location, int source_device_id) {
  const int max_distance =
      GestureConfiguration::max_separation_for_gesture_touches_in_pixels();

  if (!use_unified_gesture_detector_) {
    const GesturePoint* closest_point = NULL;
    int64 closest_distance_squared = 0;
    std::map<GestureConsumer*, GestureSequence*>::iterator i;
    for (i = consumer_sequence_.begin(); i != consumer_sequence_.end(); ++i) {
      const GesturePoint* points = i->second->points();
      for (int j = 0; j < GestureSequence::kMaxGesturePoints; ++j) {
        if (!points[j].in_use() ||
            source_device_id != points[j].source_device_id()) {
          continue;
        }
        gfx::Vector2dF delta = points[j].last_touch_position() - location;
        // Relative distance is all we need here, so LengthSquared() is
        // appropriate, and cheaper than Length().
        int64 distance_squared = delta.LengthSquared();
        if (!closest_point || distance_squared < closest_distance_squared) {
          closest_point = &points[j];
          closest_distance_squared = distance_squared;
        }
      }
    }

    if (closest_distance_squared < max_distance * max_distance && closest_point)
      return touch_id_target_[closest_point->touch_id()];
    else
      return NULL;
  } else {
    gfx::PointF closest_point;
    int closest_touch_id;
    float closest_distance_squared = std::numeric_limits<float>::infinity();

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
        float distance_squared = (point - location).LengthSquared();
        if (distance_squared < closest_distance_squared) {
          closest_point = point;
          closest_touch_id = pointer_state.GetPointerId(j);
          closest_distance_squared = distance_squared;
        }
      }
    }

    if (closest_distance_squared < max_distance * max_distance)
      return touch_id_target_[closest_touch_id];
    else
      return NULL;
  }
}

void GestureRecognizerImpl::TransferEventsTo(GestureConsumer* current_consumer,
                                             GestureConsumer* new_consumer) {
  // Send cancel to all those save |new_consumer| and |current_consumer|.
  // Don't send a cancel to |current_consumer|, unless |new_consumer| is NULL.
  // Dispatching a touch-cancel event can end up altering |touch_id_target_|
  // (e.g. when the target of the event is destroyed, causing it to be removed
  // from |touch_id_target_| in |CleanupStateForConsumer()|). So create a list
  // of the touch-ids that need to be cancelled, and dispatch the cancel events
  // for them at the end.
  std::vector<std::pair<int, GestureConsumer*> > ids;
  for (TouchIdToConsumerMap::iterator i = touch_id_target_.begin();
       i != touch_id_target_.end(); ++i) {
    if (i->second && i->second != new_consumer &&
        (i->second != current_consumer || new_consumer == NULL) &&
        i->second) {
      ids.push_back(std::make_pair(i->first, i->second));
    }
  }

  CancelTouches(&ids);

  // Transfer events from |current_consumer| to |new_consumer|.
  if (current_consumer && new_consumer) {
    TransferTouchIdToConsumerMap(current_consumer, new_consumer,
                                 &touch_id_target_);
    TransferTouchIdToConsumerMap(current_consumer, new_consumer,
                                 &touch_id_target_for_gestures_);
    if (!use_unified_gesture_detector_)
      TransferConsumer(current_consumer, new_consumer, &consumer_sequence_);
    else
      TransferConsumer(
          current_consumer, new_consumer, &consumer_gesture_provider_);
  }
}

bool GestureRecognizerImpl::GetLastTouchPointForTarget(
    GestureConsumer* consumer,
    gfx::PointF* point) {
  if (!use_unified_gesture_detector_) {
    if (consumer_sequence_.count(consumer) == 0)
      return false;
    *point = consumer_sequence_[consumer]->last_touch_location();
    return true;
  } else {
    if (consumer_gesture_provider_.count(consumer) == 0)
      return false;
    const MotionEvent& pointer_state =
        consumer_gesture_provider_[consumer]->pointer_state();
    *point = gfx::PointF(pointer_state.GetX(), pointer_state.GetY());
    return true;
  }
}

bool GestureRecognizerImpl::CancelActiveTouches(GestureConsumer* consumer) {
  std::vector<std::pair<int, GestureConsumer*> > ids;
  for (TouchIdToConsumerMap::const_iterator i = touch_id_target_.begin();
       i != touch_id_target_.end(); ++i) {
    if (i->second == consumer)
      ids.push_back(std::make_pair(i->first, i->second));
  }
  bool cancelled_touch = !ids.empty();
  CancelTouches(&ids);
  return cancelled_touch;
}

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, protected:

GestureSequence* GestureRecognizerImpl::CreateSequence(
    GestureSequenceDelegate* delegate) {
  return new GestureSequence(delegate);
}

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, private:

GestureSequence* GestureRecognizerImpl::GetGestureSequenceForConsumer(
    GestureConsumer* consumer) {
  GestureSequence* gesture_sequence = consumer_sequence_[consumer];
  if (!gesture_sequence) {
    gesture_sequence = CreateSequence(this);
    consumer_sequence_[consumer] = gesture_sequence;
  }
  return gesture_sequence;
}

GestureProviderAura* GestureRecognizerImpl::GetGestureProviderForConsumer(
    GestureConsumer* consumer) {
  GestureProviderAura* gesture_provider = consumer_gesture_provider_[consumer];
  if (!gesture_provider) {
    gesture_provider = CreateGestureProvider(this);
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
    if (target)
      touch_id_target_for_gestures_[event.touch_id()] = target;
  }
}

void GestureRecognizerImpl::CancelTouches(
    std::vector<std::pair<int, GestureConsumer*> >* touches) {
  while (!touches->empty()) {
    int touch_id = touches->begin()->first;
    GestureConsumer* target = touches->begin()->second;
    TouchEvent touch_event(ui::ET_TOUCH_CANCELLED, gfx::PointF(0, 0),
                           ui::EF_IS_SYNTHESIZED, touch_id,
                           ui::EventTimeForNow(), 0.0f, 0.0f, 0.0f, 0.0f);
    GestureEventHelper* helper = FindDispatchHelperForConsumer(target);
    if (helper)
      helper->DispatchCancelTouchEvent(&touch_event);
    touches->erase(touches->begin());
  }
}

void GestureRecognizerImpl::DispatchGestureEvent(GestureEvent* event) {
  GestureConsumer* consumer = GetTargetForGestureEvent(*event);
  if (consumer) {
    GestureEventHelper* helper = FindDispatchHelperForConsumer(consumer);
    if (helper)
      helper->DispatchGestureEvent(event);
  }
}

ScopedVector<GestureEvent>* GestureRecognizerImpl::ProcessTouchEventForGesture(
    const TouchEvent& event,
    ui::EventResult result,
    GestureConsumer* target) {
  SetupTargets(event, target);

  if (!use_unified_gesture_detector_) {
    GestureSequence* gesture_sequence = GetGestureSequenceForConsumer(target);
    return gesture_sequence->ProcessTouchEventForGesture(event, result);
  } else {
    GestureProviderAura* gesture_provider =
        GetGestureProviderForConsumer(target);
    // TODO(tdresser) - detect gestures eagerly.
    if (!(result & ER_CONSUMED)) {
      if (gesture_provider->OnTouchEvent(event)) {
        gesture_provider->OnTouchEventAck(result != ER_UNHANDLED);
        return gesture_provider->GetAndResetPendingGestures();
      }
    }
    return NULL;
  }
}

bool GestureRecognizerImpl::CleanupStateForConsumer(
    GestureConsumer* consumer) {
  bool state_cleaned_up = false;

  if (!use_unified_gesture_detector_) {
    if (consumer_sequence_.count(consumer)) {
      state_cleaned_up = true;
      delete consumer_sequence_[consumer];
      consumer_sequence_.erase(consumer);
    }
  } else {
    if (consumer_gesture_provider_.count(consumer)) {
      state_cleaned_up = true;
      delete consumer_gesture_provider_[consumer];
      consumer_gesture_provider_.erase(consumer);
    }
  }

  state_cleaned_up |= RemoveConsumerFromMap(consumer, &touch_id_target_);
  state_cleaned_up |=
      RemoveConsumerFromMap(consumer, &touch_id_target_for_gestures_);
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

void GestureRecognizerImpl::DispatchPostponedGestureEvent(GestureEvent* event) {
  DispatchGestureEvent(event);
}

void GestureRecognizerImpl::OnGestureEvent(GestureEvent* event) {
  DispatchGestureEvent(event);
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
