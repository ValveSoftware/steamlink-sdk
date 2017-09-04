// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorBudget_h
#define NavigatorBudget_h

#include "platform/Supplementable.h"

namespace blink {

class BudgetService;
class Navigator;

// This exposes the budget object on the Navigator partial interface.
class NavigatorBudget final : public GarbageCollected<NavigatorBudget>,
                              public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorBudget);
  WTF_MAKE_NONCOPYABLE(NavigatorBudget);

 public:
  static NavigatorBudget& from(Navigator&);

  static BudgetService* budget(Navigator&);
  BudgetService* budget();

  DECLARE_VIRTUAL_TRACE();

 private:
  NavigatorBudget();
  static const char* supplementName();

  Member<BudgetService> m_budget;
};

}  // namespace blink

#endif  // NavigatorBudget_h
