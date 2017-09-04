// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/budget/NavigatorBudget.h"

#include "core/frame/Navigator.h"
#include "modules/budget/BudgetService.h"

namespace blink {

NavigatorBudget::NavigatorBudget() {}

// static
const char* NavigatorBudget::supplementName() {
  return "NavigatorBudget";
}

// static
NavigatorBudget& NavigatorBudget::from(Navigator& navigator) {
  // Get the unique NavigatorBudget associated with this navigator.
  NavigatorBudget* navigatorBudget = static_cast<NavigatorBudget*>(
      Supplement<Navigator>::from(navigator, supplementName()));
  if (!navigatorBudget) {
    // If there isn't one already, create it now and associate it.
    navigatorBudget = new NavigatorBudget();
    Supplement<Navigator>::provideTo(navigator, supplementName(),
                                     navigatorBudget);
  }
  return *navigatorBudget;
}

BudgetService* NavigatorBudget::budget() {
  if (!m_budget)
    m_budget = BudgetService::create();
  return m_budget.get();
}

// static
BudgetService* NavigatorBudget::budget(Navigator& navigator) {
  return NavigatorBudget::from(navigator).budget();
}

DEFINE_TRACE(NavigatorBudget) {
  visitor->trace(m_budget);
  Supplement<Navigator>::trace(visitor);
}

}  // namespace blink
