// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/user_data/arc_user_data_service.h"

#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"

namespace arc {

ArcUserDataService::ArcUserDataService(
    ArcBridgeService* bridge_service,
    std::unique_ptr<BooleanPrefMember> arc_enabled_pref,
    const AccountId& account_id)
    : ArcService(bridge_service),
      arc_enabled_pref_(std::move(arc_enabled_pref)),
      primary_user_account_id_(account_id) {
  arc_bridge_service()->AddObserver(this);
  WipeIfRequired();
}

ArcUserDataService::~ArcUserDataService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->RemoveObserver(this);
}

void ArcUserDataService::OnBridgeStopped(ArcBridgeService::StopReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const AccountId& account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  if (account_id != primary_user_account_id_) {
    LOG(ERROR) << "User preferences not loaded for "
               << primary_user_account_id_.GetUserEmail()
               << ", but current primary user is " << account_id.GetUserEmail();
    primary_user_account_id_ = EmptyAccountId();
    return;
  }
  WipeIfRequired();
}

void ArcUserDataService::RequireUserDataWiped(const ArcDataCallback& callback) {
  VLOG(1) << "Require ARC user data to be wiped.";
  arc_user_data_wipe_required_ = true;
  callback_ = callback;
}

void ArcUserDataService::WipeIfRequired() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (arc_bridge_service()->state() != ArcBridgeService::State::STOPPED) {
    LOG(ERROR) << "ARC instance not stopped, user data can't be wiped";
    return;
  }
  if (arc_enabled_pref_->GetValue() && !arc_user_data_wipe_required_)
    return;
  VLOG(1) << "Wipe ARC user data.";
  arc_user_data_wipe_required_ = false;
  const cryptohome::Identification cryptohome_id(primary_user_account_id_);
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->RemoveArcData(cryptohome_id, callback_);
  callback_.Reset();
}

}  // namespace arc
