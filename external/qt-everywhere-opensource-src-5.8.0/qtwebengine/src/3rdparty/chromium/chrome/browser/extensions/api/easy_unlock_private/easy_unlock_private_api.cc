// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_api.h"

#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_connection_manager.h"
#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_crypto_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/easy_unlock_screenlock_state_handler.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/signin/easy_unlock_service_regular.h"
#include "chrome/browser/ui/proximity_auth/proximity_auth_error_bubble.h"
#include "chrome/common/extensions/api/easy_unlock_private.h"
#include "chrome/grit/generated_resources.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_connection.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_connection_finder.h"
#include "components/proximity_auth/bluetooth_throttler_impl.h"
#include "components/proximity_auth/bluetooth_util.h"
#include "components/proximity_auth/cryptauth/cryptauth_device_manager.h"
#include "components/proximity_auth/cryptauth/cryptauth_enrollment_manager.h"
#include "components/proximity_auth/cryptauth/cryptauth_enrollment_utils.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"
#include "components/proximity_auth/cryptauth/secure_message_delegate.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/proximity_auth_client.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "components/proximity_auth/screenlock_state.h"
#include "components/proximity_auth/switches.h"
#include "components/signin/core/account_id/account_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

#if defined(OS_CHROMEOS)
#include "ash/common/system/chromeos/devicetype_utils.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_tpm_key_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_tpm_key_manager_factory.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

using proximity_auth::ScreenlockState;

namespace extensions {

namespace easy_unlock_private = api::easy_unlock_private;

namespace {

static base::LazyInstance<BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// Utility method for getting the API's crypto delegate.
EasyUnlockPrivateCryptoDelegate* GetCryptoDelegate(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>::Get(context)
      ->GetCryptoDelegate();
}

EasyUnlockPrivateConnectionManager* GetConnectionManager(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>::Get(context)
      ->get_connection_manager();
}

ScreenlockState ToScreenlockState(easy_unlock_private::State state) {
  switch (state) {
    case easy_unlock_private::STATE_NO_BLUETOOTH:
      return ScreenlockState::NO_BLUETOOTH;
    case easy_unlock_private::STATE_BLUETOOTH_CONNECTING:
      return ScreenlockState::BLUETOOTH_CONNECTING;
    case easy_unlock_private::STATE_NO_PHONE:
      return ScreenlockState::NO_PHONE;
    case easy_unlock_private::STATE_PHONE_NOT_AUTHENTICATED:
      return ScreenlockState::PHONE_NOT_AUTHENTICATED;
    case easy_unlock_private::STATE_PHONE_LOCKED:
      return ScreenlockState::PHONE_LOCKED;
    case easy_unlock_private::STATE_PHONE_UNLOCKABLE:
      return ScreenlockState::PHONE_NOT_LOCKABLE;
    case easy_unlock_private::STATE_PHONE_UNSUPPORTED:
      return ScreenlockState::PHONE_UNSUPPORTED;
    case easy_unlock_private::STATE_RSSI_TOO_LOW:
      return ScreenlockState::RSSI_TOO_LOW;
    case easy_unlock_private::STATE_TX_POWER_TOO_HIGH:
      return ScreenlockState::TX_POWER_TOO_HIGH;
    case easy_unlock_private::STATE_PHONE_LOCKED_AND_TX_POWER_TOO_HIGH:
      return ScreenlockState::PHONE_LOCKED_AND_TX_POWER_TOO_HIGH;
    case easy_unlock_private::STATE_AUTHENTICATED:
      return ScreenlockState::AUTHENTICATED;
    default:
      return ScreenlockState::INACTIVE;
  }
}

}  // namespace

// static
BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>*
    EasyUnlockPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

EasyUnlockPrivateAPI::EasyUnlockPrivateAPI(content::BrowserContext* context)
    : connection_manager_(new EasyUnlockPrivateConnectionManager(context)) {}

EasyUnlockPrivateAPI::~EasyUnlockPrivateAPI() {}

EasyUnlockPrivateCryptoDelegate* EasyUnlockPrivateAPI::GetCryptoDelegate() {
  if (!crypto_delegate_)
    crypto_delegate_ = EasyUnlockPrivateCryptoDelegate::Create();
  return crypto_delegate_.get();
}

EasyUnlockPrivateGetStringsFunction::EasyUnlockPrivateGetStringsFunction() {
}
EasyUnlockPrivateGetStringsFunction::~EasyUnlockPrivateGetStringsFunction() {
}

bool EasyUnlockPrivateGetStringsFunction::RunSync() {
  std::unique_ptr<base::DictionaryValue> strings(new base::DictionaryValue);

#if defined(OS_CHROMEOS)
  const base::string16 device_type = ash::GetChromeOSDeviceName();
#else
  // TODO(isherman): Set an appropriate device name for non-ChromeOS devices.
  const base::string16 device_type = base::ASCIIToUTF16("Chromeschnozzle");
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
  const user_manager::UserManager* manager = user_manager::UserManager::Get();
  const user_manager::User* user = manager ? manager->GetActiveUser() : NULL;
  const std::string user_email_utf8 =
      user ? user->display_email() : std::string();
  const base::string16 user_email = base::UTF8ToUTF16(user_email_utf8);
#else
  // TODO(isherman): Set an appropriate user display email for non-ChromeOS
  // platforms.
  const base::string16 user_email = base::UTF8ToUTF16("superman@example.com");
#endif  // defined(OS_CHROMEOS)

  // Common strings.
  strings->SetString(
      "learnMoreLinkTitle",
      l10n_util::GetStringUTF16(IDS_EASY_UNLOCK_LEARN_MORE_LINK_TITLE));
  strings->SetString("deviceType", device_type);

  // Setup notification strings.
  strings->SetString(
      "setupNotificationTitle",
      l10n_util::GetStringUTF16(IDS_EASY_UNLOCK_SETUP_NOTIFICATION_TITLE));
  strings->SetString(
      "setupNotificationMessage",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_NOTIFICATION_MESSAGE,
                                 device_type));
  strings->SetString(
      "setupNotificationButtonTitle",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_NOTIFICATION_BUTTON_TITLE));

  // Chromebook added to Easy Unlock notification strings.
  strings->SetString(
      "chromebookAddedNotificationTitle",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_TITLE));
  strings->SetString(
      "chromebookAddedNotificationMessage",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_MESSAGE,
          device_type));
  strings->SetString(
      "chromebookAddedNotificationAboutButton",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON));

  // Shared "Learn more" button for the pairing changed and pairing change
  // applied notification.
  strings->SetString(
      "phoneChangedNotificationLearnMoreButton",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_NOTIFICATION_LEARN_MORE_BUTTON));

  // Pairing changed notification strings.
  strings->SetString(
      "phoneChangedNotificationTitle",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_TITLE));
  strings->SetString(
      "phoneChangedNotificationMessage",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_MESSAGE,
          device_type));
  strings->SetString(
      "phoneChangedNotificationUpdateButton",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_UPDATE_BUTTON));

  // Phone change applied notification strings.
  strings->SetString(
      "phoneChangeAppliedNotificationTitle",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGE_APPLIED_NOTIFICATION_TITLE));
  strings->SetString(
      "phoneChangeAppliedNotificationMessage",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGE_APPLIED_NOTIFICATION_MESSAGE));

  // Setup dialog strings.
  // Step 1: Intro.
  strings->SetString(
      "setupIntroHeaderTitle",
      l10n_util::GetStringUTF16(IDS_EASY_UNLOCK_SETUP_INTRO_HEADER_TITLE));
  strings->SetString(
      "setupIntroHeaderText",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_INTRO_HEADER_TEXT,
                                 device_type,
                                 user_email));
  strings->SetString(
      "setupIntroFindPhoneButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_FIND_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupIntroFindingPhoneButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_FINDING_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupIntroRetryFindPhoneButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_RETRY_FIND_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupIntroCloseFindPhoneButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_CLOSE_FIND_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupIntroHowIsThisSecureLinkText",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_HOW_IS_THIS_SECURE_LINK_TEXT));
  // Step 1.5: Phone found but is not secured with lock screen
  strings->SetString("setupSecurePhoneHeaderTitle",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_SECURE_PHONE_HEADER_TITLE));
  strings->SetString(
      "setupSecurePhoneHeaderText",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_SECURE_PHONE_HEADER_TEXT,
                                 device_type));
  strings->SetString("setupSecurePhoneButtonLabel",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_SECURE_PHONE_BUTTON_LABEL));
  strings->SetString("setupSecurePhoneLinkText",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_SECURE_PHONE_LINK_TEXT));
  // Step 2: Found a viable phone.
  strings->SetString(
      "setupFoundPhoneHeaderTitle",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_FOUND_PHONE_HEADER_TITLE, device_type));
  strings->SetString(
      "setupFoundPhoneHeaderText",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_FOUND_PHONE_HEADER_TEXT, device_type));
  strings->SetString(
      "setupFoundPhoneUseThisPhoneButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_FOUND_PHONE_USE_THIS_PHONE_BUTTON_LABEL));
  strings->SetString("setupFoundPhoneDeviceFormattedButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_FOUND_PHONE_DEVICE_FORMATTED_BUTTON_LABEL));
  strings->SetString(
      "setupFoundPhoneSwitchPhoneLinkLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_FOUND_PHONE_SWITCH_PHONE_LINK_LABEL));
  strings->SetString(
      "setupPairingPhoneFailedButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_PAIRING_PHONE_FAILED_BUTTON_LABEL));
  // Step 2.5: Recommend user to set up Android Smart Lock
  strings->SetString(
      "setupAndroidSmartLockHeaderTitle",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_ANDROID_SMART_LOCK_HEADER_TITLE));
  strings->SetString(
      "setupAndroidSmartLockHeaderText",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_ANDROID_SMART_LOCK_HEADER_TEXT, device_type));
  strings->SetString(
      "setupAndroidSmartLockDoneButtonText",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_ANDROID_SMART_LOCK_DONE_BUTTON_LABEL));
  strings->SetString(
      "setupAndroidSmartLockAboutLinkText",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_ANDROID_SMART_LOCK_ABOUT_LINK_TEXT));
  // Step 3: Setup completed successfully.
  strings->SetString(
      "setupCompleteHeaderTitle",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_COMPLETE_HEADER_TITLE));
  strings->SetString(
      "setupCompleteHeaderText",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_COMPLETE_HEADER_TEXT));
  strings->SetString(
      "setupCompleteTryItOutButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_COMPLETE_TRY_IT_OUT_BUTTON_LABEL));
  strings->SetString(
      "setupCompleteSettingsLinkText",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_COMPLETE_SETTINGS_LINK_TEXT));
  // Step 4: Post lockscreen confirmation.
  strings->SetString("setupPostLockDismissButtonLabel",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_POST_LOCK_DISMISS_BUTTON_LABEL));

  // Error strings.
  strings->SetString(
      "setupErrorBluetoothUnavailable",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_ERROR_BLUETOOTH_UNAVAILBLE, device_type));
  strings->SetString(
      "setupErrorOffline",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_ERROR_OFFLINE, device_type));
  strings->SetString(
      "setupErrorRemoteSoftwareOutOfDate",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_ERROR_REMOTE_SOFTWARE_OUT_OF_DATE));
  strings->SetString(
      "setupErrorRemoteSoftwareOutOfDateGeneric",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_ERROR_REMOTE_SOFTWARE_OUT_OF_DATE_GENERIC));
  strings->SetString(
      "setupErrorFindingPhone",
      l10n_util::GetStringUTF16(IDS_EASY_UNLOCK_SETUP_ERROR_FINDING_PHONE));
  strings->SetString("setupErrorSyncPhoneState",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_ERROR_SYNC_PHONE_STATE_FAILED));
  strings->SetString("setupErrorConnectingToPhone",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_ERROR_CONNECTING_TO_PHONE));

  SetResult(std::move(strings));
  return true;
}

EasyUnlockPrivatePerformECDHKeyAgreementFunction::
EasyUnlockPrivatePerformECDHKeyAgreementFunction() {}

EasyUnlockPrivatePerformECDHKeyAgreementFunction::
~EasyUnlockPrivatePerformECDHKeyAgreementFunction() {}

bool EasyUnlockPrivatePerformECDHKeyAgreementFunction::RunAsync() {
  std::unique_ptr<easy_unlock_private::PerformECDHKeyAgreement::Params> params =
      easy_unlock_private::PerformECDHKeyAgreement::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetCryptoDelegate(browser_context())->PerformECDHKeyAgreement(
      *params,
      base::Bind(&EasyUnlockPrivatePerformECDHKeyAgreementFunction::OnData,
                 this));
  return true;
}

void EasyUnlockPrivatePerformECDHKeyAgreementFunction::OnData(
    const std::string& secret_key) {
  // TODO(tbarzic): Improve error handling.
  if (!secret_key.empty()) {
    results_ = easy_unlock_private::PerformECDHKeyAgreement::Results::Create(
        std::vector<char>(secret_key.begin(), secret_key.end()));
  }
  SendResponse(true);
}

EasyUnlockPrivateGenerateEcP256KeyPairFunction::
EasyUnlockPrivateGenerateEcP256KeyPairFunction() {}

EasyUnlockPrivateGenerateEcP256KeyPairFunction::
~EasyUnlockPrivateGenerateEcP256KeyPairFunction() {}

bool EasyUnlockPrivateGenerateEcP256KeyPairFunction::RunAsync() {
  GetCryptoDelegate(browser_context())->GenerateEcP256KeyPair(
      base::Bind(&EasyUnlockPrivateGenerateEcP256KeyPairFunction::OnData,
                 this));
  return true;
}

void EasyUnlockPrivateGenerateEcP256KeyPairFunction::OnData(
    const std::string& private_key,
    const std::string& public_key) {
  // TODO(tbarzic): Improve error handling.
  if (!public_key.empty() && !private_key.empty()) {
    results_ = easy_unlock_private::GenerateEcP256KeyPair::Results::Create(
        std::vector<char>(public_key.begin(), public_key.end()),
        std::vector<char>(private_key.begin(), private_key.end()));
  }
  SendResponse(true);
}

EasyUnlockPrivateCreateSecureMessageFunction::
EasyUnlockPrivateCreateSecureMessageFunction() {}

EasyUnlockPrivateCreateSecureMessageFunction::
~EasyUnlockPrivateCreateSecureMessageFunction() {}

bool EasyUnlockPrivateCreateSecureMessageFunction::RunAsync() {
  std::unique_ptr<easy_unlock_private::CreateSecureMessage::Params> params =
      easy_unlock_private::CreateSecureMessage::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetCryptoDelegate(browser_context())->CreateSecureMessage(
      *params,
      base::Bind(&EasyUnlockPrivateCreateSecureMessageFunction::OnData,
                 this));
  return true;
}

void EasyUnlockPrivateCreateSecureMessageFunction::OnData(
    const std::string& message) {
  // TODO(tbarzic): Improve error handling.
  if (!message.empty()) {
    results_ = easy_unlock_private::CreateSecureMessage::Results::Create(
        std::vector<char>(message.begin(), message.end()));
  }
  SendResponse(true);
}

EasyUnlockPrivateUnwrapSecureMessageFunction::
EasyUnlockPrivateUnwrapSecureMessageFunction() {}

EasyUnlockPrivateUnwrapSecureMessageFunction::
~EasyUnlockPrivateUnwrapSecureMessageFunction() {}

bool EasyUnlockPrivateUnwrapSecureMessageFunction::RunAsync() {
  std::unique_ptr<easy_unlock_private::UnwrapSecureMessage::Params> params =
      easy_unlock_private::UnwrapSecureMessage::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetCryptoDelegate(browser_context())->UnwrapSecureMessage(
      *params,
      base::Bind(&EasyUnlockPrivateUnwrapSecureMessageFunction::OnData,
                 this));
  return true;
}

void EasyUnlockPrivateUnwrapSecureMessageFunction::OnData(
    const std::string& data) {
  // TODO(tbarzic): Improve error handling.
  if (!data.empty()) {
    results_ = easy_unlock_private::UnwrapSecureMessage::Results::Create(
        std::vector<char>(data.begin(), data.end()));
  }
  SendResponse(true);
}

EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction::
    EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction() {}

EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction::
    ~EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction() {}

bool EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction::RunAsync() {
  std::unique_ptr<easy_unlock_private::SeekBluetoothDeviceByAddress::Params>
      params(easy_unlock_private::SeekBluetoothDeviceByAddress::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  proximity_auth::bluetooth_util::SeekDeviceByAddress(
      params->device_address,
      base::Bind(
          &EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction::OnSeekSuccess,
          this),
      base::Bind(
          &EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction::OnSeekFailure,
          this),
      content::BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)
          .get());
  return true;
}

void EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction::OnSeekSuccess() {
  SendResponse(true);
}

void EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction::OnSeekFailure(
    const std::string& error_message) {
  SetError(error_message);
  SendResponse(false);
}

EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction::
    EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction() {}

EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction::
    ~EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction() {}

void EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction::
    ConnectToService(device::BluetoothDevice* device,
                     const device::BluetoothUUID& uuid) {
  device->ConnectToServiceInsecurely(
      uuid,
      base::Bind(&EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction::
                     OnConnect,
                 this),
      base::Bind(&EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction::
                     OnConnectError,
                 this));
}

EasyUnlockPrivateUpdateScreenlockStateFunction::
    EasyUnlockPrivateUpdateScreenlockStateFunction() {}

EasyUnlockPrivateUpdateScreenlockStateFunction::
    ~EasyUnlockPrivateUpdateScreenlockStateFunction() {}

bool EasyUnlockPrivateUpdateScreenlockStateFunction::RunSync() {
  std::unique_ptr<easy_unlock_private::UpdateScreenlockState::Params> params(
      easy_unlock_private::UpdateScreenlockState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (EasyUnlockService::Get(profile)->UpdateScreenlockState(
          ToScreenlockState(params->state)))
    return true;

  SetError("Not allowed");
  return false;
}

EasyUnlockPrivateSetPermitAccessFunction::
    EasyUnlockPrivateSetPermitAccessFunction() {
}

EasyUnlockPrivateSetPermitAccessFunction::
    ~EasyUnlockPrivateSetPermitAccessFunction() {
}

bool EasyUnlockPrivateSetPermitAccessFunction::RunSync() {
  std::unique_ptr<easy_unlock_private::SetPermitAccess::Params> params(
      easy_unlock_private::SetPermitAccess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  EasyUnlockService::Get(profile)
      ->SetPermitAccess(*params->permit_access.ToValue());

  return true;
}

EasyUnlockPrivateGetPermitAccessFunction::
    EasyUnlockPrivateGetPermitAccessFunction() {
}

EasyUnlockPrivateGetPermitAccessFunction::
    ~EasyUnlockPrivateGetPermitAccessFunction() {
}

bool EasyUnlockPrivateGetPermitAccessFunction::RunSync() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          proximity_auth::switches::kEnableBluetoothLowEnergyDiscovery)) {
    ReturnPermitAccessForExperiment();
    return true;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const base::DictionaryValue* permit_value =
      EasyUnlockService::Get(profile)->GetPermitAccess();
  if (permit_value) {
    std::unique_ptr<easy_unlock_private::PermitRecord> permit =
        easy_unlock_private::PermitRecord::FromValue(*permit_value);
    results_ = easy_unlock_private::GetPermitAccess::Results::Create(*permit);
  }

  return true;
}

void EasyUnlockPrivateGetPermitAccessFunction::GetKeyPairForExperiment(
    std::string* user_public_key,
    std::string* user_private_key) {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  proximity_auth::CryptAuthEnrollmentManager* enrollment_manager =
      EasyUnlockService::Get(profile)
          ->proximity_auth_client()
          ->GetCryptAuthEnrollmentManager();
  base::Base64UrlEncode(enrollment_manager->GetUserPublicKey(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        user_public_key);
  base::Base64UrlEncode(enrollment_manager->GetUserPrivateKey(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        user_private_key);
}

void EasyUnlockPrivateGetPermitAccessFunction::
    ReturnPermitAccessForExperiment() {
  // Check that we are inside a user session.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  EasyUnlockService* easy_unlock_service = EasyUnlockService::Get(profile);
  if (easy_unlock_service->GetType() != EasyUnlockService::TYPE_REGULAR) {
    SetError("This function must be called inside a user session.");
    SendResponse(true);
    return;
  }

  std::string b64_public_key, b64_private_key;
  GetKeyPairForExperiment(&b64_public_key, &b64_private_key);

  // Fill in the permit access JSON dictionary.
  proximity_auth::ProximityAuthClient* client =
      easy_unlock_service->proximity_auth_client();
  std::unique_ptr<base::DictionaryValue> permit_access(
      new base::DictionaryValue());
  permit_access->SetString("permitId",
                           "permit://google.com/" + client->GetAccountId());
  permit_access->SetString("id", b64_public_key);
  permit_access->SetString("type", "access");
  permit_access->SetString("data", b64_private_key);

  PA_LOG(INFO) << "Returning permit access for "
               << "chrome.easyUnlockPrivate.getPermitAccess:\n"
               << "  id: " << b64_public_key;

  std::unique_ptr<easy_unlock_private::PermitRecord> result =
      easy_unlock_private::PermitRecord::FromValue(*permit_access);
  results_ = easy_unlock_private::GetPermitAccess::Results::Create(*result);
}

EasyUnlockPrivateClearPermitAccessFunction::
    EasyUnlockPrivateClearPermitAccessFunction() {
}

EasyUnlockPrivateClearPermitAccessFunction::
    ~EasyUnlockPrivateClearPermitAccessFunction() {
}

bool EasyUnlockPrivateClearPermitAccessFunction::RunSync() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  EasyUnlockService::Get(profile)->ClearPermitAccess();
  return true;
}

EasyUnlockPrivateSetRemoteDevicesFunction::
    EasyUnlockPrivateSetRemoteDevicesFunction() {
}

EasyUnlockPrivateSetRemoteDevicesFunction::
    ~EasyUnlockPrivateSetRemoteDevicesFunction() {
}

bool EasyUnlockPrivateSetRemoteDevicesFunction::RunSync() {
  std::unique_ptr<easy_unlock_private::SetRemoteDevices::Params> params(
      easy_unlock_private::SetRemoteDevices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::ListValue devices;
  for (const easy_unlock_private::Device& device : params->devices)
    devices.Append(device.ToValue());

  // Store the BLE device if we are trying out the BLE experiment.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          proximity_auth::switches::kEnableBluetoothLowEnergyDiscovery)) {
    EasyUnlockService::Get(profile)->SetRemoteBleDevices(devices);
  } else {
    EasyUnlockService::Get(profile)->SetRemoteDevices(devices);
  }

  return true;
}

EasyUnlockPrivateGetRemoteDevicesFunction::
    EasyUnlockPrivateGetRemoteDevicesFunction() {
}

EasyUnlockPrivateGetRemoteDevicesFunction::
    ~EasyUnlockPrivateGetRemoteDevicesFunction() {
}

bool EasyUnlockPrivateGetRemoteDevicesFunction::RunAsync() {
  // Return the remote devices stored with the native CryptAuthDeviceManager if
  // we are trying out the BLE experiment.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          proximity_auth::switches::kEnableBluetoothLowEnergyDiscovery)) {
    ReturnDevicesForExperiment();
  } else {
    Profile* profile = Profile::FromBrowserContext(browser_context());
    const base::ListValue* devices =
        EasyUnlockService::Get(profile)->GetRemoteDevices();
    SetResult(devices ? devices->CreateDeepCopy()
                      : base::MakeUnique<base::ListValue>());
    SendResponse(true);
  }

  return true;
}

std::string EasyUnlockPrivateGetRemoteDevicesFunction::GetUserPrivateKey() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  proximity_auth::ProximityAuthClient* client =
      EasyUnlockService::Get(profile)->proximity_auth_client();
  proximity_auth::CryptAuthEnrollmentManager* enrollment_manager =
      client->GetCryptAuthEnrollmentManager();
  return enrollment_manager->GetUserPrivateKey();
}

std::vector<cryptauth::ExternalDeviceInfo>
EasyUnlockPrivateGetRemoteDevicesFunction::GetUnlockKeys() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  proximity_auth::ProximityAuthClient* client =
      EasyUnlockService::Get(profile)->proximity_auth_client();
  proximity_auth::CryptAuthDeviceManager* device_manager =
      client->GetCryptAuthDeviceManager();
  return device_manager->unlock_keys();
}

void EasyUnlockPrivateGetRemoteDevicesFunction::ReturnDevicesForExperiment() {
  // Check that we are inside a user profile.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  EasyUnlockService* easy_unlock_service = EasyUnlockService::Get(profile);
  if (easy_unlock_service->GetType() != EasyUnlockService::TYPE_REGULAR) {
    SetError("This function must be called inside a user session.");
    SendResponse(true);
    return;
  }

  // Get the synced unlock key data.
  proximity_auth::ProximityAuthClient* client =
      easy_unlock_service->proximity_auth_client();

  permit_id_ = "permit://google.com/easyunlock/v1/" + client->GetAccountId();
  secure_message_delegate_ = client->CreateSecureMessageDelegate();
  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys = GetUnlockKeys();
  expected_devices_count_ = unlock_keys.size();

  remote_devices_.reset(new base::ListValue());
  if (expected_devices_count_ == 0) {
    SetResult(std::move(remote_devices_));
    SendResponse(true);
    return;
  }

  // If there is a BLE unlock key, then don't return anything, so the app does
  // not try the classic Bluetooth protocol.
  for (const auto& unlock_key : unlock_keys) {
    if (unlock_key.bluetooth_address().empty()) {
      SetResult(std::move(remote_devices_));
      SendResponse(true);
      return;
    }
  }

  // Derive the PSKs for the user's unlock keys.
  PA_LOG(INFO) << "Deriving PSKs for "
               << "chrome.easyUnlockPrivate.getRemoteDevices.\n"
               << "Expecting " << expected_devices_count_ << " devices.";
  for (const auto& unlock_key : unlock_keys) {
    secure_message_delegate_->DeriveKey(
        GetUserPrivateKey(), unlock_key.public_key(),
        base::Bind(
            &EasyUnlockPrivateGetRemoteDevicesFunction::OnPSKDerivedForDevice,
            this, unlock_key));
  }
}

void EasyUnlockPrivateGetRemoteDevicesFunction::OnPSKDerivedForDevice(
    const cryptauth::ExternalDeviceInfo& device,
    const std::string& persistent_symmetric_key) {
  std::string b64_public_key, b64_psk;
  base::Base64UrlEncode(device.public_key(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &b64_public_key);
  base::Base64UrlEncode(persistent_symmetric_key,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &b64_psk);

  // Fill in the JSON dictionary containing a single unlock key's data.
  std::unique_ptr<base::DictionaryValue> device_dictionary(
      new base::DictionaryValue());
  device_dictionary->SetString("name", device.friendly_device_name());
  device_dictionary->SetString("bluetoothAddress", device.bluetooth_address());
  device_dictionary->SetString("psk", b64_psk);

  // Fill in the permit license for the unlock key.
  std::unique_ptr<base::DictionaryValue> permit_license(
      new base::DictionaryValue());
  permit_license->SetString("permitId", permit_id_);
  permit_license->SetString("id", b64_public_key);
  permit_license->SetString("type", "license");
  permit_license->SetString("data", b64_public_key);
  device_dictionary->Set("permitRecord", std::move(permit_license));

  remote_devices_->Append(std::move(device_dictionary));

  // If all PSKs are derived, then return from the API call.
  PA_LOG(INFO) << "Derived PSK for " << b64_public_key << ": "
               << remote_devices_->GetSize() << "/" << expected_devices_count_;
  if (remote_devices_->GetSize() == expected_devices_count_) {
    SetResult(std::move(remote_devices_));
    SendResponse(true);
  }
}

EasyUnlockPrivateGetSignInChallengeFunction::
    EasyUnlockPrivateGetSignInChallengeFunction() {
}

EasyUnlockPrivateGetSignInChallengeFunction::
    ~EasyUnlockPrivateGetSignInChallengeFunction() {
}

bool EasyUnlockPrivateGetSignInChallengeFunction::RunAsync() {
  std::unique_ptr<easy_unlock_private::GetSignInChallenge::Params> params(
      easy_unlock_private::GetSignInChallenge::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

#if defined(OS_CHROMEOS)
  Profile* profile = Profile::FromBrowserContext(browser_context());
  const std::string challenge =
      EasyUnlockService::Get(profile)->GetChallenge();
  if (!challenge.empty() && !params->nonce.empty()) {
    EasyUnlockTpmKeyManager* key_manager =
        EasyUnlockTpmKeyManagerFactory::GetInstance()->Get(profile);
    if (!key_manager) {
      SetError("No EasyUnlockTpmKeyManager.");
      return false;
    }
    key_manager->SignUsingTpmKey(
        EasyUnlockService::Get(profile)->GetAccountId(),
        std::string(params->nonce.begin(), params->nonce.end()),
        base::Bind(&EasyUnlockPrivateGetSignInChallengeFunction::OnDone, this,
                   challenge));
  } else {
    OnDone(challenge, std::string());
  }
  return true;
#else  // if !defined(OS_CHROMEOS)
  SetError("Sign-in not supported.");
  return false;
#endif  // defined(OS_CHROMEOS)
}

void EasyUnlockPrivateGetSignInChallengeFunction::OnDone(
    const std::string& challenge,
    const std::string& signed_nonce) {
  results_ = easy_unlock_private::GetSignInChallenge::Results::Create(
      std::vector<char>(challenge.begin(), challenge.end()),
      std::vector<char>(signed_nonce.begin(), signed_nonce.end()));
  SendResponse(true);
}

EasyUnlockPrivateTrySignInSecretFunction::
    EasyUnlockPrivateTrySignInSecretFunction() {
}

EasyUnlockPrivateTrySignInSecretFunction::
    ~EasyUnlockPrivateTrySignInSecretFunction() {
}

bool EasyUnlockPrivateTrySignInSecretFunction::RunSync() {
  std::unique_ptr<easy_unlock_private::TrySignInSecret::Params> params(
      easy_unlock_private::TrySignInSecret::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  EasyUnlockService::Get(profile)->FinalizeSignin(std::string(
      params->sign_in_secret.begin(), params->sign_in_secret.end()));
  return true;
}

EasyUnlockPrivateGetUserInfoFunction::EasyUnlockPrivateGetUserInfoFunction() {
}

EasyUnlockPrivateGetUserInfoFunction::~EasyUnlockPrivateGetUserInfoFunction() {
}

bool EasyUnlockPrivateGetUserInfoFunction::RunSync() {
  EasyUnlockService* service =
      EasyUnlockService::Get(Profile::FromBrowserContext(browser_context()));
  std::vector<easy_unlock_private::UserInfo> users;
  const AccountId& account_id = service->GetAccountId();
  if (account_id.is_valid()) {
    easy_unlock_private::UserInfo user;
    user.user_id = account_id.GetUserEmail();
    user.logged_in = service->GetType() == EasyUnlockService::TYPE_REGULAR;
    user.data_ready = user.logged_in || service->GetRemoteDevices() != NULL;

    EasyUnlockService::UserSettings user_settings =
        EasyUnlockService::GetUserSettings(account_id);
    user.require_close_proximity = user_settings.require_close_proximity;

    user.device_user_id = proximity_auth::CalculateDeviceUserId(
        EasyUnlockService::GetDeviceId(), account_id.GetUserEmail());

    user.ble_discovery_enabled =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            proximity_auth::switches::kEnableBluetoothLowEnergyDiscovery);
    users.push_back(std::move(user));
  }
  results_ = easy_unlock_private::GetUserInfo::Results::Create(users);
  return true;
}

EasyUnlockPrivateGetConnectionInfoFunction::
    EasyUnlockPrivateGetConnectionInfoFunction() {
}

EasyUnlockPrivateGetConnectionInfoFunction::
    ~EasyUnlockPrivateGetConnectionInfoFunction() {
}

bool EasyUnlockPrivateGetConnectionInfoFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  std::unique_ptr<easy_unlock_private::GetConnectionInfo::Params> params =
      easy_unlock_private::GetConnectionInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  device::BluetoothDevice* device = adapter->GetDevice(params->device_address);

  std::string error;
  if (!device)
    error = "Invalid Bluetooth device.";
  else if (!device->IsConnected())
    error = "Bluetooth device not connected.";

  if (!error.empty()) {
    SetError(error);
    SendResponse(false);
    return true;
  }

  device->GetConnectionInfo(base::Bind(
      &EasyUnlockPrivateGetConnectionInfoFunction::OnConnectionInfo, this));
  return false;
}

void EasyUnlockPrivateGetConnectionInfoFunction::OnConnectionInfo(
    const device::BluetoothDevice::ConnectionInfo& connection_info) {
  std::unique_ptr<base::ListValue> results(new base::ListValue());
  results->AppendInteger(connection_info.rssi);
  results->AppendInteger(connection_info.transmit_power);
  results->AppendInteger(connection_info.max_transmit_power);
  SetResultList(std::move(results));
  SendResponse(true);
}

EasyUnlockPrivateShowErrorBubbleFunction::
    EasyUnlockPrivateShowErrorBubbleFunction() {
}

EasyUnlockPrivateShowErrorBubbleFunction::
    ~EasyUnlockPrivateShowErrorBubbleFunction() {
}

bool EasyUnlockPrivateShowErrorBubbleFunction::RunSync() {
  content::WebContents* web_contents = GetAssociatedWebContents();
  if (!web_contents) {
    SetError("A foreground app window is required.");
    return true;
  }

  std::unique_ptr<easy_unlock_private::ShowErrorBubble::Params> params(
      easy_unlock_private::ShowErrorBubble::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->link_range.start < 0 ||
      params->link_range.end < 0 ||
      base::saturated_cast<size_t>(params->link_range.end) >
          params->message.size()) {
    SetError("Invalid link range.");
    return true;
  }

#if defined(TOOLKIT_VIEWS)
  gfx::Rect anchor_rect(
      params->anchor_rect.left, params->anchor_rect.top,
      params->anchor_rect.width, params->anchor_rect.height);
  anchor_rect +=
      web_contents->GetContainerBounds().OffsetFromOrigin();
  ShowProximityAuthErrorBubble(
      base::UTF8ToUTF16(params->message),
      gfx::Range(params->link_range.start, params->link_range.end),
      GURL(params->link_target), anchor_rect, web_contents);
#else
  SetError("Not supported on non-Views platforms.");
#endif
  return true;
}

EasyUnlockPrivateHideErrorBubbleFunction::
    EasyUnlockPrivateHideErrorBubbleFunction() {
}

EasyUnlockPrivateHideErrorBubbleFunction::
    ~EasyUnlockPrivateHideErrorBubbleFunction() {
}

bool EasyUnlockPrivateHideErrorBubbleFunction::RunSync() {
#if defined(TOOLKIT_VIEWS)
  HideProximityAuthErrorBubble();
#else
  SetError("Not supported on non-Views platforms.");
#endif
  return true;
}

EasyUnlockPrivateSetAutoPairingResultFunction::
    EasyUnlockPrivateSetAutoPairingResultFunction() {
}

EasyUnlockPrivateSetAutoPairingResultFunction::
    ~EasyUnlockPrivateSetAutoPairingResultFunction() {
}

bool EasyUnlockPrivateSetAutoPairingResultFunction::RunSync() {
  std::unique_ptr<easy_unlock_private::SetAutoPairingResult::Params> params =
      easy_unlock_private::SetAutoPairingResult::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error_message;
  if (params->result.error_message)
    error_message = *params->result.error_message;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  EasyUnlockService::Get(profile)
      ->SetAutoPairingResult(params->result.success, error_message);

  return true;
}

EasyUnlockPrivateFindSetupConnectionFunction::
    EasyUnlockPrivateFindSetupConnectionFunction()
    : bluetooth_throttler_(new proximity_auth::BluetoothThrottlerImpl(
          base::WrapUnique(new base::DefaultTickClock()))) {}

EasyUnlockPrivateFindSetupConnectionFunction::
    ~EasyUnlockPrivateFindSetupConnectionFunction() {
  // |connection_finder_| has a raw pointer to |bluetooth_throttler_|, so it
  // should be destroyed first.
  connection_finder_.reset();
  bluetooth_throttler_.reset();
}

void EasyUnlockPrivateFindSetupConnectionFunction::
    OnConnectionFinderTimedOut() {
  SetError("No connection found.");
  connection_finder_.reset();
  SendResponse(false);
}

void EasyUnlockPrivateFindSetupConnectionFunction::OnConnectionFound(
    std::unique_ptr<proximity_auth::Connection> connection) {
  // Connection are not persistent by default.
  std::string device_address = connection->remote_device().bluetooth_address;
  bool persistent = false;
  int connection_id =
      GetConnectionManager(browser_context())
          ->AddConnection(extension(), std::move(connection), persistent);
  results_ = easy_unlock_private::FindSetupConnection::Results::Create(
      connection_id, device_address);
  SendResponse(true);
}

bool EasyUnlockPrivateFindSetupConnectionFunction::RunAsync() {
  std::unique_ptr<easy_unlock_private::FindSetupConnection::Params> params =
      easy_unlock_private::FindSetupConnection::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // Creates a BLE connection finder to look for any device advertising
  // |params->setup_service_uuid|.
  connection_finder_.reset(
      new proximity_auth::BluetoothLowEnergyConnectionFinder(
          proximity_auth::RemoteDevice(), params->setup_service_uuid,
          proximity_auth::BluetoothLowEnergyConnectionFinder::FIND_ANY_DEVICE,
          nullptr, bluetooth_throttler_.get(), 3));

  connection_finder_->Find(base::Bind(
      &EasyUnlockPrivateFindSetupConnectionFunction::OnConnectionFound, this));

  timer_.reset(new base::OneShotTimer());
  timer_->Start(FROM_HERE, base::TimeDelta::FromSeconds(params->time_out),
                base::Bind(&EasyUnlockPrivateFindSetupConnectionFunction::
                               OnConnectionFinderTimedOut,
                           this));

  return true;
}

EasyUnlockPrivateSetupConnectionStatusFunction::
    EasyUnlockPrivateSetupConnectionStatusFunction() {}

EasyUnlockPrivateSetupConnectionStatusFunction::
    ~EasyUnlockPrivateSetupConnectionStatusFunction() {}

bool EasyUnlockPrivateSetupConnectionStatusFunction::RunSync() {
  std::unique_ptr<easy_unlock_private::SetupConnectionStatus::Params> params =
      easy_unlock_private::SetupConnectionStatus::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  api::easy_unlock_private::ConnectionStatus status =
      GetConnectionManager(browser_context())
          ->ConnectionStatus(extension(), params->connection_id);
  results_ =
      easy_unlock_private::SetupConnectionStatus::Results::Create(status);
  if (status == api::easy_unlock_private::CONNECTION_STATUS_NONE)
    SetError("Invalid connectionId");
  return true;
}

EasyUnlockPrivateSetupConnectionDisconnectFunction::
    EasyUnlockPrivateSetupConnectionDisconnectFunction() {}

EasyUnlockPrivateSetupConnectionDisconnectFunction::
    ~EasyUnlockPrivateSetupConnectionDisconnectFunction() {}

bool EasyUnlockPrivateSetupConnectionDisconnectFunction::RunSync() {
  std::unique_ptr<easy_unlock_private::SetupConnectionDisconnect::Params>
      params = easy_unlock_private::SetupConnectionDisconnect::Params::Create(
          *args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  bool success = GetConnectionManager(browser_context())
                     ->Disconnect(extension(), params->connection_id);
  if (!success)
    SetError("Invalid connectionId.");
  return true;
}

EasyUnlockPrivateSetupConnectionSendFunction::
    EasyUnlockPrivateSetupConnectionSendFunction() {}

EasyUnlockPrivateSetupConnectionSendFunction::
    ~EasyUnlockPrivateSetupConnectionSendFunction() {}

bool EasyUnlockPrivateSetupConnectionSendFunction::RunSync() {
  std::unique_ptr<easy_unlock_private::SetupConnectionSend::Params> params =
      easy_unlock_private::SetupConnectionSend::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string payload(params->data.begin(), params->data.end());
  bool success = GetConnectionManager(browser_context())
                     ->SendMessage(extension(), params->connection_id, payload);
  if (!success)
    SetError("Invalid connectionId.");
  return true;
}

EasyUnlockPrivateSetupConnectionGetDeviceAddressFunction::
    EasyUnlockPrivateSetupConnectionGetDeviceAddressFunction() {}

EasyUnlockPrivateSetupConnectionGetDeviceAddressFunction::
    ~EasyUnlockPrivateSetupConnectionGetDeviceAddressFunction() {}

bool EasyUnlockPrivateSetupConnectionGetDeviceAddressFunction::RunSync() {
  std::unique_ptr<easy_unlock_private::SetupConnectionGetDeviceAddress::Params>
      params =
          easy_unlock_private::SetupConnectionGetDeviceAddress::Params::Create(
              *args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string device_address =
      GetConnectionManager(browser_context())
          ->GetDeviceAddress(extension(), params->connection_id);
  results_ =
      easy_unlock_private::SetupConnectionGetDeviceAddress::Results::Create(
          device_address);
  if (device_address.empty())
    SetError("Invalid connectionId.");
  return true;
}

}  // namespace extensions
