// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "AddEventListenerOptionsResolved.h"

namespace blink {

AddEventListenerOptionsResolved::AddEventListenerOptionsResolved()
    : m_passiveForcedForDocumentTarget(false), m_passiveSpecified(false) {}

AddEventListenerOptionsResolved::AddEventListenerOptionsResolved(
    const AddEventListenerOptions& options)
    : AddEventListenerOptions(options),
      m_passiveForcedForDocumentTarget(false),
      m_passiveSpecified(false) {}

AddEventListenerOptionsResolved::~AddEventListenerOptionsResolved() {}

DEFINE_TRACE(AddEventListenerOptionsResolved) {
  AddEventListenerOptions::trace(visitor);
}

}  // namespace blink
