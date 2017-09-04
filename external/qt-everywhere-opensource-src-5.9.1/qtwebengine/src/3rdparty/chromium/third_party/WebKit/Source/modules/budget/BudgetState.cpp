// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/budget/BudgetState.h"

namespace blink {

BudgetState::BudgetState() : m_budgetAt(0), m_time(DOMTimeStamp()) {}

BudgetState::BudgetState(double budgetAt, DOMTimeStamp time)
    : m_budgetAt(budgetAt), m_time(time) {}

BudgetState::BudgetState(const BudgetState& other)
    : m_budgetAt(other.m_budgetAt), m_time(other.m_time) {}

}  // namespace blink
