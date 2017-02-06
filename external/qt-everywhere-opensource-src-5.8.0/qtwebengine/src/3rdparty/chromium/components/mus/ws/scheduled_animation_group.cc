// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/scheduled_animation_group.h"

#include <set>

#include "components/mus/ws/server_window.h"

using mus::mojom::AnimationProperty;

namespace mus {
namespace ws {
namespace {

using Sequences = std::vector<ScheduledAnimationSequence>;

// Gets the value of |property| from |window| into |value|.
void GetValueFromWindow(const ServerWindow* window,
                        AnimationProperty property,
                        ScheduledAnimationValue* value) {
  switch (property) {
    case AnimationProperty::NONE:
      NOTREACHED();
      break;
    case AnimationProperty::OPACITY:
      value->float_value = window->opacity();
      break;
    case AnimationProperty::TRANSFORM:
      value->transform = window->transform();
      break;
  }
}

// Sets the value of |property| from |value| into |window|.
void SetWindowPropertyFromValue(ServerWindow* window,
                                AnimationProperty property,
                                const ScheduledAnimationValue& value) {
  switch (property) {
    case AnimationProperty::NONE:
      break;
    case AnimationProperty::OPACITY:
      window->SetOpacity(value.float_value);
      break;
    case AnimationProperty::TRANSFORM:
      window->SetTransform(value.transform);
      break;
  }
}

// Sets the value of |property| into |window| between two points.
void SetWindowPropertyFromValueBetween(ServerWindow* window,
                                       AnimationProperty property,
                                       double value,
                                       gfx::Tween::Type tween_type,
                                       const ScheduledAnimationValue& start,
                                       const ScheduledAnimationValue& target) {
  const double tween_value = gfx::Tween::CalculateValue(tween_type, value);
  switch (property) {
    case AnimationProperty::NONE:
      break;
    case AnimationProperty::OPACITY:
      window->SetOpacity(gfx::Tween::FloatValueBetween(
          tween_value, start.float_value, target.float_value));
      break;
    case AnimationProperty::TRANSFORM:
      window->SetTransform(gfx::Tween::TransformValueBetween(
          tween_value, start.transform, target.transform));
      break;
  }
}

// TODO(mfomitchev): Use struct traits for this?
gfx::Tween::Type AnimationTypeToTweenType(mojom::AnimationTweenType type) {
  switch (type) {
    case mojom::AnimationTweenType::LINEAR:
      return gfx::Tween::LINEAR;
    case mojom::AnimationTweenType::EASE_IN:
      return gfx::Tween::EASE_IN;
    case mojom::AnimationTweenType::EASE_OUT:
      return gfx::Tween::EASE_OUT;
    case mojom::AnimationTweenType::EASE_IN_OUT:
      return gfx::Tween::EASE_IN_OUT;
  }
  return gfx::Tween::LINEAR;
}

void ConvertToScheduledValue(const mojom::AnimationValue& transport_value,
                             ScheduledAnimationValue* value) {
  value->float_value = transport_value.float_value;
  value->transform = transport_value.transform;
}

void ConvertToScheduledElement(const mojom::AnimationElement& transport_element,
                               ScheduledAnimationElement* element) {
  element->property = transport_element.property;
  element->duration =
      base::TimeDelta::FromMicroseconds(transport_element.duration);
  element->tween_type = AnimationTypeToTweenType(transport_element.tween_type);
  if (transport_element.property != AnimationProperty::NONE) {
    if (transport_element.start_value.get()) {
      element->is_start_valid = true;
      ConvertToScheduledValue(*transport_element.start_value,
                              &(element->start_value));
    } else {
      element->is_start_valid = false;
    }
    ConvertToScheduledValue(*transport_element.target_value,
                            &(element->target_value));
  }
}

bool IsAnimationValueValid(AnimationProperty property,
                           const mojom::AnimationValue& value) {
  bool result;
  switch (property) {
    case AnimationProperty::NONE:
      NOTREACHED();
      return false;
    case AnimationProperty::OPACITY:
      result = value.float_value >= 0.f && value.float_value <= 1.f;
      DVLOG_IF(1, !result) << "Invalid AnimationValue for opacity: "
                           << value.float_value;
      return result;
    case AnimationProperty::TRANSFORM:
      return true;
  }
  return false;
}

bool IsAnimationElementValid(const mojom::AnimationElement& element) {
  if (element.property == AnimationProperty::NONE)
    return true;  // None is a pause and doesn't need any values.
  if (element.start_value.get() &&
      !IsAnimationValueValid(element.property, *element.start_value))
    return false;
  // For all other properties we require a target.
  return element.target_value.get() &&
         IsAnimationValueValid(element.property, *element.target_value);
}

bool IsAnimationSequenceValid(const mojom::AnimationSequence& sequence) {
  if (sequence.elements.size() == 0) {
    DVLOG(1) << "Empty or null AnimationSequence - invalid.";
    return false;
  }

  for (size_t i = 0; i < sequence.elements.size(); ++i) {
    if (!IsAnimationElementValid(*sequence.elements[i]))
      return false;
  }
  return true;
}

bool IsAnimationGroupValid(const mojom::AnimationGroup& transport_group) {
  if (transport_group.sequences.size() == 0) {
    DVLOG(1) << "Empty or null AnimationGroup - invalid; window_id="
             << transport_group.window_id;
    return false;
  }
  for (size_t i = 0; i < transport_group.sequences.size(); ++i) {
    if (!IsAnimationSequenceValid(*transport_group.sequences[i]))
      return false;
  }
  return true;
}

// If the start value for |element| isn't valid, the value for the property
// is obtained from |window| and placed into |element|.
void GetStartValueFromWindowIfNecessary(const ServerWindow* window,
                                        ScheduledAnimationElement* element) {
  if (element->property != AnimationProperty::NONE &&
      !element->is_start_valid) {
    GetValueFromWindow(window, element->property, &(element->start_value));
  }
}

void GetScheduledAnimationProperties(const Sequences& sequences,
                                     std::set<AnimationProperty>* properties) {
  for (const ScheduledAnimationSequence& sequence : sequences) {
    for (const ScheduledAnimationElement& element : sequence.elements)
      properties->insert(element.property);
  }
}

// Set |window|'s specified |property| to the value resulting from running all
// the |sequences|.
void SetPropertyToTargetProperty(ServerWindow* window,
                                 mojom::AnimationProperty property,
                                 const Sequences& sequences) {
  // NOTE: this doesn't deal with |cycle_count| quite right, but I'm honestly
  // not sure we really want to support the same property in multiple sequences
  // animating at once so I'm not dealing.
  base::TimeDelta max_end_duration;
  std::unique_ptr<ScheduledAnimationValue> value;
  for (const ScheduledAnimationSequence& sequence : sequences) {
    base::TimeDelta duration;
    for (const ScheduledAnimationElement& element : sequence.elements) {
      if (element.property != property)
        continue;

      duration += element.duration;
      if (duration > max_end_duration) {
        max_end_duration = duration;
        value.reset(new ScheduledAnimationValue(element.target_value));
      }
    }
  }
  if (value.get())
    SetWindowPropertyFromValue(window, property, *value);
}

void ConvertSequenceToScheduled(
    const mojom::AnimationSequence& transport_sequence,
    base::TimeTicks now,
    ScheduledAnimationSequence* sequence) {
  sequence->run_until_stopped = transport_sequence.cycle_count == 0u;
  sequence->cycle_count = transport_sequence.cycle_count;
  DCHECK_NE(0u, transport_sequence.elements.size());
  sequence->elements.resize(transport_sequence.elements.size());

  base::TimeTicks element_start_time = now;
  for (size_t i = 0; i < transport_sequence.elements.size(); ++i) {
    ConvertToScheduledElement(*(transport_sequence.elements[i].get()),
                              &(sequence->elements[i]));
    sequence->elements[i].start_time = element_start_time;
    sequence->duration += sequence->elements[i].duration;
    element_start_time += sequence->elements[i].duration;
  }
}

bool AdvanceSequence(ServerWindow* window,
                     ScheduledAnimationSequence* sequence,
                     base::TimeTicks now) {
  ScheduledAnimationElement* element =
      &(sequence->elements[sequence->current_index]);
  while (element->start_time + element->duration < now) {
    SetWindowPropertyFromValue(window, element->property,
                               element->target_value);
    if (++sequence->current_index == sequence->elements.size()) {
      if (!sequence->run_until_stopped && --sequence->cycle_count == 0) {
        return false;
      }

      sequence->current_index = 0;
    }
    sequence->elements[sequence->current_index].start_time =
        element->start_time + element->duration;
    element = &(sequence->elements[sequence->current_index]);
    GetStartValueFromWindowIfNecessary(window, element);

    // It's possible for the delta between now and |last_tick_time_| to be very
    // big (could happen if machine sleeps and is woken up much later). Normally
    // the repeat count is smallish, so we don't bother optimizing it. OTOH if
    // a sequence repeats forever we optimize it lest we get stuck in this loop
    // for a very long time.
    if (sequence->run_until_stopped && sequence->current_index == 0) {
      element->start_time =
          now - base::TimeDelta::FromMicroseconds(
                    (now - element->start_time).InMicroseconds() %
                    sequence->duration.InMicroseconds());
    }
  }
  return true;
}

}  // namespace

ScheduledAnimationValue::ScheduledAnimationValue() : float_value(0) {}
ScheduledAnimationValue::~ScheduledAnimationValue() {}

ScheduledAnimationElement::ScheduledAnimationElement()
    : property(AnimationProperty::OPACITY),
      tween_type(gfx::Tween::EASE_IN),
      is_start_valid(false) {}
ScheduledAnimationElement::ScheduledAnimationElement(
    const ScheduledAnimationElement& other) = default;
ScheduledAnimationElement::~ScheduledAnimationElement() {}

ScheduledAnimationSequence::ScheduledAnimationSequence()
    : run_until_stopped(false), cycle_count(0), current_index(0u) {}
ScheduledAnimationSequence::ScheduledAnimationSequence(
    const ScheduledAnimationSequence& other) = default;
ScheduledAnimationSequence::~ScheduledAnimationSequence() {}

ScheduledAnimationGroup::~ScheduledAnimationGroup() {}

// static
std::unique_ptr<ScheduledAnimationGroup> ScheduledAnimationGroup::Create(
    ServerWindow* window,
    base::TimeTicks now,
    uint32_t id,
    const mojom::AnimationGroup& transport_group) {
  if (!IsAnimationGroupValid(transport_group))
    return nullptr;

  std::unique_ptr<ScheduledAnimationGroup> group(
      new ScheduledAnimationGroup(window, id, now));
  group->sequences_.resize(transport_group.sequences.size());
  for (size_t i = 0; i < transport_group.sequences.size(); ++i) {
    const mojom::AnimationSequence& transport_sequence(
        *(transport_group.sequences[i]));
    DCHECK_NE(0u, transport_sequence.elements.size());
    ConvertSequenceToScheduled(transport_sequence, now, &group->sequences_[i]);
  }
  return group;
}

void ScheduledAnimationGroup::ObtainStartValues() {
  for (ScheduledAnimationSequence& sequence : sequences_)
    GetStartValueFromWindowIfNecessary(window_, &(sequence.elements[0]));
}

void ScheduledAnimationGroup::SetValuesToTargetValuesForPropertiesNotIn(
    const ScheduledAnimationGroup& other) {
  std::set<AnimationProperty> our_properties;
  GetScheduledAnimationProperties(sequences_, &our_properties);

  std::set<AnimationProperty> other_properties;
  GetScheduledAnimationProperties(other.sequences_, &other_properties);

  for (AnimationProperty property : our_properties) {
    if (other_properties.count(property) == 0 &&
        property != AnimationProperty::NONE) {
      SetPropertyToTargetProperty(window_, property, sequences_);
    }
  }
}

bool ScheduledAnimationGroup::Tick(base::TimeTicks time) {
  for (Sequences::iterator i = sequences_.begin(); i != sequences_.end();) {
    if (!AdvanceSequence(window_, &(*i), time)) {
      i = sequences_.erase(i);
      continue;
    }
    const ScheduledAnimationElement& active_element(
        i->elements[i->current_index]);
    const double percent =
        (time - active_element.start_time).InMillisecondsF() /
        active_element.duration.InMillisecondsF();
    SetWindowPropertyFromValueBetween(
        window_, active_element.property, percent, active_element.tween_type,
        active_element.start_value, active_element.target_value);
    ++i;
  }
  return sequences_.empty();
}

ScheduledAnimationGroup::ScheduledAnimationGroup(ServerWindow* window,
                                                 uint32_t id,
                                                 base::TimeTicks time_scheduled)
    : window_(window), id_(id), time_scheduled_(time_scheduled) {}

}  // namespace ws
}  // namespace mus
