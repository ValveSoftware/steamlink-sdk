// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BudgetState_h
#define BudgetState_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMTimeStamp.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class BudgetService;

// This exposes the BudgetState interface which is returned from BudgetService
// when there is a GetBudget call.
class BudgetState final : public GarbageCollected<BudgetState>,
                          public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BudgetState();
  BudgetState(double budgetAt, DOMTimeStamp);
  BudgetState(const BudgetState& other);

  double budgetAt() const { return m_budgetAt; }
  DOMTimeStamp time() const { return m_time; }

  void setBudgetAt(const double budgetAt) { m_budgetAt = budgetAt; }
  void setTime(const DOMTimeStamp& time) { m_time = time; }

  DEFINE_INLINE_TRACE() {}

 private:
  double m_budgetAt;
  DOMTimeStamp m_time;
};

}  // namespace blink

#endif  // BudgetState_h
