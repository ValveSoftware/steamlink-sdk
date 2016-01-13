// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_profiler/power_data_provider_ia_win.h"

#include "base/basictypes.h"
#include "base/logging.h"

namespace content {

// Default sampling period, as recommended by Intel Power Gadget.
// Section 3.1 of
// http://software.intel.com/en-us/blogs/2013/10/03/using-the-intel-power-gadget-api-on-windows
const int kDefaultSamplePeriodMs = 50;

scoped_ptr<PowerDataProvider> PowerDataProvider::Create() {
  scoped_ptr<PowerDataProviderIA> provider(new PowerDataProviderIA());
  if (provider->Initialize())
    return make_scoped_ptr<PowerDataProvider>(provider.release());

  return make_scoped_ptr<PowerDataProvider>(NULL);
}

PowerDataProviderIA::PowerDataProviderIA()
    : sockets_number_(0),
      is_open_(false) {
  for (int i = 0; i < PowerEvent::ID_COUNT; i++ )
      power_msr_ids_[i] = -1;
}

PowerDataProviderIA::~PowerDataProviderIA() {
}

PowerEventVector PowerDataProviderIA::GetData() {
  PowerEventVector events;

  if (!energy_lib_.ReadSample())
    return events;

  PowerEvent event;
  double package_power = 0.0;
  double power[3];
  int data_count;

  for (int i = 0; i < sockets_number_; i++) {
    if (power_msr_ids_[PowerEvent::SOC_PACKAGE] == -1)
      break;

    energy_lib_.GetPowerData(i,
        power_msr_ids_[PowerEvent::SOC_PACKAGE], power, &data_count);
    package_power += power[0];
  }

  event.type = PowerEvent::SOC_PACKAGE;
  event.value = package_power;
  event.time = base::TimeTicks::Now();
  events.push_back(event);

  return events;
}

base::TimeDelta PowerDataProviderIA::GetSamplingRate() {
  return base::TimeDelta::FromMilliseconds(kDefaultSamplePeriodMs);
}

bool PowerDataProviderIA::Initialize() {
  if (is_open_)
    return true;

  if (!energy_lib_.IntelEnergyLibInitialize()) {
    LOG(ERROR) << "Power Data Provider initialize failed!";
    return false;
  }

  energy_lib_.GetNumNodes(&sockets_number_);

  const std::wstring package_msr_name = L"Processor";

  int msr_number;
  energy_lib_.GetNumMsrs(&msr_number);

  int func_id;
  wchar_t name[32];
  for(int i = 0; i < msr_number; i++) {
    energy_lib_.GetMsrFunc(i, &func_id);
    energy_lib_.GetMsrName(i, name);

    if (func_id != 1)
      continue;

    if (package_msr_name.compare(name) == 0)
      power_msr_ids_[PowerEvent::SOC_PACKAGE] = i;
  }

  is_open_ = true;
  return true;
}

}  // namespace content
