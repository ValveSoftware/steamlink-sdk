// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentUpdater_h
#define PaymentUpdater_h

#include "modules/ModulesExport.h"
#include "platform/heap/GarbageCollected.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ScriptValue;

class MODULES_EXPORT PaymentUpdater : public GarbageCollectedMixin {
 public:
  virtual void onUpdatePaymentDetails(
      const ScriptValue& detailsScriptValue) = 0;
  virtual void onUpdatePaymentDetailsFailure(const String& error) = 0;

 protected:
  virtual ~PaymentUpdater() {}
};

}  // namespace blink

#endif  // PaymentUpdater_h
