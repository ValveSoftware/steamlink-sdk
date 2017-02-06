// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_USER_DATA_ARC_USER_DATA_SERVICE
#define COMPONENTS_ARC_USER_DATA_ARC_USER_DATA_SERVICE

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/account_id/account_id.h"

namespace arc {

class ArcBridgeService;

// This class controls the lifecycle of ARC user data, removing it when
// necessary.
class ArcUserDataService : public ArcService,
                           public ArcBridgeService::Observer {
 public:
  explicit ArcUserDataService(
      ArcBridgeService* arc_bridge_service,
      std::unique_ptr<BooleanPrefMember> arc_enabled_pref,
      const AccountId& account_id);
  ~ArcUserDataService() override;

  using ArcDataCallback = chromeos::SessionManagerClient::ArcCallback;

  // ArcBridgeService::Observer:
  // Called whenever the arc bridge is stopped to potentially wipe data if
  // the user has not opted in or it is required.
  void OnBridgeStopped(ArcBridgeService::StopReason reason) override;

  // Requires to wipe ARC user data after the next ARC bridge shutdown and call
  // |callback| with an operation result.
  void RequireUserDataWiped(const ArcDataCallback& callback);

 private:
  base::ThreadChecker thread_checker_;

  // Checks if ARC is both stopped and disabled (not opt-in) or data wipe is
  // required and triggers removal of user data.
  void WipeIfRequired();

  const std::unique_ptr<BooleanPrefMember> arc_enabled_pref_;

  // Account ID for the account for which we currently have opt-in information.
  AccountId primary_user_account_id_;

  // Set to true when user data wipe is required and set to false again after
  // user data has been wiped.
  // This ensures data is wiped even if the user tries to enable ARC before the
  // bridge has shut down.
  bool arc_user_data_wipe_required_ = false;

  // Callback object that is passed to RemoveArcData to be invoked with a
  // result of ARC user data wipe.
  // Set when ARC user data wipe is required by RequireUserDataWiped.
  ArcDataCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ArcUserDataService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_USER_DATA_ARC_USER_DATA_SERVICE
