// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/EditingState.h"

namespace blink {

EditingState::EditingState() {}

EditingState::~EditingState() {}

void EditingState::abort() {
  DCHECK(!m_isAborted);
  m_isAborted = true;
}

// ---
IgnorableEditingAbortState::IgnorableEditingAbortState() {}

IgnorableEditingAbortState::~IgnorableEditingAbortState() {}

#if DCHECK_IS_ON()
// ---

NoEditingAbortChecker::NoEditingAbortChecker(const char* file, int line)
    : m_file(file), m_line(line) {}

NoEditingAbortChecker::~NoEditingAbortChecker() {
  DCHECK_AT(!m_editingState.isAborted(), m_file, m_line)
      << "The operation should not have been aborted.";
}

#endif

}  // namespace blink
