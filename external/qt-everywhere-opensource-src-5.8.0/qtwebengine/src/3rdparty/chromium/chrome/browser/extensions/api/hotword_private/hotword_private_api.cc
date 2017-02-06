// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/hotword_private/hotword_private_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_audio_history_handler.h"
#include "chrome/browser/search/hotword_client.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/speech_recognition_session_preamble.h"
#include "extensions/browser/event_router.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(OS_CHROMEOS)
#include "ash/common/system/chromeos/devicetype_utils.h"
#endif

namespace extensions {

namespace hotword_private_constants {
const char kHotwordServiceUnavailable[] = "Hotword Service is unavailable.";
const char kHotwordEventServiceUnavailable[] =
    "Hotword Private Event Service is unavailable.";
}  // hotword_private_constants

namespace OnEnabledChanged =
    api::hotword_private::OnEnabledChanged;

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<HotwordPrivateEventService> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

HotwordPrivateEventService::HotwordPrivateEventService(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kHotwordSearchEnabled,
      base::Bind(&HotwordPrivateEventService::OnEnabledChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kHotwordAlwaysOnSearchEnabled,
      base::Bind(&HotwordPrivateEventService::OnEnabledChanged,
                 base::Unretained(this)));
}

HotwordPrivateEventService::~HotwordPrivateEventService() {
}

void HotwordPrivateEventService::Shutdown() {
}

// static
BrowserContextKeyedAPIFactory<HotwordPrivateEventService>*
HotwordPrivateEventService::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
const char* HotwordPrivateEventService::service_name() {
  return "HotwordPrivateEventService";
}

void HotwordPrivateEventService::OnEnabledChanged(
    const std::string& pref_name) {
  DCHECK(pref_name == std::string(prefs::kHotwordSearchEnabled) ||
         pref_name == std::string(prefs::kHotwordAlwaysOnSearchEnabled) ||
         pref_name == std::string(
             hotword_internal::kHotwordTrainingEnabled));
  SignalEvent(events::HOTWORD_PRIVATE_ON_ENABLED_CHANGED,
              OnEnabledChanged::kEventName);
}

void HotwordPrivateEventService::OnHotwordSessionRequested() {
  SignalEvent(events::HOTWORD_PRIVATE_ON_HOTWORD_SESSION_REQUESTED,
              api::hotword_private::OnHotwordSessionRequested::kEventName);
}

void HotwordPrivateEventService::OnHotwordSessionStopped() {
  SignalEvent(events::HOTWORD_PRIVATE_ON_HOTWORD_SESSION_STOPPED,
              api::hotword_private::OnHotwordSessionStopped::kEventName);
}

void HotwordPrivateEventService::OnFinalizeSpeakerModel() {
  SignalEvent(events::HOTWORD_PRIVATE_ON_FINALIZE_SPEAKER_MODEL,
              api::hotword_private::OnFinalizeSpeakerModel::kEventName);
}

void HotwordPrivateEventService::OnSpeakerModelSaved() {
  SignalEvent(events::HOTWORD_PRIVATE_ON_SPEAKER_MODEL_SAVED,
              api::hotword_private::OnSpeakerModelSaved::kEventName);
}

void HotwordPrivateEventService::OnHotwordTriggered() {
  SignalEvent(events::HOTWORD_PRIVATE_ON_HOTWORD_TRIGGERED,
              api::hotword_private::OnHotwordTriggered::kEventName);
}

void HotwordPrivateEventService::OnDeleteSpeakerModel() {
  SignalEvent(events::HOTWORD_PRIVATE_ON_DELETE_SPEAKER_MODEL,
              api::hotword_private::OnDeleteSpeakerModel::kEventName);
}

void HotwordPrivateEventService::OnSpeakerModelExists() {
  SignalEvent(events::HOTWORD_PRIVATE_ON_SPEAKER_MODEL_EXISTS,
              api::hotword_private::OnSpeakerModelExists::kEventName);
}

void HotwordPrivateEventService::OnMicrophoneStateChanged(bool enabled) {
  SignalEvent(events::HOTWORD_PRIVATE_ON_MICROPHONE_STATE_CHANGED,
              api::hotword_private::OnMicrophoneStateChanged::kEventName,
              api::hotword_private::OnMicrophoneStateChanged::Create(enabled));
}

void HotwordPrivateEventService::SignalEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name) {
  SignalEvent(histogram_value, event_name,
              base::WrapUnique(new base::ListValue()));
}

void HotwordPrivateEventService::SignalEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> args) {
  EventRouter* router = EventRouter::Get(profile_);
  if (!router || !router->HasEventListener(event_name))
    return;

  std::unique_ptr<Event> event(
      new Event(histogram_value, event_name, std::move(args)));
  router->BroadcastEvent(std::move(event));
}

bool HotwordPrivateSetEnabledFunction::RunSync() {
  std::unique_ptr<api::hotword_private::SetEnabled::Params> params(
      api::hotword_private::SetEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kHotwordSearchEnabled, params->state);
  return true;
}

bool HotwordPrivateSetAudioLoggingEnabledFunction::RunSync() {
  std::unique_ptr<api::hotword_private::SetAudioLoggingEnabled::Params> params(
      api::hotword_private::SetAudioLoggingEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(kcarattini): Sync the chrome pref with the account-level
  // Audio History setting.
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kHotwordAudioLoggingEnabled, params->state);
  return true;
}

bool HotwordPrivateSetHotwordAlwaysOnSearchEnabledFunction::RunSync() {
  std::unique_ptr<api::hotword_private::SetHotwordAlwaysOnSearchEnabled::Params>
      params(
          api::hotword_private::SetHotwordAlwaysOnSearchEnabled::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled, params->state);
  return true;
}

bool HotwordPrivateGetStatusFunction::RunSync() {
  std::unique_ptr<api::hotword_private::GetStatus::Params> params(
      api::hotword_private::GetStatus::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  api::hotword_private::StatusDetails result;

  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service) {
    result.available = false;
    result.always_on_available = false;
    result.enabled = false;
    result.audio_logging_enabled = false;
    result.always_on_enabled = false;
    result.user_is_active = false;
    result.hotword_hardware_available = false;
  } else {
    result.available = false;
    result.always_on_available = false;
    if (params->get_optional_fields && *params->get_optional_fields) {
      result.available = hotword_service->IsServiceAvailable();
      result.always_on_available =
          HotwordServiceFactory::IsAlwaysOnAvailable();
    }
    result.enabled = hotword_service->IsSometimesOnEnabled();
    result.audio_logging_enabled = hotword_service->IsOptedIntoAudioLogging();
    result.training_enabled = hotword_service->IsTraining();
    result.always_on_enabled = hotword_service->IsAlwaysOnEnabled();
    result.user_is_active = hotword_service->UserIsActive();
    result.hotword_hardware_available =
        HotwordService::IsHotwordHardwareAvailable();
  }

  PrefService* prefs = GetProfile()->GetPrefs();
  result.enabled_set = prefs->HasPrefPath(prefs::kHotwordSearchEnabled);

  SetResult(result.ToValue());
  return true;
}

bool HotwordPrivateSetHotwordSessionStateFunction::RunSync() {
  std::unique_ptr<api::hotword_private::SetHotwordSessionState::Params> params(
      api::hotword_private::SetHotwordSessionState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (hotword_service &&
      hotword_service->client() &&
      !hotword_service->IsTraining())
    hotword_service->client()->OnHotwordStateChanged(params->started);
  return true;
}

bool HotwordPrivateNotifyHotwordRecognitionFunction::RunSync() {
  std::unique_ptr<api::hotword_private::NotifyHotwordRecognition::Params>
      params(api::hotword_private::NotifyHotwordRecognition::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_refptr<content::SpeechRecognitionSessionPreamble> preamble;
  if (params->log.get() &&
      !params->log->buffer.empty() &&
      params->log->channels == 1) {
    // TODO(amistry): Convert multi-channel preamble log into mono.
    preamble = new content::SpeechRecognitionSessionPreamble();
    preamble->sample_rate = params->log->sample_rate;
    preamble->sample_depth = params->log->bytes_per_sample;
    preamble->sample_data.swap(params->log->buffer);
  }

  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (hotword_service) {
    if (hotword_service->IsTraining()) {
      hotword_service->NotifyHotwordTriggered();
    } else if (hotword_service->client()) {
      hotword_service->client()->OnHotwordRecognized(preamble);
    } else if (hotword_service->IsAlwaysOnEnabled()) {
      AppListService* app_list_service = AppListService::Get();
      CHECK(app_list_service);
      app_list_service->ShowForVoiceSearch(GetProfile(), preamble);
    }
  }
  return true;
}

bool HotwordPrivateGetLaunchStateFunction::RunSync() {
  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service) {
    error_ = hotword_private_constants::kHotwordServiceUnavailable;
    return false;
  }

  api::hotword_private::LaunchState result;
  result.launch_mode =
      hotword_service->GetHotwordAudioVerificationLaunchMode();
  SetResult(result.ToValue());
  return true;
}

bool HotwordPrivateStartTrainingFunction::RunSync() {
  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service) {
    error_ = hotword_private_constants::kHotwordServiceUnavailable;
    return false;
  }

  hotword_service->StartTraining();
  return true;
}

bool HotwordPrivateFinalizeSpeakerModelFunction::RunSync() {
  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service) {
    error_ = hotword_private_constants::kHotwordServiceUnavailable;
    return false;
  }

  hotword_service->FinalizeSpeakerModel();
  return true;
}

bool HotwordPrivateNotifySpeakerModelSavedFunction::RunSync() {
  HotwordPrivateEventService* event_service =
      BrowserContextKeyedAPIFactory<HotwordPrivateEventService>::Get(
          GetProfile());
  if (!event_service) {
    error_ = hotword_private_constants::kHotwordEventServiceUnavailable;
    return false;
  }

  event_service->OnSpeakerModelSaved();
  return true;
}

bool HotwordPrivateStopTrainingFunction::RunSync() {
  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service) {
    error_ = hotword_private_constants::kHotwordServiceUnavailable;
    return false;
  }

  hotword_service->StopTraining();
  return true;
}

bool HotwordPrivateGetLocalizedStringsFunction::RunSync() {
#if defined(OS_CHROMEOS)
  base::string16 device_type = ash::GetChromeOSDeviceName();
#else
  base::string16 product_name =
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  base::string16 device_type =
      l10n_util::GetStringFUTF16(IDS_HOTWORD_BROWSER_NAME, product_name);
#endif

  std::unique_ptr<base::DictionaryValue> localized_strings(
      new base::DictionaryValue());

  localized_strings->SetString(
      "close",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_CLOSE));
  localized_strings->SetString(
      "cancel",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_CANCEL));
  localized_strings->SetString(
      "introTitle",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_INTRO_TITLE));
  localized_strings->SetString(
      "introSubtitle",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_INTRO_SUBTITLE));
  localized_strings->SetString(
      "introDescription",
      l10n_util::GetStringFUTF16(IDS_HOTWORD_OPT_IN_INTRO_DESCRIPTION,
                                 device_type));
  localized_strings->SetString(
      "introDescriptionAudioHistoryEnabled",
      l10n_util::GetStringFUTF16(
          IDS_HOTWORD_OPT_IN_INTRO_DESCRIPTION_AUDIO_HISTORY_ENABLED,
          device_type));
  localized_strings->SetString(
      "introStart",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_INTRO_START));
  localized_strings->SetString(
      "audioHistoryTitle",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_AUDIO_HISTORY_TITLE));
  localized_strings->SetString(
      "audioHistoryDescription1",
      l10n_util::GetStringUTF16(
          IDS_HOTWORD_OPT_IN_AUDIO_HISTORY_DESCRIPTION_1));
  localized_strings->SetString(
      "audioHistoryDescription2",
      l10n_util::GetStringUTF16(
          IDS_HOTWORD_OPT_IN_AUDIO_HISTORY_DESCRIPTION_2));
  localized_strings->SetString(
      "audioHistoryDescription3",
      l10n_util::GetStringUTF16(
          IDS_HOTWORD_OPT_IN_AUDIO_HISTORY_DESCRIPTION_3));
  localized_strings->SetString(
      "audioHistoryAgree",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_AUDIO_HISTORY_AGREE));
  localized_strings->SetString(
      "audioHistoryWait",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_AUDIO_HISTORY_WAIT));
  localized_strings->SetString(
      "error",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_ERROR));
  localized_strings->SetString(
      "trainingTitle", l10n_util::GetStringFUTF16(
                           IDS_HOTWORD_OPT_IN_TRAINING_TITLE, device_type));
  localized_strings->SetString(
      "trainingDescription",
      l10n_util::GetStringFUTF16(IDS_HOTWORD_OPT_IN_TRAINING_DESCRIPTION,
                                 device_type));
  localized_strings->SetString(
      "trainingSpeak",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_TRAINING_SPEAK));
  localized_strings->SetString(
      "trainingFirstPrompt",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_TRAINING_FIRST_PROMPT));
  localized_strings->SetString(
      "trainingMiddlePrompt",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_TRAINING_MIDDLE_PROMPT));
  localized_strings->SetString(
      "trainingLastPrompt",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_TRAINING_LAST_PROMPT));
  localized_strings->SetString(
      "trainingRecorded",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_TRAINING_RECORDED));
  localized_strings->SetString(
      "trainingTimeout",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_TRAINING_TIMEOUT));
  localized_strings->SetString(
      "trainingRetry",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_TRAINING_RETRY));
  localized_strings->SetString(
      "finishedTitle",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_FINISHED_TITLE));
  localized_strings->SetString(
      "finishedListIntro",
      l10n_util::GetStringFUTF16(IDS_HOTWORD_OPT_IN_FINISHED_LIST_INTRO,
                                 device_type));
  localized_strings->SetString(
      "finishedListItem1",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_FINISHED_LIST_ITEM_1));
  localized_strings->SetString(
      "finishedListItem2",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_FINISHED_LIST_ITEM_2));
  localized_strings->SetString(
      "finishedSettings",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_FINISHED_SETTINGS));
  localized_strings->SetString(
      "finishedAudioHistory",
      l10n_util::GetStringUTF16(
          IDS_HOTWORD_OPT_IN_FINISHED_AUDIO_HISTORY));
  localized_strings->SetString(
      "finish",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_FINISH));
  localized_strings->SetString(
      "finishedWait",
      l10n_util::GetStringUTF16(IDS_HOTWORD_OPT_IN_FINISHED_WAIT));

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, localized_strings.get());

  SetResult(std::move(localized_strings));
  return true;
}

bool HotwordPrivateSetAudioHistoryEnabledFunction::RunAsync() {
  std::unique_ptr<api::hotword_private::SetAudioHistoryEnabled::Params> params(
      api::hotword_private::SetAudioHistoryEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service || !hotword_service->GetAudioHistoryHandler()) {
    error_ = hotword_private_constants::kHotwordServiceUnavailable;
    return false;
  }

  hotword_service->GetAudioHistoryHandler()->SetAudioHistoryEnabled(
      params->enabled,
      base::Bind(
        &HotwordPrivateSetAudioHistoryEnabledFunction::SetResultAndSendResponse,
        this));
  return true;
}

void HotwordPrivateSetAudioHistoryEnabledFunction::SetResultAndSendResponse(
    bool success, bool new_enabled_value) {
  api::hotword_private::AudioHistoryState result;
  result.success = success;
  result.enabled = new_enabled_value;
  SetResult(result.ToValue());
  SendResponse(true);
}

bool HotwordPrivateGetAudioHistoryEnabledFunction::RunAsync() {
  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service || !hotword_service->GetAudioHistoryHandler()) {
    error_ = hotword_private_constants::kHotwordServiceUnavailable;
    return false;
  }

  hotword_service->GetAudioHistoryHandler()->GetAudioHistoryEnabled(base::Bind(
      &HotwordPrivateGetAudioHistoryEnabledFunction::SetResultAndSendResponse,
      this));

  return true;
}

void HotwordPrivateGetAudioHistoryEnabledFunction::SetResultAndSendResponse(
    bool success, bool new_enabled_value) {
  api::hotword_private::AudioHistoryState result;
  result.success = success;
  result.enabled = new_enabled_value;
  SetResult(result.ToValue());
  SendResponse(true);
}

bool HotwordPrivateSpeakerModelExistsResultFunction::RunSync() {
  std::unique_ptr<api::hotword_private::SpeakerModelExistsResult::Params>
      params(api::hotword_private::SpeakerModelExistsResult::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service)
    return false;

  hotword_service->SpeakerModelExistsComplete(params->exists);
  return true;
}

}  // namespace extensions
