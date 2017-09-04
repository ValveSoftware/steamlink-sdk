// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BudgetService_h
#define BudgetService_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "public/platform/modules/budget_service/budget_service.mojom-blink.h"

namespace blink {

class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// This is the entry point into the browser for the BudgetService API, which is
// designed to give origins the ability to perform background operations
// on the user's behalf.
class BudgetService final : public GarbageCollectedFinalized<BudgetService>,
                            public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BudgetService* create() { return new BudgetService(); }

  ~BudgetService();

  // Implementation of the Budget API interface.
  ScriptPromise getCost(ScriptState*, const AtomicString& operation);
  ScriptPromise getBudget(ScriptState*);
  ScriptPromise reserve(ScriptState*, const AtomicString& operation);

  DEFINE_INLINE_TRACE() {}

 private:
  // Callbacks from the BudgetService to the blink layer.
  void gotCost(ScriptPromiseResolver*, double cost) const;
  void gotBudget(
      ScriptPromiseResolver*,
      mojom::blink::BudgetServiceErrorType,
      const mojo::WTFArray<mojom::blink::BudgetStatePtr> expectations) const;
  void gotReservation(ScriptPromiseResolver*,
                      mojom::blink::BudgetServiceErrorType,
                      bool success) const;

  // Error handler for use if mojo service doesn't connect.
  void onConnectionError();

  BudgetService();

  // Pointer to the Mojo service which will proxy calls to the browser.
  mojom::blink::BudgetServicePtr m_service;
};

}  // namespace blink

#endif  // BudgetService_h
