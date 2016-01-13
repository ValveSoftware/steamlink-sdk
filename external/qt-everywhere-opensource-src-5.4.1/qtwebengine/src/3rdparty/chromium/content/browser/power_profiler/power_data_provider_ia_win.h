// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_POWER_PROFILER_POWER_DATA_PROVIDER_IA_WIN_H_
#define CONTENT_BROWSER_POWER_PROFILER_POWER_DATA_PROVIDER_IA_WIN_H_

#include "content/browser/power_profiler/power_data_provider.h"
#include "third_party/power_gadget/PowerGadgetLib.h"

namespace content {

// A class used to get power usage via Power Gadget API.
class PowerDataProviderIA : public PowerDataProvider {
 public:
  PowerDataProviderIA();

  virtual ~PowerDataProviderIA();

  bool Initialize();
  virtual PowerEventVector GetData() OVERRIDE;
  virtual base::TimeDelta GetSamplingRate() OVERRIDE;

 private:
  CIntelPowerGadgetLib energy_lib_;

  int sockets_number_;
  int power_msr_ids_[PowerEvent::ID_COUNT];
  bool is_open_;
  DISALLOW_COPY_AND_ASSIGN(PowerDataProviderIA);
};

}  // namespace content

#endif  // CONTENT_BROWSER_POWER_PROFILER_POWER_DATA_PROVIDER_IA_WIN_H_
