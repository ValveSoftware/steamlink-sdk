// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_POWER_MANAGER_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_POWER_MANAGER_DBUS_CONSTANTS_H_

namespace power_manager {
// powerd
const char kPowerManagerInterface[] = "org.chromium.PowerManager";
const char kPowerManagerServicePath[] = "/org/chromium/PowerManager";
const char kPowerManagerServiceName[] = "org.chromium.PowerManager";
// Methods exposed by powerd.
const char kDecreaseScreenBrightnessMethod[] = "DecreaseScreenBrightness";
const char kIncreaseScreenBrightnessMethod[] = "IncreaseScreenBrightness";
const char kGetScreenBrightnessPercentMethod[] = "GetScreenBrightnessPercent";
const char kSetScreenBrightnessPercentMethod[] = "SetScreenBrightnessPercent";
const char kDecreaseKeyboardBrightnessMethod[] = "DecreaseKeyboardBrightness";
const char kIncreaseKeyboardBrightnessMethod[] = "IncreaseKeyboardBrightness";
const char kRequestRestartMethod[] = "RequestRestart";
const char kRequestShutdownMethod[] = "RequestShutdown";
const char kRequestSuspendMethod[] = "RequestSuspend";
const char kGetPowerSupplyPropertiesMethod[] = "GetPowerSupplyProperties";
const char kHandleUserActivityMethod[] = "HandleUserActivity";
const char kHandleVideoActivityMethod[] = "HandleVideoActivity";
const char kSetIsProjectingMethod[] = "SetIsProjecting";
const char kSetPolicyMethod[] = "SetPolicy";
const char kSetPowerSourceMethod[] = "SetPowerSource";
const char kRegisterSuspendDelayMethod[] = "RegisterSuspendDelay";
const char kUnregisterSuspendDelayMethod[] = "UnregisterSuspendDelay";
const char kHandleSuspendReadinessMethod[] = "HandleSuspendReadiness";
const char kRegisterDarkSuspendDelayMethod[] = "RegisterDarkSuspendDelay";
const char kUnregisterDarkSuspendDelayMethod[] = "UnregisterDarkSuspendDelay";
const char kHandleDarkSuspendReadinessMethod[] = "HandleDarkSuspendReadiness";
const char kHandlePowerButtonAcknowledgmentMethod[] =
    "HandlePowerButtonAcknowledgment";
const char kRecordDarkResumeWakeReasonMethod[] = "RecordDarkResumeWakeReason";
// Signals emitted by powerd.
const char kBrightnessChangedSignal[] = "BrightnessChanged";
const char kKeyboardBrightnessChangedSignal[] = "KeyboardBrightnessChanged";
const char kPeripheralBatteryStatusSignal[] = "PeripheralBatteryStatus";
const char kPowerSupplyPollSignal[] = "PowerSupplyPoll";
const char kSuspendImminentSignal[] = "SuspendImminent";
const char kDarkSuspendImminentSignal[] = "DarkSuspendImminent";
const char kSuspendDoneSignal[] = "SuspendDone";
const char kInputEventSignal[] = "InputEvent";
const char kIdleActionImminentSignal[] = "IdleActionImminent";
const char kIdleActionDeferredSignal[] = "IdleActionDeferred";
// Values
const int kBrightnessTransitionGradual = 1;
const int kBrightnessTransitionInstant = 2;
enum UserActivityType {
  USER_ACTIVITY_OTHER = 0,
  USER_ACTIVITY_BRIGHTNESS_UP_KEY_PRESS = 1,
  USER_ACTIVITY_BRIGHTNESS_DOWN_KEY_PRESS = 2,
  USER_ACTIVITY_VOLUME_UP_KEY_PRESS = 3,
  USER_ACTIVITY_VOLUME_DOWN_KEY_PRESS = 4,
  USER_ACTIVITY_VOLUME_MUTE_KEY_PRESS = 5,
};
enum RequestRestartReason {
  REQUEST_RESTART_FOR_USER = 0,
  REQUEST_RESTART_FOR_UPDATE = 1,
};
}  // namespace power_manager

#endif  // SYSTEM_API_DBUS_POWER_MANAGER_DBUS_CONSTANTS_H_
