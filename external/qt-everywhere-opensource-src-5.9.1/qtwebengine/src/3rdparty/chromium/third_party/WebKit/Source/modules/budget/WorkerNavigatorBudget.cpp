// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/budget/WorkerNavigatorBudget.h"

#include "core/workers/WorkerNavigator.h"
#include "modules/budget/BudgetService.h"

namespace blink {

WorkerNavigatorBudget::WorkerNavigatorBudget() {}

// static
const char* WorkerNavigatorBudget::supplementName() {
  return "WorkerNavigatorBudget";
}

// static
WorkerNavigatorBudget& WorkerNavigatorBudget::from(
    WorkerNavigator& workerNavigator) {
  // Get the unique WorkerNavigatorBudget associated with this workerNavigator.
  WorkerNavigatorBudget* workerNavigatorBudget =
      static_cast<WorkerNavigatorBudget*>(
          Supplement<WorkerNavigator>::from(workerNavigator, supplementName()));
  if (!workerNavigatorBudget) {
    // If there isn't one already, create it now and associate it.
    workerNavigatorBudget = new WorkerNavigatorBudget();
    Supplement<WorkerNavigator>::provideTo(workerNavigator, supplementName(),
                                           workerNavigatorBudget);
  }
  return *workerNavigatorBudget;
}

BudgetService* WorkerNavigatorBudget::budget() {
  if (!m_budget)
    m_budget = BudgetService::create();
  return m_budget.get();
}

// static
BudgetService* WorkerNavigatorBudget::budget(WorkerNavigator& workerNavigator) {
  return WorkerNavigatorBudget::from(workerNavigator).budget();
}

DEFINE_TRACE(WorkerNavigatorBudget) {
  visitor->trace(m_budget);
  Supplement<WorkerNavigator>::trace(visitor);
}

}  // namespace blink
