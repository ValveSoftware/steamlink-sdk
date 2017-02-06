// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include <set>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/supports_user_data.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "chrome/browser/spellchecker/feedback_sender.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_platform.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_bdict_language.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_messages.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "ipc/ipc_platform_file.h"

using content::BrowserThread;

// TODO(rlp): I do not like globals, but keeping these for now during
// transition.
// An event used by browser tests to receive status events from this class and
// its derived classes.
base::WaitableEvent* g_status_event = NULL;
SpellcheckService::EventType g_status_type =
    SpellcheckService::BDICT_NOTINITIALIZED;

SpellcheckService::SpellcheckService(content::BrowserContext* context)
    : context_(context),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  pref_change_registrar_.Init(prefs);
  StringListPrefMember dictionaries_pref;
  dictionaries_pref.Init(prefs::kSpellCheckDictionaries, prefs);
  std::string first_of_dictionaries;

#if defined(USE_BROWSER_SPELLCHECKER)
  // Ensure that the renderer always knows the platform spellchecking language.
  // This language is used for initialization of the text iterator. If the
  // iterator is not initialized, then the context menu does not show spellcheck
  // suggestions.
  //
  // No migration is necessary, because the spellcheck language preference is
  // not user visible or modifiable in Chrome on Mac.
  dictionaries_pref.SetValue(std::vector<std::string>(
      1, spellcheck_platform::GetSpellCheckerLanguage()));
  first_of_dictionaries = dictionaries_pref.GetValue().front();
#else
  // Migrate preferences from single-language to multi-language schema.
  StringPrefMember single_dictionary_pref;
  single_dictionary_pref.Init(prefs::kSpellCheckDictionary, prefs);
  std::string single_dictionary = single_dictionary_pref.GetValue();

  if (!dictionaries_pref.GetValue().empty())
    first_of_dictionaries = dictionaries_pref.GetValue().front();

  if (first_of_dictionaries.empty() && !single_dictionary.empty()) {
    first_of_dictionaries = single_dictionary;
    dictionaries_pref.SetValue(
        std::vector<std::string>(1, first_of_dictionaries));
  }

  single_dictionary_pref.SetValue("");
#endif  // defined(USE_BROWSER_SPELLCHECKER)

  std::string language_code;
  std::string country_code;
  chrome::spellcheck_common::GetISOLanguageCountryCodeFromLocale(
      first_of_dictionaries,
      &language_code,
      &country_code);
  feedback_sender_.reset(new spellcheck::FeedbackSender(
      content::BrowserContext::GetDefaultStoragePartition(context)->
            GetURLRequestContext(),
      language_code, country_code));

  pref_change_registrar_.Add(
      prefs::kSpellCheckDictionaries,
      base::Bind(&SpellcheckService::OnSpellCheckDictionariesChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSpellCheckUseSpellingService,
      base::Bind(&SpellcheckService::OnUseSpellingServiceChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kAcceptLanguages,
      base::Bind(&SpellcheckService::OnAcceptLanguagesChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kEnableContinuousSpellcheck,
      base::Bind(&SpellcheckService::InitForAllRenderers,
                 base::Unretained(this)));

  custom_dictionary_.reset(new SpellcheckCustomDictionary(context_->GetPath()));
  custom_dictionary_->AddObserver(this);
  custom_dictionary_->Load();

  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());

  LoadHunspellDictionaries();
  UpdateFeedbackSenderState();
}

SpellcheckService::~SpellcheckService() {
  // Remove pref observers
  pref_change_registrar_.RemoveAll();
}

base::WeakPtr<SpellcheckService> SpellcheckService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

#if !defined(OS_MACOSX)
// static
void SpellcheckService::GetDictionaries(base::SupportsUserData* browser_context,
                                        std::vector<Dictionary>* dictionaries) {
  PrefService* prefs = user_prefs::UserPrefs::Get(browser_context);
  std::set<std::string> spellcheck_dictionaries;
  for (const auto& value : *prefs->GetList(prefs::kSpellCheckDictionaries)) {
    std::string dictionary;
    if (value->GetAsString(&dictionary))
      spellcheck_dictionaries.insert(dictionary);
  }

  dictionaries->clear();
  std::vector<std::string> accept_languages =
      base::SplitString(prefs->GetString(prefs::kAcceptLanguages), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& accept_language : accept_languages) {
    Dictionary dictionary;
    dictionary.language =
        chrome::spellcheck_common::GetCorrespondingSpellCheckLanguage(
            accept_language);
    if (dictionary.language.empty())
      continue;

    dictionary.used_for_spellcheck =
        spellcheck_dictionaries.count(dictionary.language) > 0;
    dictionaries->push_back(dictionary);
  }
}
#endif  // !OS_MACOSX

// static
bool SpellcheckService::SignalStatusEvent(
    SpellcheckService::EventType status_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!g_status_event)
    return false;
  g_status_type = status_type;
  g_status_event->Signal();
  return true;
}

void SpellcheckService::StartRecordingMetrics(bool spellcheck_enabled) {
  metrics_.reset(new SpellCheckHostMetrics());
  metrics_->RecordEnabledStats(spellcheck_enabled);
  OnUseSpellingServiceChanged();
}

void SpellcheckService::InitForRenderer(content::RenderProcessHost* process) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* context = process->GetBrowserContext();
  if (SpellcheckServiceFactory::GetForContext(context) != this)
    return;

  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  std::vector<SpellCheckBDictLanguage> bdict_languages;

  for (const auto& hunspell_dictionary : hunspell_dictionaries_) {
    bdict_languages.push_back(SpellCheckBDictLanguage());
    bdict_languages.back().language = hunspell_dictionary->GetLanguage();
    bdict_languages.back().file =
        hunspell_dictionary->GetDictionaryFile().IsValid()
            ? IPC::GetPlatformFileForTransit(
                  hunspell_dictionary->GetDictionaryFile().GetPlatformFile(),
                  false)
            : IPC::InvalidPlatformFileForTransit();
  }

  bool enabled = prefs->GetBoolean(prefs::kEnableContinuousSpellcheck) &&
                 !bdict_languages.empty();
  process->Send(new SpellCheckMsg_Init(
      bdict_languages,
      enabled ? custom_dictionary_->GetWords() : std::set<std::string>()));
  process->Send(new SpellCheckMsg_EnableSpellCheck(enabled));
}

SpellCheckHostMetrics* SpellcheckService::GetMetrics() const {
  return metrics_.get();
}

SpellcheckCustomDictionary* SpellcheckService::GetCustomDictionary() {
  return custom_dictionary_.get();
}

void SpellcheckService::LoadHunspellDictionaries() {
  hunspell_dictionaries_.clear();

  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  DCHECK(prefs);

  const base::ListValue* dictionary_values =
      prefs->GetList(prefs::kSpellCheckDictionaries);

  for (const auto& dictionary_value : *dictionary_values) {
    std::string dictionary;
    dictionary_value->GetAsString(&dictionary);
    hunspell_dictionaries_.push_back(new SpellcheckHunspellDictionary(
        dictionary,
        content::BrowserContext::GetDefaultStoragePartition(context_)->
            GetURLRequestContext(),
        this));
    hunspell_dictionaries_.back()->AddObserver(this);
    hunspell_dictionaries_.back()->Load();
  }
}

const ScopedVector<SpellcheckHunspellDictionary>&
SpellcheckService::GetHunspellDictionaries() {
  return hunspell_dictionaries_;
}

spellcheck::FeedbackSender* SpellcheckService::GetFeedbackSender() {
  return feedback_sender_.get();
}

bool SpellcheckService::LoadExternalDictionary(std::string language,
                                               std::string locale,
                                               std::string path,
                                               DictionaryFormat format) {
  return false;
}

bool SpellcheckService::UnloadExternalDictionary(
    const std::string& /* path */) {
  return false;
}

void SpellcheckService::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_RENDERER_PROCESS_CREATED, type);
  InitForRenderer(content::Source<content::RenderProcessHost>(source).ptr());
}

void SpellcheckService::OnCustomDictionaryLoaded() {
  InitForAllRenderers();
}

void SpellcheckService::OnCustomDictionaryChanged(
    const SpellcheckCustomDictionary::Change& dictionary_change) {
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->Send(new SpellCheckMsg_CustomDictionaryChanged(
        dictionary_change.to_add(),
        dictionary_change.to_remove()));
  }
}

void SpellcheckService::OnHunspellDictionaryInitialized(
    const std::string& language) {
  InitForAllRenderers();
}

void SpellcheckService::OnHunspellDictionaryDownloadBegin(
    const std::string& language) {
}

void SpellcheckService::OnHunspellDictionaryDownloadSuccess(
    const std::string& language) {
}

void SpellcheckService::OnHunspellDictionaryDownloadFailure(
    const std::string& language) {
#ifdef TOOLKIT_QT
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    context_->failedToLoadDictionary(language);
#endif
}

// static
void SpellcheckService::AttachStatusEvent(base::WaitableEvent* status_event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  g_status_event = status_event;
}

// static
SpellcheckService::EventType SpellcheckService::GetStatusEvent() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_status_type;
}

void SpellcheckService::InitForAllRenderers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    content::RenderProcessHost* process = i.GetCurrentValue();
    if (process && process->GetHandle())
      InitForRenderer(process);
  }
}

void SpellcheckService::OnSpellCheckDictionariesChanged() {
  // If there are hunspell dictionaries, then fire off notifications to the
  // renderers after the dictionaries are finished loading.
  LoadHunspellDictionaries();
  UpdateFeedbackSenderState();

  // If there are no hunspell dictionaries to load, then immediately let the
  // renderers know the new state.
  if (hunspell_dictionaries_.empty())
    InitForAllRenderers();
}

void SpellcheckService::OnUseSpellingServiceChanged() {
  bool enabled = pref_change_registrar_.prefs()->GetBoolean(
      prefs::kSpellCheckUseSpellingService);
  if (metrics_)
    metrics_->RecordSpellingServiceStats(enabled);
  UpdateFeedbackSenderState();
}

void SpellcheckService::OnAcceptLanguagesChanged() {
  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  std::vector<std::string> accept_languages =
      base::SplitString(prefs->GetString(prefs::kAcceptLanguages), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::transform(
      accept_languages.begin(), accept_languages.end(),
      accept_languages.begin(),
      &chrome::spellcheck_common::GetCorrespondingSpellCheckLanguage);

  StringListPrefMember dictionaries_pref;
  dictionaries_pref.Init(prefs::kSpellCheckDictionaries, prefs);
  std::vector<std::string> dictionaries = dictionaries_pref.GetValue();
  std::vector<std::string> filtered_dictionaries;

  for (const auto& dictionary : dictionaries) {
    if (std::find(accept_languages.begin(), accept_languages.end(),
                  dictionary) != accept_languages.end()) {
      filtered_dictionaries.push_back(dictionary);
    }
  }

  dictionaries_pref.SetValue(filtered_dictionaries);
}

void SpellcheckService::UpdateFeedbackSenderState() {
  std::string feedback_language;
  if (!hunspell_dictionaries_.empty())
    feedback_language = hunspell_dictionaries_.front()->GetLanguage();
  std::string language_code;
  std::string country_code;
  chrome::spellcheck_common::GetISOLanguageCountryCodeFromLocale(
      feedback_language, &language_code, &country_code);
  feedback_sender_->OnLanguageCountryChange(language_code, country_code);
  if (SpellingServiceClient::IsAvailable(
          context_, SpellingServiceClient::SPELLCHECK)) {
    feedback_sender_->StartFeedbackCollection();
  } else {
    feedback_sender_->StopFeedbackCollection();
  }
}
