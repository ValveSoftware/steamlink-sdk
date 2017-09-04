// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/ntp_snippets_status_service.h"

#include <string>

#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"

namespace ntp_snippets {

namespace {

const char kFetchingRequiresSignin[] = "fetching_requires_signin";
const char kFetchingRequiresSigninEnabled[] = "true";
const char kFetchingRequiresSigninDisabled[] = "false";

}  // namespace

NTPSnippetsStatusService::NTPSnippetsStatusService(
    SigninManagerBase* signin_manager,
    PrefService* pref_service)
    : snippets_status_(SnippetsStatus::EXPLICITLY_DISABLED),
      require_signin_(false),
      signin_manager_(signin_manager),
      pref_service_(pref_service),
      signin_observer_(this) {
  std::string param_value_str = variations::GetVariationParamValueByFeature(
      kArticleSuggestionsFeature, kFetchingRequiresSignin);
  if (param_value_str == kFetchingRequiresSigninEnabled) {
    require_signin_ = true;
  } else if (!param_value_str.empty() &&
             param_value_str != kFetchingRequiresSigninDisabled) {
    DLOG(WARNING) << "Unknow value for the variations parameter "
                  << kFetchingRequiresSignin << ": " << param_value_str;
  }
}

NTPSnippetsStatusService::~NTPSnippetsStatusService() = default;

// static
void NTPSnippetsStatusService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kEnableSnippets, true);
}

void NTPSnippetsStatusService::Init(
    const SnippetsStatusChangeCallback& callback) {
  DCHECK(snippets_status_change_callback_.is_null());

  snippets_status_change_callback_ = callback;

  // Notify about the current state before registering the observer, to make
  // sure we don't get a double notification due to an undefined start state.
  SnippetsStatus old_snippets_status = snippets_status_;
  snippets_status_ = GetSnippetsStatusFromDeps();
  snippets_status_change_callback_.Run(old_snippets_status, snippets_status_);

  signin_observer_.Add(signin_manager_);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kEnableSnippets,
      base::Bind(&NTPSnippetsStatusService::OnSnippetsEnabledChanged,
                 base::Unretained(this)));
}

void NTPSnippetsStatusService::OnSnippetsEnabledChanged() {
  OnStateChanged(GetSnippetsStatusFromDeps());
}

void NTPSnippetsStatusService::OnStateChanged(
    SnippetsStatus new_snippets_status) {
  if (new_snippets_status == snippets_status_)
    return;

  snippets_status_change_callback_.Run(snippets_status_, new_snippets_status);
  snippets_status_ = new_snippets_status;
}

bool NTPSnippetsStatusService::IsSignedIn() const {
  return signin_manager_ && signin_manager_->IsAuthenticated();
}

void NTPSnippetsStatusService::GoogleSigninSucceeded(
    const std::string& account_id,
    const std::string& username,
    const std::string& password) {
  OnStateChanged(GetSnippetsStatusFromDeps());
}

void NTPSnippetsStatusService::GoogleSignedOut(const std::string& account_id,
                                               const std::string& username) {
  OnStateChanged(GetSnippetsStatusFromDeps());
}

SnippetsStatus NTPSnippetsStatusService::GetSnippetsStatusFromDeps() const {
  if (!pref_service_->GetBoolean(prefs::kEnableSnippets)) {
    DVLOG(1) << "[GetNewSnippetsStatus] Disabled via pref";
    return SnippetsStatus::EXPLICITLY_DISABLED;
  }

  if (require_signin_ && !IsSignedIn()) {
    DVLOG(1) << "[GetNewSnippetsStatus] Signed out and disabled due to this.";
    return SnippetsStatus::SIGNED_OUT_AND_DISABLED;
  }

  DVLOG(1) << "[GetNewSnippetsStatus] Enabled, signed "
           << (IsSignedIn() ? "in" : "out");
  return IsSignedIn() ? SnippetsStatus::ENABLED_AND_SIGNED_IN
                      : SnippetsStatus::ENABLED_AND_SIGNED_OUT;
}

}  // namespace ntp_snippets
