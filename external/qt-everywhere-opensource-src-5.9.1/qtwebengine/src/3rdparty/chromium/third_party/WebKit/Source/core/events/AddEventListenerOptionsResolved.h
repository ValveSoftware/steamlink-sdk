// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AddEventListenerOptionsResolved_h
#define AddEventListenerOptionsResolved_h

#include "core/events/AddEventListenerOptions.h"

namespace blink {

// AddEventListenerOptionsResolved class represents resolved event listener
// options. An application requests AddEventListenerOptions and the user
// agent may change ('resolve') these settings (based on settings or policies)
// and the result and the reasons why changes occurred are stored in this class.
class CORE_EXPORT AddEventListenerOptionsResolved
    : public AddEventListenerOptions {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  AddEventListenerOptionsResolved();
  AddEventListenerOptionsResolved(const AddEventListenerOptions&);
  virtual ~AddEventListenerOptionsResolved();

  void setPassiveForcedForDocumentTarget(bool forced) {
    m_passiveForcedForDocumentTarget = forced;
  }
  bool passiveForcedForDocumentTarget() const {
    return m_passiveForcedForDocumentTarget;
  }

  // Set whether passive was specified when the options were
  // created by callee.
  void setPassiveSpecified(bool specified) { m_passiveSpecified = specified; }
  bool passiveSpecified() const { return m_passiveSpecified; }

  DECLARE_VIRTUAL_TRACE();

 private:
  bool m_passiveForcedForDocumentTarget;
  bool m_passiveSpecified;
};

}  // namespace blink

#endif  // AddEventListenerOptionsResolved_h
