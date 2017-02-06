// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata_services/web_data_service_wrapper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/webdata/autocomplete_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_profile_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/password_manager/core/browser/webdata/logins_table.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/signin/core/browser/webdata/token_service_table.h"
#include "components/signin/core/browser/webdata/token_web_data.h"
#include "components/webdata/common/web_database_service.h"
#include "components/webdata/common/webdata_constants.h"

#if defined(OS_WIN)
#include "components/password_manager/core/browser/webdata/password_web_data_service_win.h"
#endif

namespace {

void InitSyncableServicesOnDBThread(
    scoped_refptr<base::SingleThreadTaskRunner> db_thread,
    const syncer::SyncableService::StartSyncFlare& sync_flare,
    const scoped_refptr<autofill::AutofillWebDataService>& autofill_web_data,
    const base::FilePath& context_path,
    const std::string& app_locale,
    autofill::AutofillWebDataBackend* autofill_backend) {
  DCHECK(db_thread->BelongsToCurrentThread());

  // Currently only Autocomplete and Autofill profiles use the new Sync API, but
  // all the database data should migrate to this API over time.
  autofill::AutocompleteSyncableService::CreateForWebDataServiceAndBackend(
      autofill_web_data.get(), autofill_backend);
  autofill::AutocompleteSyncableService::FromWebDataService(
      autofill_web_data.get())->InjectStartSyncFlare(sync_flare);

  autofill::AutofillProfileSyncableService::CreateForWebDataServiceAndBackend(
      autofill_web_data.get(), autofill_backend, app_locale);
  autofill::AutofillWalletSyncableService::CreateForWebDataServiceAndBackend(
      autofill_web_data.get(), autofill_backend, app_locale);
  autofill::AutofillWalletMetadataSyncableService::
      CreateForWebDataServiceAndBackend(autofill_web_data.get(),
                                        autofill_backend, app_locale);

  autofill::AutofillProfileSyncableService::FromWebDataService(
      autofill_web_data.get())->InjectStartSyncFlare(sync_flare);
  autofill::AutofillWalletSyncableService::FromWebDataService(
      autofill_web_data.get())->InjectStartSyncFlare(sync_flare);
}

}  // namespace

WebDataServiceWrapper::WebDataServiceWrapper() {
}

WebDataServiceWrapper::WebDataServiceWrapper(
    const base::FilePath& context_path,
    const std::string& application_locale,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
    const syncer::SyncableService::StartSyncFlare& flare,
    const ShowErrorCallback& show_error_callback) {
  base::FilePath path = context_path.Append(kWebDataFilename);
  web_database_ = new WebDatabaseService(path, ui_thread, db_thread);

  // All tables objects that participate in managing the database must
  // be added here.
  web_database_->AddTable(base::WrapUnique(new autofill::AutofillTable));
  web_database_->AddTable(base::WrapUnique(new KeywordTable));
  // TODO(mdm): We only really need the LoginsTable on Windows for IE7 password
  // access, but for now, we still create it on all platforms since it deletes
  // the old logins table. We can remove this after a while, e.g. in M22 or so.
  web_database_->AddTable(base::WrapUnique(new LoginsTable));
  web_database_->AddTable(base::WrapUnique(new TokenServiceTable));
  web_database_->LoadDatabase();

  autofill_web_data_ = new autofill::AutofillWebDataService(
      web_database_, ui_thread, db_thread,
      base::Bind(show_error_callback, ERROR_LOADING_AUTOFILL));
  autofill_web_data_->Init();

  keyword_web_data_ = new KeywordWebDataService(
      web_database_, ui_thread,
      base::Bind(show_error_callback, ERROR_LOADING_KEYWORD));
  keyword_web_data_->Init();

  token_web_data_ = new TokenWebData(
      web_database_, ui_thread, db_thread,
      base::Bind(show_error_callback, ERROR_LOADING_TOKEN));
  token_web_data_->Init();

#if defined(OS_WIN)
  password_web_data_ = new PasswordWebDataService(
      web_database_, ui_thread,
      base::Bind(show_error_callback, ERROR_LOADING_PASSWORD));
  password_web_data_->Init();
#endif

  autofill_web_data_->GetAutofillBackend(
      base::Bind(&InitSyncableServicesOnDBThread, db_thread, flare,
                 autofill_web_data_, context_path, application_locale));
}

WebDataServiceWrapper::~WebDataServiceWrapper() {
}

void WebDataServiceWrapper::Shutdown() {
  autofill_web_data_->ShutdownOnUIThread();
  keyword_web_data_->ShutdownOnUIThread();
  token_web_data_->ShutdownOnUIThread();

#if defined(OS_WIN)
  password_web_data_->ShutdownOnUIThread();
#endif

  web_database_->ShutdownDatabase();
}

scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceWrapper::GetAutofillWebData() {
  return autofill_web_data_.get();
}

scoped_refptr<KeywordWebDataService>
WebDataServiceWrapper::GetKeywordWebData() {
  return keyword_web_data_.get();
}

scoped_refptr<TokenWebData> WebDataServiceWrapper::GetTokenWebData() {
  return token_web_data_.get();
}

#if defined(OS_WIN)
scoped_refptr<PasswordWebDataService>
WebDataServiceWrapper::GetPasswordWebData() {
  return password_web_data_.get();
}
#endif
