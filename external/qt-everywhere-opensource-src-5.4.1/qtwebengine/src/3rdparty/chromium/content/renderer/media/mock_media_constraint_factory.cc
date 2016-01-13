// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/mock_media_constraint_factory.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.h"

namespace content {

namespace {

static const char kValueTrue[] = "true";
static const char kValueFalse[] = "false";

}  // namespace

MockMediaConstraintFactory::MockMediaConstraintFactory() {
}

MockMediaConstraintFactory::~MockMediaConstraintFactory() {
}

blink::WebMediaConstraints
MockMediaConstraintFactory::CreateWebMediaConstraints() {
  blink::WebVector<blink::WebMediaConstraint> mandatory(mandatory_);
  blink::WebVector<blink::WebMediaConstraint> optional(optional_);
  blink::WebMediaConstraints constraints;
  constraints.initialize(optional, mandatory);
  return constraints;
}

void MockMediaConstraintFactory::AddMandatory(const std::string& key,
                                              int value) {
  mandatory_.push_back(blink::WebMediaConstraint(base::UTF8ToUTF16(key),
                                                 base::IntToString16(value)));
}

void MockMediaConstraintFactory::AddMandatory(const std::string& key,
                                              double value) {
  mandatory_.push_back(blink::WebMediaConstraint(
      base::UTF8ToUTF16(key),
      base::UTF8ToUTF16(base::DoubleToString(value))));
}

void MockMediaConstraintFactory::AddMandatory(const std::string& key,
                                              const std::string& value) {
  mandatory_.push_back(blink::WebMediaConstraint(
      base::UTF8ToUTF16(key), base::UTF8ToUTF16(value)));
}

void MockMediaConstraintFactory::AddMandatory(const std::string& key,
                                              bool value) {
  const std::string string_value = value ? kValueTrue : kValueFalse;
  AddMandatory(key, string_value);
}

void MockMediaConstraintFactory::AddOptional(const std::string& key,
                                             int value) {
  optional_.push_back(blink::WebMediaConstraint(base::UTF8ToUTF16(key),
                                                base::IntToString16(value)));
}

void MockMediaConstraintFactory::AddOptional(const std::string& key,
                                             double value) {
  optional_.push_back(blink::WebMediaConstraint(
      base::UTF8ToUTF16(key),
      base::UTF8ToUTF16(base::DoubleToString(value))));
}

void MockMediaConstraintFactory::AddOptional(const std::string& key,
                                             const std::string& value) {
  optional_.push_back(blink::WebMediaConstraint(
      base::UTF8ToUTF16(key), base::UTF8ToUTF16(value)));
}

void MockMediaConstraintFactory::AddOptional(const std::string& key,
                                              bool value) {
  const std::string string_value = value ? kValueTrue : kValueFalse;
  AddOptional(key, string_value);
}

void MockMediaConstraintFactory::DisableDefaultAudioConstraints() {
  static const char* kDefaultAudioConstraints[] = {
      webrtc::MediaConstraintsInterface::kEchoCancellation,
      webrtc::MediaConstraintsInterface::kExperimentalEchoCancellation,
      webrtc::MediaConstraintsInterface::kAutoGainControl,
      webrtc::MediaConstraintsInterface::kExperimentalAutoGainControl,
      webrtc::MediaConstraintsInterface::kNoiseSuppression,
      webrtc::MediaConstraintsInterface::kHighpassFilter,
      webrtc::MediaConstraintsInterface::kTypingNoiseDetection,
      webrtc::MediaConstraintsInterface::kExperimentalNoiseSuppression
  };
  MockMediaConstraintFactory factory;
  for (size_t i = 0; i < arraysize(kDefaultAudioConstraints); ++i) {
    AddMandatory(kDefaultAudioConstraints[i], false);
  }
}

}  // namespace content
