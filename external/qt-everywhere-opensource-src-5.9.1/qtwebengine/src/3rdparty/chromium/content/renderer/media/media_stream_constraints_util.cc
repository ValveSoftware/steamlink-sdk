// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_constraints_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

namespace {

template <typename P, typename T>
bool ScanConstraintsForExactValue(const blink::WebMediaConstraints& constraints,
                                  P picker,
                                  T* value) {
  DCHECK(!constraints.isNull());
  const auto& the_field = constraints.basic().*picker;
  if (the_field.hasExact()) {
    *value = the_field.exact();
    return true;
  }
  for (const auto& advanced_constraint : constraints.advanced()) {
    const auto& the_field = advanced_constraint.*picker;
    if (the_field.hasExact()) {
      *value = the_field.exact();
      return true;
    }
  }
  return false;
}

template <typename P, typename T>
bool ScanConstraintsForMaxValue(const blink::WebMediaConstraints& constraints,
                                P picker,
                                T* value) {
  DCHECK(!constraints.isNull());
  const auto& the_field = constraints.basic().*picker;
  if (the_field.hasMax()) {
    *value = the_field.max();
    return true;
  }
  if (the_field.hasExact()) {
    *value = the_field.exact();
    return true;
  }
  for (const auto& advanced_constraint : constraints.advanced()) {
    const auto& the_field = advanced_constraint.*picker;
    if (the_field.hasMax()) {
      *value = the_field.max();
      return true;
    }
    if (the_field.hasExact()) {
      *value = the_field.exact();
      return true;
    }
  }
  return false;
}

template <typename P, typename T>
bool ScanConstraintsForMinValue(const blink::WebMediaConstraints& constraints,
                                P picker,
                                T* value) {
  DCHECK(!constraints.isNull());
  const auto& the_field = constraints.basic().*picker;
  if (the_field.hasMin()) {
    *value = the_field.min();
    return true;
  }
  if (the_field.hasExact()) {
    *value = the_field.exact();
    return true;
  }
  for (const auto& advanced_constraint : constraints.advanced()) {
    const auto& the_field = advanced_constraint.*picker;
    if (the_field.hasMin()) {
      *value = the_field.min();
      return true;
    }
    if (the_field.hasExact()) {
      *value = the_field.exact();
      return true;
    }
  }
  return false;
}

}  // namespace

bool GetConstraintValueAsBoolean(
    const blink::WebMediaConstraints& constraints,
    const blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*picker,
    bool* value) {
  return ScanConstraintsForExactValue(constraints, picker, value);
}

bool GetConstraintValueAsInteger(
    const blink::WebMediaConstraints& constraints,
    const blink::LongConstraint blink::WebMediaTrackConstraintSet::*picker,
    int* value) {
  return ScanConstraintsForExactValue(constraints, picker, value);
}

bool GetConstraintMinAsInteger(
    const blink::WebMediaConstraints& constraints,
    const blink::LongConstraint blink::WebMediaTrackConstraintSet::*picker,
    int* value) {
  return ScanConstraintsForMinValue(constraints, picker, value);
}

bool GetConstraintMaxAsInteger(
    const blink::WebMediaConstraints& constraints,
    const blink::LongConstraint blink::WebMediaTrackConstraintSet::*picker,
    int* value) {
  return ScanConstraintsForMaxValue(constraints, picker, value);
}

bool GetConstraintValueAsDouble(
    const blink::WebMediaConstraints& constraints,
    const blink::DoubleConstraint blink::WebMediaTrackConstraintSet::*picker,
    double* value) {
  return ScanConstraintsForExactValue(constraints, picker, value);
}

bool GetConstraintMinAsDouble(
    const blink::WebMediaConstraints& constraints,
    const blink::DoubleConstraint blink::WebMediaTrackConstraintSet::*picker,
    double* value) {
  return ScanConstraintsForMinValue(constraints, picker, value);
}

bool GetConstraintMaxAsDouble(
    const blink::WebMediaConstraints& constraints,
    const blink::DoubleConstraint blink::WebMediaTrackConstraintSet::*picker,
    double* value) {
  return ScanConstraintsForMaxValue(constraints, picker, value);
}

bool GetConstraintValueAsString(
    const blink::WebMediaConstraints& constraints,
    const blink::StringConstraint blink::WebMediaTrackConstraintSet::*picker,
    std::string* value) {
  blink::WebVector<blink::WebString> return_value;
  if (ScanConstraintsForExactValue(constraints, picker, &return_value)) {
    *value = return_value[0].utf8();
    return true;
  }
  return false;
}

rtc::Optional<bool> ConstraintToOptional(
    const blink::WebMediaConstraints& constraints,
    const blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*picker) {
  bool value;
  if (GetConstraintValueAsBoolean(constraints, picker, &value)) {
    return rtc::Optional<bool>(value);
  }
  return rtc::Optional<bool>();
}

}  // namespace content
