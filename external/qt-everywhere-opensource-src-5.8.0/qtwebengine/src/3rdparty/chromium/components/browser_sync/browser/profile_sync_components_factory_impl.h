// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__
#define COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__

#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "components/version_info/version_info.h"
#include "sync/internal_api/public/base/model_type.h"
#include "url/gurl.h"

class OAuth2TokenService;
class Profile;

namespace autofill {
class AutofillWebDataService;
}

namespace password_manager {
class PasswordStore;
}

namespace net {
class URLRequestContextGetter;
}

class ProfileSyncComponentsFactoryImpl
    : public sync_driver::SyncApiComponentFactory {
 public:
  // Constructs a ProfileSyncComponentsFactoryImpl.
  //
  // |sync_service_url| is the base URL of the sync server.
  //
  // |token_service| must outlive the ProfileSyncComponentsFactoryImpl.
  //
  // |url_request_context_getter| must outlive the
  // ProfileSyncComponentsFactoryImpl.
  ProfileSyncComponentsFactoryImpl(
      sync_driver::SyncClient* sync_client,
      version_info::Channel channel,
      const std::string& version,
      bool is_tablet,
      const base::CommandLine& command_line,
      const char* history_disabled_pref,
      const GURL& sync_service_url,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
      OAuth2TokenService* token_service,
      net::URLRequestContextGetter* url_request_context_getter,
      const scoped_refptr<autofill::AutofillWebDataService>& web_data_service,
      const scoped_refptr<password_manager::PasswordStore>& password_store);
  ~ProfileSyncComponentsFactoryImpl() override;

  // SyncApiComponentFactory implementation:
  void RegisterDataTypes(
      sync_driver::SyncService* sync_service,
      const RegisterDataTypesMethod& register_platform_types_method) override;
  sync_driver::DataTypeManager* CreateDataTypeManager(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const sync_driver::DataTypeController::TypeMap* controllers,
      const sync_driver::DataTypeEncryptionHandler* encryption_handler,
      browser_sync::SyncBackendHost* backend,
      sync_driver::DataTypeManagerObserver* observer) override;
  browser_sync::SyncBackendHost* CreateSyncBackendHost(
      const std::string& name,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
      const base::FilePath& sync_folder) override;
  std::unique_ptr<sync_driver::LocalDeviceInfoProvider>
  CreateLocalDeviceInfoProvider() override;
  std::unique_ptr<syncer::AttachmentService> CreateAttachmentService(
      std::unique_ptr<syncer::AttachmentStoreForSync> attachment_store,
      const syncer::UserShare& user_share,
      const std::string& store_birthday,
      syncer::ModelType model_type,
      syncer::AttachmentService::Delegate* delegate) override;
  sync_driver::SyncApiComponentFactory::SyncComponents
  CreateBookmarkSyncComponents(
      sync_driver::SyncService* sync_service,
      syncer::DataTypeErrorHandler* error_handler) override;

 private:
  // Register data types which are enabled on both desktop and mobile.
  // |disabled_types| and |enabled_types| correspond only to those types
  // being explicitly enabled/disabled by the command line.
  void RegisterCommonDataTypes(sync_driver::SyncService* sync_service,
                               syncer::ModelTypeSet disabled_types,
                               syncer::ModelTypeSet enabled_types);

  void DisableBrokenType(syncer::ModelType type,
                         const tracked_objects::Location& from_here,
                         const std::string& message);

  // Client/platform specific members.
  sync_driver::SyncClient* const sync_client_;
  const version_info::Channel channel_;
  const std::string version_;
  const bool is_tablet_;
  const base::CommandLine command_line_;
  const char* history_disabled_pref_;
  const GURL sync_service_url_;
  const scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;
  const scoped_refptr<base::SingleThreadTaskRunner> db_thread_;
  OAuth2TokenService* const token_service_;
  net::URLRequestContextGetter* const url_request_context_getter_;
  const scoped_refptr<autofill::AutofillWebDataService> web_data_service_;
  const scoped_refptr<password_manager::PasswordStore> password_store_;

  base::WeakPtrFactory<ProfileSyncComponentsFactoryImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncComponentsFactoryImpl);
};

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__
