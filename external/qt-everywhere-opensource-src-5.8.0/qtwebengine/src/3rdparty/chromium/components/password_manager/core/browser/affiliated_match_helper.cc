// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliated_match_helper.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliation_service.h"

namespace password_manager {

namespace {

// Returns whether or not |form| represents a credential for an Android
// application, and if so, returns the |facet_uri| of that application.
bool IsAndroidApplicationCredential(const autofill::PasswordForm& form,
                                    FacetURI* facet_uri) {
  DCHECK(facet_uri);
  if (form.scheme != autofill::PasswordForm::SCHEME_HTML)
    return false;

  *facet_uri = FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
  return facet_uri->IsValidAndroidFacetURI();
}

}  // namespace

// static
const int64_t AffiliatedMatchHelper::kInitializationDelayOnStartupInSeconds;

AffiliatedMatchHelper::AffiliatedMatchHelper(
    PasswordStore* password_store,
    std::unique_ptr<AffiliationService> affiliation_service)
    : password_store_(password_store),
      task_runner_for_waiting_(base::ThreadTaskRunnerHandle::Get()),
      affiliation_service_(std::move(affiliation_service)),
      weak_ptr_factory_(this) {}

AffiliatedMatchHelper::~AffiliatedMatchHelper() {
  if (password_store_)
    password_store_->RemoveObserver(this);
}

void AffiliatedMatchHelper::Initialize() {
  DCHECK(password_store_);
  DCHECK(affiliation_service_);
  task_runner_for_waiting_->PostDelayedTask(
      FROM_HERE, base::Bind(&AffiliatedMatchHelper::DoDeferredInitialization,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kInitializationDelayOnStartupInSeconds));
}

void AffiliatedMatchHelper::GetAffiliatedAndroidRealms(
    const autofill::PasswordForm& observed_form,
    const AffiliatedRealmsCallback& result_callback) {
  if (IsValidWebCredential(observed_form)) {
    FacetURI facet_uri(
        FacetURI::FromPotentiallyInvalidSpec(observed_form.signon_realm));
    affiliation_service_->GetAffiliations(
        facet_uri, AffiliationService::StrategyOnCacheMiss::FAIL,
        base::Bind(&AffiliatedMatchHelper::CompleteGetAffiliatedAndroidRealms,
                   weak_ptr_factory_.GetWeakPtr(), facet_uri, result_callback));
  } else {
    result_callback.Run(std::vector<std::string>());
  }
}

void AffiliatedMatchHelper::GetAffiliatedWebRealms(
    const autofill::PasswordForm& android_form,
    const AffiliatedRealmsCallback& result_callback) {
  if (IsValidAndroidCredential(android_form)) {
    affiliation_service_->GetAffiliations(
        FacetURI::FromPotentiallyInvalidSpec(android_form.signon_realm),
        AffiliationService::StrategyOnCacheMiss::FETCH_OVER_NETWORK,
        base::Bind(&AffiliatedMatchHelper::CompleteGetAffiliatedWebRealms,
                   weak_ptr_factory_.GetWeakPtr(), result_callback));
  } else {
    result_callback.Run(std::vector<std::string>());
  }
}

void AffiliatedMatchHelper::InjectAffiliatedWebRealms(
    ScopedVector<autofill::PasswordForm> forms,
    const PasswordFormsCallback& result_callback) {
  std::vector<autofill::PasswordForm*> android_credentials;
  for (auto* form : forms) {
    if (IsValidAndroidCredential(*form))
      android_credentials.push_back(form);
  }
  base::Closure on_get_all_realms(
      base::Bind(result_callback, base::Passed(&forms)));
  base::Closure barrier_closure =
      base::BarrierClosure(android_credentials.size(), on_get_all_realms);
  for (auto* form : android_credentials) {
    affiliation_service_->GetAffiliations(
        FacetURI::FromPotentiallyInvalidSpec(form->signon_realm),
        AffiliationService::StrategyOnCacheMiss::FAIL,
        base::Bind(&AffiliatedMatchHelper::CompleteInjectAffiliatedWebRealm,
                   weak_ptr_factory_.GetWeakPtr(), base::Unretained(form),
                   barrier_closure));
  }
}

void AffiliatedMatchHelper::CompleteInjectAffiliatedWebRealm(
    autofill::PasswordForm* form,
    base::Closure barrier_closure,
    const AffiliatedFacets& results,
    bool success) {
  // If there is a number of realms, choose the first in the list.
  if (success) {
    for (const FacetURI& affiliated_facet : results) {
      if (affiliated_facet.IsValidWebFacetURI()) {
        form->affiliated_web_realm = affiliated_facet.canonical_spec() + "/";
        break;
      }
    }
  }
  barrier_closure.Run();
}

void AffiliatedMatchHelper::TrimAffiliationCache() {
  affiliation_service_->TrimCache();
}

// static
bool AffiliatedMatchHelper::IsValidAndroidCredential(
    const autofill::PasswordForm& form) {
  return form.scheme == autofill::PasswordForm::SCHEME_HTML &&
         IsValidAndroidFacetURI(form.signon_realm);
}

// static
bool AffiliatedMatchHelper::IsValidWebCredential(
    const autofill::PasswordForm& form) {
  FacetURI facet_uri(FacetURI::FromPotentiallyInvalidSpec(form.signon_realm));
  return form.scheme == autofill::PasswordForm::SCHEME_HTML && form.ssl_valid &&
         facet_uri.IsValidWebFacetURI();
}

void AffiliatedMatchHelper::SetTaskRunnerUsedForWaitingForTesting(
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_for_waiting_ = task_runner;
}

void AffiliatedMatchHelper::DoDeferredInitialization() {
  // Must start observing for changes at the same time as when the snapshot is
  // taken to avoid inconsistencies due to any changes taking place in-between.
  password_store_->AddObserver(this);
  password_store_->GetAutofillableLogins(this);
  password_store_->GetBlacklistLogins(this);
}

void AffiliatedMatchHelper::CompleteGetAffiliatedAndroidRealms(
    const FacetURI& original_facet_uri,
    const AffiliatedRealmsCallback& result_callback,
    const AffiliatedFacets& results,
    bool success) {
  std::vector<std::string> affiliated_realms;
  if (success) {
    for (const FacetURI& affiliated_facet : results) {
      if (affiliated_facet != original_facet_uri &&
          affiliated_facet.IsValidAndroidFacetURI())
        // Facet URIs have no trailing slash, whereas realms do.
        affiliated_realms.push_back(affiliated_facet.canonical_spec() + "/");
    }
  }
  result_callback.Run(affiliated_realms);
}

void AffiliatedMatchHelper::CompleteGetAffiliatedWebRealms(
    const AffiliatedRealmsCallback& result_callback,
    const AffiliatedFacets& results,
    bool success) {
  std::vector<std::string> affiliated_realms;
  if (success) {
    for (const FacetURI& affiliated_facet : results) {
      if (affiliated_facet.IsValidWebFacetURI())
        // Facet URIs have no trailing slash, whereas realms do.
        affiliated_realms.push_back(affiliated_facet.canonical_spec() + "/");
    }
  }
  result_callback.Run(affiliated_realms);
}

void AffiliatedMatchHelper::OnLoginsChanged(
    const PasswordStoreChangeList& changes) {
  std::vector<FacetURI> facet_uris_to_trim;
  for (const PasswordStoreChange& change : changes) {
    FacetURI facet_uri;
    if (!IsAndroidApplicationCredential(change.form(), &facet_uri))
      continue;

    if (change.type() == PasswordStoreChange::ADD) {
      affiliation_service_->Prefetch(facet_uri, base::Time::Max());
    } else if (change.type() == PasswordStoreChange::REMOVE) {
      // Stop keeping affiliation information fresh for deleted Android logins,
      // and make a note to potentially remove any unneeded cached data later.
      facet_uris_to_trim.push_back(facet_uri);
      affiliation_service_->CancelPrefetch(facet_uri, base::Time::Max());
    }
  }

  // When the primary key for a login is updated, |changes| will contain both a
  // REMOVE and ADD change for that login. Cached affiliation data should not be
  // deleted in this case. A simple solution is to call TrimCacheForFacet()
  // always after Prefetch() calls -- the trimming logic will detect that there
  // is an active prefetch and not delete the corresponding data.
  for (const FacetURI& facet_uri : facet_uris_to_trim)
    affiliation_service_->TrimCacheForFacet(facet_uri);
}

void AffiliatedMatchHelper::OnGetPasswordStoreResults(
    ScopedVector<autofill::PasswordForm> results) {
  for (autofill::PasswordForm* form : results) {
    FacetURI facet_uri;
    if (IsAndroidApplicationCredential(*form, &facet_uri))
      affiliation_service_->Prefetch(facet_uri, base::Time::Max());
  }
}

}  // namespace password_manager
