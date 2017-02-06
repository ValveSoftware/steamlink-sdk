// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/alarms/alarms_api.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "extensions/browser/api/alarms/alarm_manager.h"
#include "extensions/browser/api/alarms/alarms_api_constants.h"
#include "extensions/common/api/alarms.h"
#include "extensions/common/error_utils.h"

namespace extensions {

namespace alarms = api::alarms;

namespace {

const char kDefaultAlarmName[] = "";
const char kBothRelativeAndAbsoluteTime[] =
    "Cannot set both when and delayInMinutes.";
const char kNoScheduledTime[] =
    "Must set at least one of when, delayInMinutes, or periodInMinutes.";

bool ValidateAlarmCreateInfo(const std::string& alarm_name,
                             const alarms::AlarmCreateInfo& create_info,
                             const Extension* extension,
                             std::string* error,
                             std::vector<std::string>* warnings) {
  if (create_info.delay_in_minutes.get() && create_info.when.get()) {
    *error = kBothRelativeAndAbsoluteTime;
    return false;
  }
  if (create_info.delay_in_minutes == NULL && create_info.when == NULL &&
      create_info.period_in_minutes == NULL) {
    *error = kNoScheduledTime;
    return false;
  }

  // Users can always use an absolute timeout to request an arbitrarily-short or
  // negative delay.  We won't honor the short timeout, but we can't check it
  // and warn the user because it would introduce race conditions (say they
  // compute a long-enough timeout, but then the call into the alarms interface
  // gets delayed past the boundary).  However, it's still worth warning about
  // relative delays that are shorter than we'll honor.
  if (create_info.delay_in_minutes.get()) {
    if (*create_info.delay_in_minutes <
        alarms_api_constants::kReleaseDelayMinimum) {
      if (Manifest::IsUnpackedLocation(extension->location())) {
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            alarms_api_constants::kWarningMinimumDevDelay, alarm_name));
      } else {
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            alarms_api_constants::kWarningMinimumReleaseDelay, alarm_name));
      }
    }
  }
  if (create_info.period_in_minutes.get()) {
    if (*create_info.period_in_minutes <
        alarms_api_constants::kReleaseDelayMinimum) {
      if (Manifest::IsUnpackedLocation(extension->location())) {
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            alarms_api_constants::kWarningMinimumDevPeriod, alarm_name));
      } else {
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            alarms_api_constants::kWarningMinimumReleasePeriod, alarm_name));
      }
    }
  }

  return true;
}

}  // namespace

AlarmsCreateFunction::AlarmsCreateFunction()
    : clock_(new base::DefaultClock()), owns_clock_(true) {
}

AlarmsCreateFunction::AlarmsCreateFunction(base::Clock* clock)
    : clock_(clock), owns_clock_(false) {
}

AlarmsCreateFunction::~AlarmsCreateFunction() {
  if (owns_clock_)
    delete clock_;
}

bool AlarmsCreateFunction::RunAsync() {
  std::unique_ptr<alarms::Create::Params> params(
      alarms::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const std::string& alarm_name =
      params->name.get() ? *params->name : kDefaultAlarmName;
  std::vector<std::string> warnings;
  if (!ValidateAlarmCreateInfo(alarm_name, params->alarm_info, extension(),
                               &error_, &warnings)) {
    return false;
  }
  for (std::vector<std::string>::const_iterator it = warnings.begin();
       it != warnings.end(); ++it)
    WriteToConsole(content::CONSOLE_MESSAGE_LEVEL_WARNING, *it);

  const int kSecondsPerMinute = 60;
  base::TimeDelta granularity =
      base::TimeDelta::FromSecondsD(
          (Manifest::IsUnpackedLocation(extension()->location())
               ? alarms_api_constants::kDevDelayMinimum
               : alarms_api_constants::kReleaseDelayMinimum)) *
      kSecondsPerMinute;

  std::unique_ptr<Alarm> alarm(
      new Alarm(alarm_name, params->alarm_info, granularity, clock_->Now()));
  AlarmManager::Get(browser_context())
      ->AddAlarm(extension_id(), std::move(alarm),
                 base::Bind(&AlarmsCreateFunction::Callback, this));

  return true;
}

void AlarmsCreateFunction::Callback() {
  SendResponse(true);
}

bool AlarmsGetFunction::RunAsync() {
  std::unique_ptr<alarms::Get::Params> params(
      alarms::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string name = params->name.get() ? *params->name : kDefaultAlarmName;
  AlarmManager::Get(browser_context())
      ->GetAlarm(extension_id(), name,
                 base::Bind(&AlarmsGetFunction::Callback, this, name));

  return true;
}

void AlarmsGetFunction::Callback(const std::string& name,
                                 extensions::Alarm* alarm) {
  if (alarm) {
    results_ = alarms::Get::Results::Create(*alarm->js_alarm);
  }
  SendResponse(true);
}

bool AlarmsGetAllFunction::RunAsync() {
  AlarmManager::Get(browser_context())
      ->GetAllAlarms(extension_id(),
                     base::Bind(&AlarmsGetAllFunction::Callback, this));
  return true;
}

void AlarmsGetAllFunction::Callback(const AlarmList* alarms) {
  std::unique_ptr<base::ListValue> alarms_value(new base::ListValue());
  if (alarms) {
    for (const std::unique_ptr<Alarm>& alarm : *alarms)
      alarms_value->Append(alarm->js_alarm->ToValue());
  }
  SetResult(std::move(alarms_value));
  SendResponse(true);
}

bool AlarmsClearFunction::RunAsync() {
  std::unique_ptr<alarms::Clear::Params> params(
      alarms::Clear::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string name = params->name.get() ? *params->name : kDefaultAlarmName;
  AlarmManager::Get(browser_context())
      ->RemoveAlarm(extension_id(), name,
                    base::Bind(&AlarmsClearFunction::Callback, this, name));

  return true;
}

void AlarmsClearFunction::Callback(const std::string& name, bool success) {
  SetResult(base::MakeUnique<base::FundamentalValue>(success));
  SendResponse(true);
}

bool AlarmsClearAllFunction::RunAsync() {
  AlarmManager::Get(browser_context())
      ->RemoveAllAlarms(extension_id(),
                        base::Bind(&AlarmsClearAllFunction::Callback, this));
  return true;
}

void AlarmsClearAllFunction::Callback() {
  SetResult(base::MakeUnique<base::FundamentalValue>(true));
  SendResponse(true);
}

}  // namespace extensions
