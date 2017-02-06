// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notification_provider/notification_provider_api.h"

#include <utility>

#include "base/callback.h"
#include "base/guid.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "ui/base/layout.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notifier_settings.h"
#include "url/gurl.h"

namespace extensions {

NotificationProviderEventRouter::NotificationProviderEventRouter(
    Profile* profile)
    : profile_(profile) {
}

NotificationProviderEventRouter::~NotificationProviderEventRouter() {
}

void NotificationProviderEventRouter::CreateNotification(
    const std::string& notification_provider_id,
    const std::string& sender_id,
    const std::string& notification_id,
    const api::notifications::NotificationOptions& options) {
  Create(notification_provider_id, sender_id, notification_id, options);
}

void NotificationProviderEventRouter::UpdateNotification(
    const std::string& notification_provider_id,
    const std::string& sender_id,
    const std::string& notification_id,
    const api::notifications::NotificationOptions& options) {
  Update(notification_provider_id, sender_id, notification_id, options);
}
void NotificationProviderEventRouter::ClearNotification(
    const std::string& notification_provider_id,
    const std::string& sender_id,
    const std::string& notification_id) {
  Clear(notification_provider_id, sender_id, notification_id);
}

void NotificationProviderEventRouter::Create(
    const std::string& notification_provider_id,
    const std::string& sender_id,
    const std::string& notification_id,
    const api::notifications::NotificationOptions& options) {
  std::unique_ptr<base::ListValue> args =
      api::notification_provider::OnCreated::Create(sender_id, notification_id,
                                                    options);

  std::unique_ptr<Event> event(new Event(
      events::NOTIFICATION_PROVIDER_ON_CREATED,
      api::notification_provider::OnCreated::kEventName, std::move(args)));

  EventRouter::Get(profile_)
      ->DispatchEventToExtension(notification_provider_id, std::move(event));
}

void NotificationProviderEventRouter::Update(
    const std::string& notification_provider_id,
    const std::string& sender_id,
    const std::string& notification_id,
    const api::notifications::NotificationOptions& options) {
  std::unique_ptr<base::ListValue> args =
      api::notification_provider::OnUpdated::Create(sender_id, notification_id,
                                                    options);

  std::unique_ptr<Event> event(new Event(
      events::NOTIFICATION_PROVIDER_ON_UPDATED,
      api::notification_provider::OnUpdated::kEventName, std::move(args)));

  EventRouter::Get(profile_)
      ->DispatchEventToExtension(notification_provider_id, std::move(event));
}

void NotificationProviderEventRouter::Clear(
    const std::string& notification_provider_id,
    const std::string& sender_id,
    const std::string& notification_id) {
  std::unique_ptr<base::ListValue> args =
      api::notification_provider::OnCleared::Create(sender_id, notification_id);

  std::unique_ptr<Event> event(new Event(
      events::NOTIFICATION_PROVIDER_ON_CLEARED,
      api::notification_provider::OnCleared::kEventName, std::move(args)));

  EventRouter::Get(profile_)
      ->DispatchEventToExtension(notification_provider_id, std::move(event));
}

NotificationProviderNotifyOnClearedFunction::
    NotificationProviderNotifyOnClearedFunction() {
}

NotificationProviderNotifyOnClearedFunction::
    ~NotificationProviderNotifyOnClearedFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderNotifyOnClearedFunction::Run() {
  std::unique_ptr<api::notification_provider::NotifyOnCleared::Params> params =
      api::notification_provider::NotifyOnCleared::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Notification* notification =
      g_browser_process->notification_ui_manager()->FindById(
          params->notification_id,
          NotificationUIManager::GetProfileID(GetProfile()));

  bool found_notification = notification != NULL;
  if (found_notification)
    notification->delegate()->Close(true);

  return RespondNow(
      ArgumentList(api::notification_provider::NotifyOnCleared::Results::Create(
          found_notification)));
}

NotificationProviderNotifyOnClickedFunction::
    NotificationProviderNotifyOnClickedFunction() {
}

NotificationProviderNotifyOnClickedFunction::
    ~NotificationProviderNotifyOnClickedFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderNotifyOnClickedFunction::Run() {
  std::unique_ptr<api::notification_provider::NotifyOnClicked::Params> params =
      api::notification_provider::NotifyOnClicked::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Notification* notification =
      g_browser_process->notification_ui_manager()->FindById(
          params->notification_id,
          NotificationUIManager::GetProfileID(GetProfile()));

  bool found_notification = notification != NULL;
  if (found_notification)
    notification->delegate()->Click();

  return RespondNow(
      ArgumentList(api::notification_provider::NotifyOnClicked::Results::Create(
          found_notification)));
}

NotificationProviderNotifyOnButtonClickedFunction::
    NotificationProviderNotifyOnButtonClickedFunction() {
}

NotificationProviderNotifyOnButtonClickedFunction::
    ~NotificationProviderNotifyOnButtonClickedFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderNotifyOnButtonClickedFunction::Run() {
  std::unique_ptr<api::notification_provider::NotifyOnButtonClicked::Params>
      params =
          api::notification_provider::NotifyOnButtonClicked::Params::Create(
              *args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Notification* notification =
      g_browser_process->notification_ui_manager()->FindById(
          params->notification_id,
          NotificationUIManager::GetProfileID(GetProfile()));

  bool found_notification = notification != NULL;
  if (found_notification)
    notification->delegate()->ButtonClick(params->button_index);

  return RespondNow(ArgumentList(
      api::notification_provider::NotifyOnButtonClicked::Results::Create(
          found_notification)));
}

NotificationProviderNotifyOnPermissionLevelChangedFunction::
    NotificationProviderNotifyOnPermissionLevelChangedFunction() {
}

NotificationProviderNotifyOnPermissionLevelChangedFunction::
    ~NotificationProviderNotifyOnPermissionLevelChangedFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderNotifyOnPermissionLevelChangedFunction::Run() {
  std::unique_ptr<
      api::notification_provider::NotifyOnPermissionLevelChanged::Params>
      params = api::notification_provider::NotifyOnPermissionLevelChanged::
          Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Third party apps/extensions with notification provider API will not be able
  // to change permission levels of web notifiers, because the list of allowed
  // websites should only be set in Chrome Settings manually by users. But they
  // are able to change permission levels of application type notifiers.
  bool is_application_type =
      (params->notifier_type ==
       api::notification_provider::NotifierType::NOTIFIER_TYPE_APPLICATION);
  if (is_application_type) {
    bool enabled =
        (params->level == api::notification_provider::NotifierPermissionLevel::
                              NOTIFIER_PERMISSION_LEVEL_GRANTED);

    NotifierStateTracker* notifier_state_tracker =
        NotifierStateTrackerFactory::GetForProfile(GetProfile());

    message_center::NotifierId notifier_id(
        message_center::NotifierId::NotifierType::APPLICATION,
        params->notifier_id);

    notifier_state_tracker->SetNotifierEnabled(notifier_id, enabled);
  }

  return RespondNow(
      ArgumentList(api::notification_provider::NotifyOnPermissionLevelChanged::
                       Results::Create(is_application_type)));
}

NotificationProviderNotifyOnShowSettingsFunction::
    NotificationProviderNotifyOnShowSettingsFunction() {
}

NotificationProviderNotifyOnShowSettingsFunction::
    ~NotificationProviderNotifyOnShowSettingsFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderNotifyOnShowSettingsFunction::Run() {
  std::unique_ptr<api::notification_provider::NotifyOnShowSettings::Params>
      params = api::notification_provider::NotifyOnShowSettings::Params::Create(
          *args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool has_advanced_settings;
  // Only application type notifiers have advanced settings.
  if (params->notifier_type ==
      api::notification_provider::NotifierType::NOTIFIER_TYPE_APPLICATION) {
    // TODO(dewittj): Refactor NotificationUIManage API to have a getter of
    // NotifierSettingsProvider, since it holds the settings provider.
    message_center::NotifierSettingsProvider* settings_provider =
        message_center::MessageCenter::Get()->GetNotifierSettingsProvider();

    message_center::NotifierId notifier_id(
        message_center::NotifierId::NotifierType::APPLICATION,
        params->notifier_id);

    has_advanced_settings =
        settings_provider->NotifierHasAdvancedSettings(notifier_id);
    if (has_advanced_settings)
      settings_provider->OnNotifierAdvancedSettingsRequested(notifier_id, NULL);
  } else {
    has_advanced_settings = false;
  }

  return RespondNow(ArgumentList(
      api::notification_provider::NotifyOnShowSettings::Results::Create(
          has_advanced_settings)));
}

NotificationProviderGetNotifierFunction::
    NotificationProviderGetNotifierFunction() {
}

NotificationProviderGetNotifierFunction::
    ~NotificationProviderGetNotifierFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderGetNotifierFunction::Run() {
  api::notification_provider::Notifier notifier;

  return RespondNow(ArgumentList(
      api::notification_provider::GetNotifier::Results::Create(notifier)));
}

NotificationProviderGetAllNotifiersFunction::
    NotificationProviderGetAllNotifiersFunction() {
}

NotificationProviderGetAllNotifiersFunction::
    ~NotificationProviderGetAllNotifiersFunction() {
}

ExtensionFunction::ResponseAction
NotificationProviderGetAllNotifiersFunction::Run() {
  std::vector<api::notification_provider::Notifier> notifiers;

  return RespondNow(ArgumentList(
      api::notification_provider::GetAllNotifiers::Results::Create(notifiers)));
}

}  // namespace extensions
