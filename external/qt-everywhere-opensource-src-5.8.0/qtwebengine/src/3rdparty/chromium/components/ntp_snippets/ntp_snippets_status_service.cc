// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_status_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/sync_service.h"

namespace ntp_snippets {

NTPSnippetsStatusService::NTPSnippetsStatusService(
    SigninManagerBase* signin_manager,
    sync_driver::SyncService* sync_service)
    : disabled_reason_(DisabledReason::EXPLICITLY_DISABLED),
      signin_manager_(signin_manager),
      sync_service_(sync_service),
      sync_service_observer_(this) {}

NTPSnippetsStatusService::~NTPSnippetsStatusService() {}

void NTPSnippetsStatusService::Init(
    const DisabledReasonChangeCallback& callback) {
  DCHECK(disabled_reason_change_callback_.is_null());

  disabled_reason_change_callback_ = callback;

  // Notify about the current state before registering the observer, to make
  // sure we don't get a double notification due to an undefined start state.
  disabled_reason_ = GetDisabledReasonFromDeps();
  disabled_reason_change_callback_.Run(disabled_reason_);

  sync_service_observer_.Add(sync_service_);
}

void NTPSnippetsStatusService::OnStateChanged() {
  DisabledReason new_disabled_reason = GetDisabledReasonFromDeps();

  if (new_disabled_reason == disabled_reason_)
    return;

  disabled_reason_ = new_disabled_reason;
  disabled_reason_change_callback_.Run(disabled_reason_);
}

DisabledReason NTPSnippetsStatusService::GetDisabledReasonFromDeps() const {
  if (!signin_manager_ || !signin_manager_->IsAuthenticated()) {
    DVLOG(1) << "[GetNewDisabledReason] Signed out";
    return DisabledReason::SIGNED_OUT;
  }

  if (!sync_service_ || !sync_service_->CanSyncStart()) {
    DVLOG(1) << "[GetNewDisabledReason] Sync disabled";
    return DisabledReason::SYNC_DISABLED;
  }

  // !IsSyncActive in cases where CanSyncStart is true hints at the backend not
  // being initialized.
  // ConfigurationDone() verifies that the sync service has properly loaded its
  // configuration and is aware of the different data types to sync.
  if (!sync_service_->IsSyncActive() || !sync_service_->ConfigurationDone()) {
    DVLOG(1) << "[GetNewDisabledReason] Sync initialization is not complete.";
    return DisabledReason::HISTORY_SYNC_STATE_UNKNOWN;
  }

  if (sync_service_->IsEncryptEverythingEnabled()) {
    DVLOG(1) << "[GetNewDisabledReason] Encryption is enabled";
    return DisabledReason::PASSPHRASE_ENCRYPTION_ENABLED;
  }

  if (!sync_service_->GetActiveDataTypes().Has(
          syncer::HISTORY_DELETE_DIRECTIVES)) {
    DVLOG(1) << "[GetNewDisabledReason] History sync disabled";
    return DisabledReason::HISTORY_SYNC_DISABLED;
  }

  DVLOG(1) << "[GetNewDisabledReason] Enabled";
  return DisabledReason::NONE;
}

}  // namespace ntp_snippets
