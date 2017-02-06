// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_OBSERVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_OBSERVER_H_

#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "sync/internal_api/public/base/model_type.h"

namespace autofill {

class AutofillWebDataServiceObserverOnDBThread {
 public:
  // Called on DB thread whenever Autofill entries are changed.
  virtual void AutofillEntriesChanged(const AutofillChangeList& changes) {}

  // Called on DB thread when an AutofillProfile has been added/removed/updated
  // in the WebDatabase.
  virtual void AutofillProfileChanged(const AutofillProfileChange& change) {}

  // Called on DB thread when a CreditCard has been added/removed/updated in the
  // WebDatabase.
  virtual void CreditCardChanged(const CreditCardChange& change) {}

  // Called on DB thread when multiple Autofill entries have been modified by
  // Sync.
  virtual void AutofillMultipleChanged() {}

 protected:
  virtual ~AutofillWebDataServiceObserverOnDBThread() {}
};

class AutofillWebDataServiceObserverOnUIThread {
 public:
  // Called on UI thread when multiple Autofill entries have been modified by
  // Sync.
  virtual void AutofillMultipleChanged() {}

  // Called on UI thread when sync has started for |model_type|.
  virtual void SyncStarted(syncer::ModelType /* model_type */) {}

 protected:
  virtual ~AutofillWebDataServiceObserverOnUIThread() {}
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_OBSERVER_H_
