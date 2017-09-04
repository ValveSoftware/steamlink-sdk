// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_CT_POLICY_MANAGER_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_CT_POLICY_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/http/transport_security_state.h"

namespace base {
class SequencedTaskRunner;
}  // base

class PrefRegistrySimple;

namespace certificate_transparency {

// CTPolicyManager serves as the bridge between the Certificate Transparency
// preferences (see pref_names.h) and the actual implementation, by exposing
// a TransportSecurityState::RequireCTDelegate that can be used to query for
// CT-related policies.
class CTPolicyManager {
 public:
  // Registers the preferences related to Certificate Transparency policy
  // in the given pref registry.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Creates a CTPolicyManager that will monitor the preferences on
  // |pref_service| and make them available to a RequireCTDelegate that
  // can be used on |network_task_runner|.
  //
  // The CTPolicyManager should be constructed on the same task runner
  // associated with the |pref_service|, but can be destructed on any
  // task runner, provided that Shutdown() has been called.
  CTPolicyManager(PrefService* pref_service,
                  scoped_refptr<base::SequencedTaskRunner> network_task_runner);
  ~CTPolicyManager();

  // Unregisters the CTPolicyManager from the preference subsystem. This
  // should be called on the same task runner that the pref service
  // it was constructed with lives on.
  void Shutdown();

  // Returns a RequireCTDelegate that responds based on the policies set via
  // preferences.
  //
  // The order of priority of the preferences is that:
  //   - Specific hosts are preferred over those that match subdomains.
  //   - The most specific host is preferred.
  //   - Requiring CT is preferred over excluding CT
  //
  // This object MUST only be used on the network task runner supplied during
  // construction, MAY be used after Shutdown() is called (at which point,
  // it will reflect the last status before Shutdown() was called), and
  // MUST NOT be used after this object has been deleted.
  net::TransportSecurityState::RequireCTDelegate* GetDelegate();

 private:
  class CTDelegate;

  // Schedules an update of the CTPolicyDelegate. As it's possible that
  // multiple preferences may be updated at the same time, this exists to
  // schedule only a single update.
  void ScheduleUpdate();

  // Performs the actual update of the CTPolicyDelegate once preference
  // changes have quiesced.
  void Update();

  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<CTDelegate> delegate_;

  base::WeakPtrFactory<CTPolicyManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CTPolicyManager);
};

}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_CT_POLICY_MANAGER_H_
