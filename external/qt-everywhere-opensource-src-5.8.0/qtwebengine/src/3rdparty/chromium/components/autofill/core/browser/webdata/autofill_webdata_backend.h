// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_H_

#include "sync/internal_api/public/base/model_type.h"

class WebDatabase;

namespace autofill {

class AutofillWebDataServiceObserverOnDBThread;

// Interface for doing Autofill work directly on the DB thread (used by
// Sync, mostly), without fully exposing the AutofillWebDataBackend to clients.
class AutofillWebDataBackend {
 public:
  virtual ~AutofillWebDataBackend() {}

  // Get a raw pointer to the WebDatabase.
  virtual WebDatabase* GetDatabase() = 0;

  // Add an observer to be notified of changes on the DB thread.
  virtual void AddObserver(
      AutofillWebDataServiceObserverOnDBThread* observer) = 0;

  // Remove an observer.
  virtual void RemoveObserver(
      AutofillWebDataServiceObserverOnDBThread* observer) = 0;

  // Remove expired elements from the database and commit if needed.
  virtual void RemoveExpiredFormElements() = 0;

  // Notifies listeners on both DB and UI threads that multiple changes have
  // been made to to Autofill records of the database.
  // NOTE: This method is intended to be called from the DB thread. The UI
  // thread notifications are asynchronous.
  virtual void NotifyOfMultipleAutofillChanges() = 0;

  // Notifies listeners on the UI thread that sync has started for |model_type|.
  // NOTE: This method is intended to be called from the DB thread. The UI
  // thread notifications are asynchronous.
  virtual void NotifyThatSyncHasStarted(syncer::ModelType model_type) = 0;
};

} // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_H_
