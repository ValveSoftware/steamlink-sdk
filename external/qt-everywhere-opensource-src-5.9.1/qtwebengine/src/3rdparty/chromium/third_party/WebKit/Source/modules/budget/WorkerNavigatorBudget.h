// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerNavigatorBudget_h
#define WorkerNavigatorBudget_h

#include "platform/Supplementable.h"
#include "platform/heap/GarbageCollected.h"
#include "wtf/Noncopyable.h"

namespace blink {

class BudgetService;
class WorkerNavigator;

// This exposes the budget object on the WorkerNavigator partial interface.
class WorkerNavigatorBudget final
    : public GarbageCollected<WorkerNavigatorBudget>,
      public Supplement<WorkerNavigator> {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerNavigatorBudget);
  WTF_MAKE_NONCOPYABLE(WorkerNavigatorBudget);

 public:
  static WorkerNavigatorBudget& from(WorkerNavigator&);

  static BudgetService* budget(WorkerNavigator&);
  BudgetService* budget();

  DECLARE_VIRTUAL_TRACE();

 private:
  WorkerNavigatorBudget();
  static const char* supplementName();

  Member<BudgetService> m_budget;
};

}  // namespace blink

#endif  // WorkerNavigatorBudget_h
