// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/weborigin/Suborigin.h"

#include "wtf/ASCIICType.h"
#include "wtf/text/ParsingUtilities.h"

namespace blink {

static unsigned emptyPolicy =
    static_cast<unsigned>(Suborigin::SuboriginPolicyOptions::None);

Suborigin::Suborigin() : m_optionsMask(emptyPolicy) {}

Suborigin::Suborigin(const Suborigin* other)
    : m_name(other->m_name.isolatedCopy()),
      m_optionsMask(other->m_optionsMask) {}

void Suborigin::setTo(const Suborigin& other) {
  m_name = other.m_name;
  m_optionsMask = other.m_optionsMask;
}

void Suborigin::addPolicyOption(SuboriginPolicyOptions option) {
  m_optionsMask |= static_cast<unsigned>(option);
}

bool Suborigin::policyContains(SuboriginPolicyOptions option) const {
  return m_optionsMask & static_cast<unsigned>(option);
}

void Suborigin::clear() {
  m_name = String();
  m_optionsMask = emptyPolicy;
}

}  // namespace blink
