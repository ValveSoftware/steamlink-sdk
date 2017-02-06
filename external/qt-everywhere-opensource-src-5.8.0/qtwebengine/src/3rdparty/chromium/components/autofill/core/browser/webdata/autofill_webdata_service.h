// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/webdata/autofill_webdata.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database.h"
#include "sync/internal_api/public/base/model_type.h"

class WebDatabaseService;

namespace base {
class SingleThreadTaskRunner;
}

namespace autofill {

class AutofillChange;
class AutofillEntry;
class AutofillProfile;
class AutofillWebDataBackend;
class AutofillWebDataBackendImpl;
class AutofillWebDataServiceObserverOnDBThread;
class AutofillWebDataServiceObserverOnUIThread;
class CreditCard;

// API for Autofill web data.
class AutofillWebDataService : public AutofillWebData,
                               public WebDataServiceBase {
 public:
  AutofillWebDataService(scoped_refptr<base::SingleThreadTaskRunner> ui_thread,
                         scoped_refptr<base::SingleThreadTaskRunner> db_thread);
  AutofillWebDataService(scoped_refptr<WebDatabaseService> wdbs,
                         scoped_refptr<base::SingleThreadTaskRunner> ui_thread,
                         scoped_refptr<base::SingleThreadTaskRunner> db_thread,
                         const ProfileErrorCallback& callback);

  // WebDataServiceBase implementation.
  void ShutdownOnUIThread() override;

  // AutofillWebData implementation.
  void AddFormFields(const std::vector<FormFieldData>& fields) override;
  WebDataServiceBase::Handle GetFormValuesForElementName(
      const base::string16& name,
      const base::string16& prefix,
      int limit,
      WebDataServiceConsumer* consumer) override;

  void RemoveFormElementsAddedBetween(const base::Time& delete_begin,
                                      const base::Time& delete_end) override;
  void RemoveFormValueForElementName(const base::string16& name,
                                     const base::string16& value) override;

  // Profiles.
  void AddAutofillProfile(const AutofillProfile& profile) override;
  void UpdateAutofillProfile(const AutofillProfile& profile) override;
  void RemoveAutofillProfile(const std::string& guid) override;
  WebDataServiceBase::Handle GetAutofillProfiles(
      WebDataServiceConsumer* consumer) override;

  // Server profiles.
  WebDataServiceBase::Handle GetServerProfiles(
      WebDataServiceConsumer* consumer) override;

  WebDataServiceBase::Handle GetCountOfValuesContainedBetween(
      const base::Time& begin,
      const base::Time& end,
      WebDataServiceConsumer* consumer) override;
  void UpdateAutofillEntries(
      const std::vector<AutofillEntry>& autofill_entries) override;

  // Credit cards.
  void AddCreditCard(const CreditCard& credit_card) override;
  void UpdateCreditCard(const CreditCard& credit_card) override;
  void RemoveCreditCard(const std::string& guid) override;
  WebDataServiceBase::Handle GetCreditCards(
      WebDataServiceConsumer* consumer) override;

  // Server cards.
  WebDataServiceBase::Handle GetServerCreditCards(
      WebDataServiceConsumer* consumer) override;
  void UnmaskServerCreditCard(const CreditCard& card,
                              const base::string16& full_number) override;
  void MaskServerCreditCard(const std::string& id) override;

  void ClearAllServerData();

  void UpdateServerCardUsageStats(const CreditCard& credit_card) override;
  void UpdateServerAddressUsageStats(const AutofillProfile& profile) override;
  void UpdateServerCardBillingAddress(const CreditCard& credit_card) override;

  void RemoveAutofillDataModifiedBetween(const base::Time& delete_begin,
                                         const base::Time& delete_end) override;
  void RemoveOriginURLsModifiedBetween(const base::Time& delete_begin,
                                       const base::Time& delete_end) override;

  void AddObserver(AutofillWebDataServiceObserverOnDBThread* observer);
  void RemoveObserver(AutofillWebDataServiceObserverOnDBThread* observer);

  void AddObserver(AutofillWebDataServiceObserverOnUIThread* observer);
  void RemoveObserver(AutofillWebDataServiceObserverOnUIThread* observer);

  // Returns a SupportsUserData objects that may be used to store data
  // owned by the DB thread on this object. Should be called only from
  // the DB thread, and will be destroyed on the DB thread soon after
  // |ShutdownOnUIThread()| is called.
  base::SupportsUserData* GetDBUserData();

  // Takes a callback which will be called on the DB thread with a pointer to an
  // |AutofillWebdataBackend|. This backend can be used to access or update the
  // WebDatabase directly on the DB thread.
  void GetAutofillBackend(
      const base::Callback<void(AutofillWebDataBackend*)>& callback);

 protected:
  ~AutofillWebDataService() override;

  virtual void NotifyAutofillMultipleChangedOnUIThread();

  virtual void NotifySyncStartedOnUIThread(syncer::ModelType model_type);

  base::WeakPtr<AutofillWebDataService> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::ObserverList<AutofillWebDataServiceObserverOnUIThread>
      ui_observer_list_;

  // The task runner that this class uses as its UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;

  // The task runner that this class uses as its DB thread.
  scoped_refptr<base::SingleThreadTaskRunner> db_thread_;

  scoped_refptr<AutofillWebDataBackendImpl> autofill_backend_;

  // This factory is used on the UI thread. All vended weak pointers are
  // invalidated in ShutdownOnUIThread().
  base::WeakPtrFactory<AutofillWebDataService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWebDataService);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_
