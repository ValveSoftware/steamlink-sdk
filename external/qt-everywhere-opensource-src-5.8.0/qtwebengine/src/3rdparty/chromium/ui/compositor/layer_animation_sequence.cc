// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animation_sequence.h"

#include <algorithm>
#include <iterator>

#include "base/trace_event/trace_event.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_id_provider.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ui {

LayerAnimationSequence::LayerAnimationSequence()
    : properties_(LayerAnimationElement::UNKNOWN),
      is_cyclic_(false),
      last_element_(0),
      waiting_for_group_start_(false),
      animation_group_id_(0),
      last_progressed_fraction_(0.0),
      weak_ptr_factory_(this) {
}

LayerAnimationSequence::LayerAnimationSequence(LayerAnimationElement* element)
    : properties_(LayerAnimationElement::UNKNOWN),
      is_cyclic_(false),
      last_element_(0),
      waiting_for_group_start_(false),
      animation_group_id_(0),
      last_progressed_fraction_(0.0),
      weak_ptr_factory_(this) {
  AddElement(element);
}

LayerAnimationSequence::~LayerAnimationSequence() {
  FOR_EACH_OBSERVER(LayerAnimationObserver,
                    observers_,
                    DetachedFromSequence(this, true));
}

void LayerAnimationSequence::Start(LayerAnimationDelegate* delegate) {
  DCHECK(start_time_ != base::TimeTicks());
  last_progressed_fraction_ = 0.0;
  if (elements_.empty())
    return;

  NotifyStarted();

  elements_[0]->set_requested_start_time(start_time_);
  elements_[0]->Start(delegate, animation_group_id_);
}

void LayerAnimationSequence::Progress(base::TimeTicks now,
                                      LayerAnimationDelegate* delegate) {
  DCHECK(start_time_ != base::TimeTicks());
  bool redraw_required = false;

  if (elements_.empty())
    return;

  if (last_element_ == 0)
    last_start_ = start_time_;

  size_t current_index = last_element_ % elements_.size();
  base::TimeDelta element_duration;
  while (is_cyclic_ || last_element_ < elements_.size()) {
    elements_[current_index]->set_requested_start_time(last_start_);
    if (!elements_[current_index]->IsFinished(now, &element_duration))
      break;

    // Let the element we're passing finish.
    if (elements_[current_index]->ProgressToEnd(delegate))
      redraw_required = true;
    last_start_ += element_duration;
    ++last_element_;
    last_progressed_fraction_ =
        elements_[current_index]->last_progressed_fraction();
    current_index = last_element_ % elements_.size();
  }

  if (is_cyclic_ || last_element_ < elements_.size()) {
    if (!elements_[current_index]->Started()) {
      animation_group_id_ = cc::AnimationIdProvider::NextGroupId();
      elements_[current_index]->Start(delegate, animation_group_id_);
    }
    base::WeakPtr<LayerAnimationSequence> alive(weak_ptr_factory_.GetWeakPtr());
    if (elements_[current_index]->Progress(now, delegate))
      redraw_required = true;
    if (!alive)
      return;
    last_progressed_fraction_ =
        elements_[current_index]->last_progressed_fraction();
  }

  // Since the delegate may be deleted due to the notifications below, it is
  // important that we schedule a draw before sending them.
  if (redraw_required)
    delegate->ScheduleDrawForAnimation();

  if (!is_cyclic_ && last_element_ == elements_.size()) {
    last_element_ = 0;
    waiting_for_group_start_ = false;
    animation_group_id_ = 0;
    NotifyEnded();
  }
}

bool LayerAnimationSequence::IsFinished(base::TimeTicks time) {
  if (is_cyclic_ || waiting_for_group_start_)
    return false;

  if (elements_.empty())
    return true;

  if (last_element_ == 0)
    last_start_ = start_time_;

  base::TimeTicks current_start = last_start_;
  size_t current_index = last_element_;
  base::TimeDelta element_duration;
  while (current_index < elements_.size()) {
    elements_[current_index]->set_requested_start_time(current_start);
    if (!elements_[current_index]->IsFinished(time, &element_duration))
      break;

    current_start += element_duration;
    ++current_index;
  }

  return (current_index == elements_.size());
}

void LayerAnimationSequence::ProgressToEnd(LayerAnimationDelegate* delegate) {
  bool redraw_required = false;

  if (elements_.empty())
    return;

  size_t current_index = last_element_ % elements_.size();
  while (current_index < elements_.size()) {
    if (elements_[current_index]->ProgressToEnd(delegate))
      redraw_required = true;
    last_progressed_fraction_ =
        elements_[current_index]->last_progressed_fraction();
    ++current_index;
    ++last_element_;
  }

  if (redraw_required)
    delegate->ScheduleDrawForAnimation();

  if (!is_cyclic_) {
    last_element_ = 0;
    waiting_for_group_start_ = false;
    animation_group_id_ = 0;
    NotifyEnded();
  }
}

void LayerAnimationSequence::GetTargetValue(
    LayerAnimationElement::TargetValue* target) const {
  if (is_cyclic_)
    return;

  for (size_t i = last_element_; i < elements_.size(); ++i)
    elements_[i]->GetTargetValue(target);
}

void LayerAnimationSequence::Abort(LayerAnimationDelegate* delegate) {
  size_t current_index = last_element_ % elements_.size();
  while (current_index < elements_.size()) {
    elements_[current_index]->Abort(delegate);
    ++current_index;
  }
  last_element_ = 0;
  waiting_for_group_start_ = false;
  NotifyAborted();
}

void LayerAnimationSequence::AddElement(LayerAnimationElement* element) {
  properties_ |= element->properties();
  elements_.push_back(make_linked_ptr(element));
}

bool LayerAnimationSequence::HasConflictingProperty(
    LayerAnimationElement::AnimatableProperties other) const {
  return (properties_ & other) != LayerAnimationElement::UNKNOWN;
}

bool LayerAnimationSequence::IsFirstElementThreaded() const {
  if (!elements_.empty())
    return elements_[0]->IsThreaded();

  return false;
}

void LayerAnimationSequence::AddObserver(LayerAnimationObserver* observer) {
  if (!observers_.HasObserver(observer)) {
    observers_.AddObserver(observer);
    observer->AttachedToSequence(this);
  }
}

void LayerAnimationSequence::RemoveObserver(LayerAnimationObserver* observer) {
  observers_.RemoveObserver(observer);
  observer->DetachedFromSequence(this, true);
}

void LayerAnimationSequence::OnThreadedAnimationStarted(
    base::TimeTicks monotonic_time,
    cc::TargetProperty::Type target_property,
    int group_id) {
  if (elements_.empty() || group_id != animation_group_id_)
    return;

  size_t current_index = last_element_ % elements_.size();
  LayerAnimationElement::AnimatableProperties element_properties =
    elements_[current_index]->properties();
  LayerAnimationElement::AnimatableProperty event_property =
      LayerAnimationElement::ToAnimatableProperty(target_property);
  DCHECK(element_properties & event_property);
  elements_[current_index]->set_effective_start_time(monotonic_time);
}

void LayerAnimationSequence::OnScheduled() {
  NotifyScheduled();
}

void LayerAnimationSequence::OnAnimatorDestroyed() {
  if (observers_.might_have_observers()) {
    base::ObserverListBase<LayerAnimationObserver>::Iterator it(&observers_);
    LayerAnimationObserver* obs;
    while ((obs = it.GetNext()) != NULL) {
      if (!obs->RequiresNotificationWhenAnimatorDestroyed()) {
        // Remove the observer, but do not allow notifications to be sent.
        observers_.RemoveObserver(obs);
        obs->DetachedFromSequence(this, false);
      }
    }
  }
}

size_t LayerAnimationSequence::size() const {
  return elements_.size();
}

LayerAnimationElement* LayerAnimationSequence::FirstElement() const {
  if (elements_.empty()) {
    return NULL;
  }

  return elements_[0].get();
}

void LayerAnimationSequence::NotifyScheduled() {
  FOR_EACH_OBSERVER(LayerAnimationObserver,
                    observers_,
                    OnLayerAnimationScheduled(this));
}

void LayerAnimationSequence::NotifyStarted() {
  FOR_EACH_OBSERVER(LayerAnimationObserver, observers_,
                    OnLayerAnimationStarted(this));
}

void LayerAnimationSequence::NotifyEnded() {
  FOR_EACH_OBSERVER(LayerAnimationObserver,
                    observers_,
                    OnLayerAnimationEnded(this));
}

void LayerAnimationSequence::NotifyAborted() {
  FOR_EACH_OBSERVER(LayerAnimationObserver,
                    observers_,
                    OnLayerAnimationAborted(this));
}

LayerAnimationElement* LayerAnimationSequence::CurrentElement() const {
  if (elements_.empty())
    return NULL;

  size_t current_index = last_element_ % elements_.size();
  return elements_[current_index].get();
}

}  // namespace ui
