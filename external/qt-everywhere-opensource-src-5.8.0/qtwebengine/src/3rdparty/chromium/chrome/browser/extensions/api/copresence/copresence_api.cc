// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/copresence/copresence_api.h"

#include <utility>

#include "base/lazy_instance.h"
#include "chrome/browser/copresence/chrome_whispernet_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/extensions/api/copresence.h"
#include "chrome/common/extensions/manifest_handlers/copresence_manifest.h"
#include "chrome/common/pref_names.h"
#include "components/copresence/copresence_manager_impl.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/proto/enums.pb.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

using user_prefs::PrefRegistrySyncable;

namespace extensions {

namespace {

base::LazyInstance<BrowserContextKeyedAPIFactory<CopresenceService>>
    g_factory = LAZY_INSTANCE_INITIALIZER;

const char kInvalidOperationsMessage[] =
    "Invalid operation in operations array.";
const char kShuttingDownMessage[] = "Shutting down.";

const std::string GetPrefName(bool authenticated) {
  return authenticated ? prefs::kCopresenceAuthenticatedDeviceId
                       : prefs::kCopresenceAnonymousDeviceId;
}

}  // namespace

namespace Execute = api::copresence::Execute;
namespace OnMessagesReceived = api::copresence::OnMessagesReceived;
namespace OnStatusUpdated = api::copresence::OnStatusUpdated;
namespace SetApiKey = api::copresence::SetApiKey;
namespace SetAuthToken = api::copresence::SetAuthToken;

// Public functions.

CopresenceService::CopresenceService(content::BrowserContext* context)
    : is_shutting_down_(false), browser_context_(context) {}

CopresenceService::~CopresenceService() {}

void CopresenceService::Shutdown() {
  is_shutting_down_ = true;
  manager_.reset();
  whispernet_client_.reset();
}

copresence::CopresenceManager* CopresenceService::manager() {
  if (!manager_ && !is_shutting_down_)
    manager_.reset(new copresence::CopresenceManagerImpl(this));
  return manager_.get();
}

std::string CopresenceService::auth_token(const std::string& app_id)
    const {
  // This won't be const if we use map[]
  const auto& key = auth_tokens_by_app_.find(app_id);
  return key == auth_tokens_by_app_.end() ? std::string() : key->second;
}

void CopresenceService::set_api_key(const std::string& app_id,
                                    const std::string& api_key) {
  DCHECK(!app_id.empty());
  api_keys_by_app_[app_id] = api_key;
}

void CopresenceService::set_auth_token(const std::string& app_id,
                                       const std::string& token) {
  DCHECK(!app_id.empty());
  auth_tokens_by_app_[app_id] = token;
}

void CopresenceService::set_manager_for_testing(
    std::unique_ptr<copresence::CopresenceManager> manager) {
  manager_ = std::move(manager);
}

void CopresenceService::ResetState() {
  DVLOG(2) << "Deleting copresence state";
  GetPrefService()->ClearPref(prefs::kCopresenceAuthenticatedDeviceId);
  GetPrefService()->ClearPref(prefs::kCopresenceAnonymousDeviceId);
  manager_ = nullptr;
}

// static
void CopresenceService::RegisterProfilePrefs(PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kCopresenceAuthenticatedDeviceId,
                               std::string());
  registry->RegisterStringPref(prefs::kCopresenceAnonymousDeviceId,
                               std::string());
}

// static
BrowserContextKeyedAPIFactory<CopresenceService>*
CopresenceService::GetFactoryInstance() {
  return g_factory.Pointer();
}


// Private functions.

void CopresenceService::HandleMessages(
    const std::string& /* app_id */,
    const std::string& subscription_id,
    const std::vector<copresence::Message>& messages) {
  // TODO(ckehoe): Once the server starts sending back the app ids associated
  // with subscriptions, use that instead of the apps_by_subs registry.
  std::string app_id = apps_by_subscription_id_[subscription_id];

  if (app_id.empty()) {
    LOG(ERROR) << "Skipping message from unrecognized subscription "
               << subscription_id;
    return;
  }

  int message_count = messages.size();
  std::vector<api::copresence::Message> api_messages(message_count);

  for (const copresence::Message& message : messages) {
    api::copresence::Message api_message;
    api_message.type = message.type().type();
    api_message.payload.assign(message.payload().begin(),
                               message.payload().end());
    api_messages.push_back(std::move(api_message));
    DVLOG(2) << "Dispatching message of type " << api_message.type << ":\n"
             << message.payload();
  }

  // Send the messages to the client app.
  std::unique_ptr<Event> event(new Event(
      events::COPRESENCE_ON_MESSAGES_RECEIVED, OnMessagesReceived::kEventName,
      OnMessagesReceived::Create(subscription_id, api_messages),
      browser_context_));
  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(app_id, std::move(event));
  DVLOG(2) << "Passed " << api_messages.size() << " messages to app \""
           << app_id << "\" for subscription \"" << subscription_id << "\"";
}

void CopresenceService::HandleStatusUpdate(
    copresence::CopresenceStatus status) {
  DCHECK_EQ(copresence::AUDIO_FAIL, status);
  std::unique_ptr<Event> event(new Event(
      events::COPRESENCE_ON_STATUS_UPDATED, OnStatusUpdated::kEventName,
      OnStatusUpdated::Create(api::copresence::STATUS_AUDIOFAILED),
      browser_context_));
  EventRouter::Get(browser_context_)->BroadcastEvent(std::move(event));
  DVLOG(2) << "Sent Audio Failed status update.";
}

net::URLRequestContextGetter* CopresenceService::GetRequestContext() const {
  return content::BrowserContext::GetDefaultStoragePartition(browser_context_)->
      GetURLRequestContext();
}

std::string CopresenceService::GetPlatformVersionString() const {
  return chrome::GetVersionString();
}

std::string
CopresenceService::GetAPIKey(const std::string& app_id) const {
  // Check first if the app has set its key via the API.
  const auto& key = api_keys_by_app_.find(app_id);
  if (key != api_keys_by_app_.end())
    return key->second;

  // If no key was found, look in the manifest.
  if (!app_id.empty()) {
    const Extension* extension = ExtensionRegistry::Get(browser_context_)
        ->GetExtensionById(app_id, ExtensionRegistry::ENABLED);
    DCHECK(extension) << "Invalid extension ID";
    CopresenceManifestData* manifest_data =
        static_cast<CopresenceManifestData*>(
            extension->GetManifestData(manifest_keys::kCopresence));
    if (manifest_data)
      return manifest_data->api_key;
  }

  return std::string();
}

audio_modem::WhispernetClient* CopresenceService::GetWhispernetClient() {
  if (!whispernet_client_ && !is_shutting_down_)
    whispernet_client_.reset(new ChromeWhispernetClient(browser_context_));
  return whispernet_client_.get();
}

gcm::GCMDriver* CopresenceService::GetGCMDriver() {
  gcm::GCMProfileService* gcm_service =
      gcm::GCMProfileServiceFactory::GetForProfile(browser_context_);
  return gcm_service ? gcm_service->driver() : nullptr;
}

std::string CopresenceService::GetDeviceId(bool authenticated) {
  std::string id = GetPrefService()->GetString(GetPrefName(authenticated));
  DVLOG(3) << "Retrieved device ID \"" << id << "\", "
           << "authenticated = " << authenticated;
  return id;
}

void CopresenceService::SaveDeviceId(bool authenticated,
                                     const std::string& device_id) {
  DVLOG(3) << "Storing device ID \"" << device_id << "\", "
           << "authenticated = " << authenticated;
  if (device_id.empty())
    GetPrefService()->ClearPref(GetPrefName(authenticated));
  else
    GetPrefService()->SetString(GetPrefName(authenticated), device_id);
}

PrefService* CopresenceService::GetPrefService() {
  return Profile::FromBrowserContext(browser_context_)->GetPrefs();
}

template <>
void
BrowserContextKeyedAPIFactory<CopresenceService>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

// CopresenceExecuteFunction implementation.
ExtensionFunction::ResponseAction CopresenceExecuteFunction::Run() {
  std::unique_ptr<Execute::Params> params(Execute::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopresenceService* service =
      CopresenceService::GetFactoryInstance()->Get(browser_context());

  // This can only happen if we're shutting down. In all other cases, if we
  // don't have a manager, we'll create one.
  if (!service->manager())
    return RespondNow(Error(kShuttingDownMessage));

  // Each execute will correspond to one ReportRequest protocol buffer.
  copresence::ReportRequest request;
  if (!PrepareReportRequestProto(params->operations,
                                 extension_id(),
                                 &service->apps_by_subscription_id(),
                                 &request)) {
    return RespondNow(Error(kInvalidOperationsMessage));
  }

  service->manager()->ExecuteReportRequest(
      request,
      extension_id(),
      service->auth_token(extension_id()),
      base::Bind(&CopresenceExecuteFunction::SendResult, this));
  return RespondLater();
}

void CopresenceExecuteFunction::SendResult(
    copresence::CopresenceStatus status) {
  api::copresence::ExecuteStatus api_status =
      (status == copresence::SUCCESS) ? api::copresence::EXECUTE_STATUS_SUCCESS
                                      : api::copresence::EXECUTE_STATUS_FAILED;
  Respond(ArgumentList(Execute::Results::Create(api_status)));
}

// CopresenceSetApiKeyFunction implementation.
ExtensionFunction::ResponseAction CopresenceSetApiKeyFunction::Run() {
  std::unique_ptr<SetApiKey::Params> params(SetApiKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  LOG(WARNING) << "copresence.setApiKey() is deprecated. "
               << "Put the key in the manifest at copresence.api_key instead.";

  // The api key may be set to empty, to clear it.
  CopresenceService::GetFactoryInstance()->Get(browser_context())
      ->set_api_key(extension_id(), params->api_key);
  return RespondNow(NoArguments());
}

// CopresenceSetAuthTokenFunction implementation
ExtensionFunction::ResponseAction CopresenceSetAuthTokenFunction::Run() {
  std::unique_ptr<SetAuthToken::Params> params(
      SetAuthToken::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // The token may be set to empty, to clear it.
  CopresenceService::GetFactoryInstance()->Get(browser_context())
      ->set_auth_token(extension_id(), params->token);
  return RespondNow(NoArguments());
}

}  // namespace extensions
