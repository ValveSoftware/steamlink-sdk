// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_api.h"

#include <stdint.h>
#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/utils.h"
#include "chrome/common/extensions/api/bluetooth_low_energy.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "extensions/common/api/bluetooth/bluetooth_manifest_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#endif

using content::BrowserContext;
using content::BrowserThread;

namespace apibtle = extensions::api::bluetooth_low_energy;

namespace extensions {

namespace {

const char kErrorAdapterNotInitialized[] =
    "Could not initialize Bluetooth adapter";
const char kErrorAlreadyConnected[] = "Already connected";
const char kErrorAlreadyNotifying[] = "Already notifying";
const char kErrorAttributeLengthInvalid[] = "Attribute length invalid";
const char kErrorAuthenticationFailed[] = "Authentication failed";
const char kErrorCanceled[] = "Request canceled";
const char kErrorConnectionCongested[] = "Connection congested";
const char kErrorGattNotSupported[] = "Operation not supported by this service";
const char kErrorHigherSecurity[] = "Higher security needed";
const char kErrorInProgress[] = "In progress";
const char kErrorInsufficientAuthorization[] = "Insufficient authorization";
const char kErrorInsufficientEncryption[] = "Insufficient encryption";
const char kErrorInvalidAdvertisementLength[] = "Invalid advertisement length";
const char kErrorInvalidLength[] = "Invalid attribute value length";
const char kErrorNotConnected[] = "Not connected";
const char kErrorNotFound[] = "Instance not found";
const char kErrorNotNotifying[] = "Not notifying";
const char kErrorOffsetInvalid[] = "Offset invalid";
const char kErrorOperationFailed[] = "Operation failed";
const char kErrorPermissionDenied[] = "Permission denied";
const char kErrorPlatformNotSupported[] =
    "This operation is not supported on the current platform";
const char kErrorRequestNotSupported[] = "Request not supported";
const char kErrorTimeout[] = "Operation timed out";
const char kErrorUnsupportedDevice[] =
    "This device is not supported on the current platform";
const char kErrorInvalidServiceId[] = "The service ID doesn't exist.";
const char kErrorInvalidCharacteristicId[] =
    "The characteristic ID doesn't exist.";
const char kErrorNotifyPropertyNotSet[] =
    "The characteristic does not have the notify property set.";
const char kErrorIndicatePropertyNotSet[] =
    "The characteristic does not have the indicate property set.";
const char kErrorServiceNotRegistered[] =
    "The characteristic is not owned by a service that is registered.";
const char kErrorUnknownNotificationError[] =
    "An unknown notification error occured.";

const char kStatusAdvertisementAlreadyExists[] =
    "An advertisement is already advertising";
const char kStatusAdvertisementDoesNotExist[] =
    "This advertisement does not exist";

// Returns the correct error string based on error status |status|. This is used
// to set the value of |chrome.runtime.lastError.message| and should not be
// passed |BluetoothLowEnergyEventRouter::kStatusSuccess|.
std::string StatusToString(BluetoothLowEnergyEventRouter::Status status) {
  switch (status) {
    case BluetoothLowEnergyEventRouter::kStatusErrorAlreadyConnected:
      return kErrorAlreadyConnected;
    case BluetoothLowEnergyEventRouter::kStatusErrorAlreadyNotifying:
      return kErrorAlreadyNotifying;
    case BluetoothLowEnergyEventRouter::kStatusErrorAttributeLengthInvalid:
      return kErrorAttributeLengthInvalid;
    case BluetoothLowEnergyEventRouter::kStatusErrorAuthenticationFailed:
      return kErrorAuthenticationFailed;
    case BluetoothLowEnergyEventRouter::kStatusErrorCanceled:
      return kErrorCanceled;
    case BluetoothLowEnergyEventRouter::kStatusErrorConnectionCongested:
      return kErrorConnectionCongested;
    case BluetoothLowEnergyEventRouter::kStatusErrorGattNotSupported:
      return kErrorGattNotSupported;
    case BluetoothLowEnergyEventRouter::kStatusErrorHigherSecurity:
      return kErrorHigherSecurity;
    case BluetoothLowEnergyEventRouter::kStatusErrorInProgress:
      return kErrorInProgress;
    case BluetoothLowEnergyEventRouter::kStatusErrorInsufficientAuthorization:
      return kErrorInsufficientAuthorization;
    case BluetoothLowEnergyEventRouter::kStatusErrorInsufficientEncryption:
      return kErrorInsufficientEncryption;
    case BluetoothLowEnergyEventRouter::kStatusErrorInvalidLength:
      return kErrorInvalidLength;
    case BluetoothLowEnergyEventRouter::kStatusErrorNotConnected:
      return kErrorNotConnected;
    case BluetoothLowEnergyEventRouter::kStatusErrorNotFound:
      return kErrorNotFound;
    case BluetoothLowEnergyEventRouter::kStatusErrorNotNotifying:
      return kErrorNotNotifying;
    case BluetoothLowEnergyEventRouter::kStatusErrorOffsetInvalid:
      return kErrorOffsetInvalid;
    case BluetoothLowEnergyEventRouter::kStatusErrorPermissionDenied:
      return kErrorPermissionDenied;
    case BluetoothLowEnergyEventRouter::kStatusErrorRequestNotSupported:
      return kErrorRequestNotSupported;
    case BluetoothLowEnergyEventRouter::kStatusErrorTimeout:
      return kErrorTimeout;
    case BluetoothLowEnergyEventRouter::kStatusErrorUnsupportedDevice:
      return kErrorUnsupportedDevice;
    case BluetoothLowEnergyEventRouter::kStatusErrorInvalidServiceId:
      return kErrorInvalidServiceId;
    case BluetoothLowEnergyEventRouter::kStatusSuccess:
      NOTREACHED();
      break;
    default:
      return kErrorOperationFailed;
  }
  return "";
}

extensions::BluetoothLowEnergyEventRouter* GetEventRouter(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return extensions::BluetoothLowEnergyAPI::Get(context)->event_router();
}

template <typename T>
void DoWorkCallback(const base::Callback<T()>& callback) {
  DCHECK(!callback.is_null());
  callback.Run();
}

std::unique_ptr<device::BluetoothAdvertisement::ManufacturerData>
CreateManufacturerData(
    std::vector<apibtle::ManufacturerData>* manufacturer_data) {
  std::unique_ptr<device::BluetoothAdvertisement::ManufacturerData>
      created_data(new device::BluetoothAdvertisement::ManufacturerData());
  for (const auto& it : *manufacturer_data) {
    std::vector<uint8_t> data(it.data.size());
    std::copy(it.data.begin(), it.data.end(), data.begin());
    (*created_data)[it.id] = data;
  }
  return created_data;
}

std::unique_ptr<device::BluetoothAdvertisement::ServiceData> CreateServiceData(
    std::vector<apibtle::ServiceData>* service_data) {
  std::unique_ptr<device::BluetoothAdvertisement::ServiceData> created_data(
      new device::BluetoothAdvertisement::ServiceData());
  for (const auto& it : *service_data) {
    std::vector<uint8_t> data(it.data.size());
    std::copy(it.data.begin(), it.data.end(), data.begin());
    (*created_data)[it.uuid] = data;
  }
  return created_data;
}

bool HasProperty(
    const std::vector<apibtle::CharacteristicProperty>& api_properties,
    apibtle::CharacteristicProperty property) {
  return find(api_properties.begin(), api_properties.end(), property) !=
         api_properties.end();
}

bool HasPermission(
    const std::vector<apibtle::DescriptorPermission>& api_permissions,
    apibtle::DescriptorPermission permission) {
  return find(api_permissions.begin(), api_permissions.end(), permission) !=
         api_permissions.end();
}

device::BluetoothGattCharacteristic::Properties GetBluetoothProperties(
    const std::vector<apibtle::CharacteristicProperty>& api_properties) {
  device::BluetoothGattCharacteristic::Properties properties =
      device::BluetoothGattCharacteristic::PROPERTY_NONE;

  static_assert(
      apibtle::CHARACTERISTIC_PROPERTY_LAST == 14,
      "Update required if the number of characteristic properties changes.");

  if (HasProperty(api_properties, apibtle::CHARACTERISTIC_PROPERTY_BROADCAST)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_BROADCAST;
  }

  if (HasProperty(api_properties, apibtle::CHARACTERISTIC_PROPERTY_READ)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_READ;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_WRITEWITHOUTRESPONSE)) {
    properties |=
        device::BluetoothGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE;
  }

  if (HasProperty(api_properties, apibtle::CHARACTERISTIC_PROPERTY_WRITE)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_WRITE;
  }

  if (HasProperty(api_properties, apibtle::CHARACTERISTIC_PROPERTY_NOTIFY)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_NOTIFY;
  }

  if (HasProperty(api_properties, apibtle::CHARACTERISTIC_PROPERTY_INDICATE)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_INDICATE;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_AUTHENTICATEDSIGNEDWRITES)) {
    properties |= device::BluetoothGattCharacteristic::
        PROPERTY_AUTHENTICATED_SIGNED_WRITES;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_EXTENDEDPROPERTIES)) {
    properties |=
        device::BluetoothGattCharacteristic::PROPERTY_EXTENDED_PROPERTIES;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_RELIABLEWRITE)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_RELIABLE_WRITE;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_WRITABLEAUXILIARIES)) {
    properties |=
        device::BluetoothGattCharacteristic::PROPERTY_WRITABLE_AUXILIARIES;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_ENCRYPTREAD)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_READ_ENCRYPTED;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_ENCRYPTWRITE)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_WRITE_ENCRYPTED;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_ENCRYPTAUTHENTICATEDREAD)) {
    properties |= device::BluetoothGattCharacteristic::
        PROPERTY_READ_ENCRYPTED_AUTHENTICATED;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_ENCRYPTAUTHENTICATEDWRITE)) {
    properties |= device::BluetoothGattCharacteristic::
        PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED;
  }

  return properties;
}

device::BluetoothGattCharacteristic::Permissions GetBluetoothPermissions(
    const std::vector<apibtle::DescriptorPermission>& api_permissions) {
  device::BluetoothGattCharacteristic::Permissions permissions =
      device::BluetoothGattCharacteristic::PERMISSION_NONE;

  static_assert(
      apibtle::DESCRIPTOR_PERMISSION_LAST == 6,
      "Update required if the number of descriptor permissions changes.");

  if (HasPermission(api_permissions, apibtle::DESCRIPTOR_PERMISSION_READ)) {
    permissions |= device::BluetoothGattCharacteristic::PERMISSION_READ;
  }

  if (HasPermission(api_permissions, apibtle::DESCRIPTOR_PERMISSION_WRITE)) {
    permissions |= device::BluetoothGattCharacteristic::PERMISSION_WRITE;
  }

  if (HasPermission(api_permissions,
                    apibtle::DESCRIPTOR_PERMISSION_ENCRYPTEDREAD)) {
    permissions |=
        device::BluetoothGattCharacteristic::PERMISSION_READ_ENCRYPTED;
  }

  if (HasPermission(api_permissions,
                    apibtle::DESCRIPTOR_PERMISSION_ENCRYPTEDWRITE)) {
    permissions |=
        device::BluetoothGattCharacteristic::PERMISSION_WRITE_ENCRYPTED;
  }

  if (HasPermission(
          api_permissions,
          apibtle::DESCRIPTOR_PERMISSION_ENCRYPTEDAUTHENTICATEDREAD)) {
    permissions |= device::BluetoothGattCharacteristic::
        PERMISSION_READ_ENCRYPTED_AUTHENTICATED;
  }

  if (HasPermission(
          api_permissions,
          apibtle::DESCRIPTOR_PERMISSION_ENCRYPTEDAUTHENTICATEDWRITE)) {
    permissions |= device::BluetoothGattCharacteristic::
        PERMISSION_WRITE_ENCRYPTED_AUTHENTICATED;
  }

  return permissions;
}

bool IsAutoLaunchedKioskApp(const ExtensionId& id) {
#if defined(OS_CHROMEOS)
  chromeos::KioskAppManager::App app_info;
  return chromeos::KioskAppManager::Get()->GetApp(id, &app_info) &&
         app_info.was_auto_launched_with_zero_delay;
#else
  return false;
#endif
}

bool IsPeripheralFlagEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBLEAdvertising);
}

}  // namespace


static base::LazyInstance<BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI>*
BluetoothLowEnergyAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
BluetoothLowEnergyAPI* BluetoothLowEnergyAPI::Get(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetFactoryInstance()->Get(context);
}

BluetoothLowEnergyAPI::BluetoothLowEnergyAPI(BrowserContext* context)
    : event_router_(new BluetoothLowEnergyEventRouter(context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BluetoothLowEnergyAPI::~BluetoothLowEnergyAPI() {
}

void BluetoothLowEnergyAPI::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

namespace api {

BluetoothLowEnergyExtensionFunctionDeprecated::
    BluetoothLowEnergyExtensionFunctionDeprecated() {}

BluetoothLowEnergyExtensionFunctionDeprecated::
    ~BluetoothLowEnergyExtensionFunctionDeprecated() {}

bool BluetoothLowEnergyExtensionFunctionDeprecated::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!BluetoothManifestData::CheckLowEnergyPermitted(extension())) {
    error_ = kErrorPermissionDenied;
    return false;
  }

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());
  if (!event_router->IsBluetoothSupported()) {
    SetError(kErrorPlatformNotSupported);
    return false;
  }

  // It is safe to pass |this| here as ExtensionFunction is refcounted.
  if (!event_router->InitializeAdapterAndInvokeCallback(base::Bind(
          &DoWorkCallback<bool>,
          base::Bind(&BluetoothLowEnergyExtensionFunctionDeprecated::DoWork,
                     this)))) {
    SetError(kErrorAdapterNotInitialized);
    return false;
  }

  return true;
}

BluetoothLowEnergyExtensionFunction::BluetoothLowEnergyExtensionFunction()
    : event_router_(nullptr) {}

BluetoothLowEnergyExtensionFunction::~BluetoothLowEnergyExtensionFunction() {}

ExtensionFunction::ResponseAction BluetoothLowEnergyExtensionFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!BluetoothManifestData::CheckLowEnergyPermitted(extension()))
    return RespondNow(Error(kErrorPermissionDenied));

  event_router_ = GetEventRouter(browser_context());
  if (!event_router_->IsBluetoothSupported())
    return RespondNow(Error(kErrorPlatformNotSupported));

  // It is safe to pass |this| here as ExtensionFunction is refcounted.
  if (!event_router_->InitializeAdapterAndInvokeCallback(base::Bind(
          &DoWorkCallback<void>,
          base::Bind(&BluetoothLowEnergyExtensionFunction::PreDoWork, this)))) {
    // DoWork will respond when the adapter gets initialized.
    return RespondNow(Error(kErrorAdapterNotInitialized));
  }

  return RespondLater();
}

void BluetoothLowEnergyExtensionFunction::PreDoWork() {
  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router_->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }
  DoWork();
}

template <typename Params>
BLEPeripheralExtensionFunction<Params>::BLEPeripheralExtensionFunction() {}

template <typename Params>
BLEPeripheralExtensionFunction<Params>::~BLEPeripheralExtensionFunction() {}

template <typename Params>
ExtensionFunction::ResponseAction
BLEPeripheralExtensionFunction<Params>::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check permissions in manifest.
  if (!BluetoothManifestData::CheckPeripheralPermitted(extension()))
    return RespondNow(Error(kErrorPermissionDenied));

  if (!(IsAutoLaunchedKioskApp(extension()->id()) ||
        IsPeripheralFlagEnabled())) {
    return RespondNow(Error(kErrorPermissionDenied));
  }

// Causes link error on Windows. API will never be on Windows, so #ifdefing.
#if !defined(OS_WIN)
  params_ = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get() != NULL);
#endif

  return BluetoothLowEnergyExtensionFunction::Run();
}

bool BluetoothLowEnergyConnectFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::Connect::Params> params(
      apibtle::Connect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  bool persistent = false;  // Not persistent by default.
  apibtle::ConnectProperties* properties = params.get()->properties.get();
  if (properties)
    persistent = properties->persistent;

  event_router->Connect(
      persistent,
      extension(),
      params->device_address,
      base::Bind(&BluetoothLowEnergyConnectFunction::SuccessCallback, this),
      base::Bind(&BluetoothLowEnergyConnectFunction::ErrorCallback, this));

  return true;
}

void BluetoothLowEnergyConnectFunction::SuccessCallback() {
  SendResponse(true);
}

void BluetoothLowEnergyConnectFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyDisconnectFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::Disconnect::Params> params(
      apibtle::Disconnect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  event_router->Disconnect(
      extension(),
      params->device_address,
      base::Bind(&BluetoothLowEnergyDisconnectFunction::SuccessCallback, this),
      base::Bind(&BluetoothLowEnergyDisconnectFunction::ErrorCallback, this));

  return true;
}

void BluetoothLowEnergyDisconnectFunction::SuccessCallback() {
  SendResponse(true);
}

void BluetoothLowEnergyDisconnectFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyGetServiceFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::GetService::Params> params(
      apibtle::GetService::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  apibtle::Service service;
  BluetoothLowEnergyEventRouter::Status status =
      event_router->GetService(params->service_id, &service);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
    SendResponse(false);
    return false;
  }

  results_ = apibtle::GetService::Results::Create(service);
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetServicesFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::GetServices::Params> params(
      apibtle::GetServices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  BluetoothLowEnergyEventRouter::ServiceList service_list;
  if (!event_router->GetServices(params->device_address, &service_list)) {
    SetError(kErrorNotFound);
    SendResponse(false);
    return false;
  }

  results_ = apibtle::GetServices::Results::Create(service_list);
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetCharacteristicFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::GetCharacteristic::Params> params(
      apibtle::GetCharacteristic::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  apibtle::Characteristic characteristic;
  BluetoothLowEnergyEventRouter::Status status =
      event_router->GetCharacteristic(
          extension(), params->characteristic_id, &characteristic);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
    SendResponse(false);
    return false;
  }

  // Manually construct the result instead of using
  // apibtle::GetCharacteristic::Result::Create as it doesn't convert lists of
  // enums correctly.
  SetResult(apibtle::CharacteristicToValue(&characteristic));
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetCharacteristicsFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::GetCharacteristics::Params> params(
      apibtle::GetCharacteristics::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  BluetoothLowEnergyEventRouter::CharacteristicList characteristic_list;
  BluetoothLowEnergyEventRouter::Status status =
      event_router->GetCharacteristics(
          extension(), params->service_id, &characteristic_list);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
    SendResponse(false);
    return false;
  }

  // Manually construct the result instead of using
  // apibtle::GetCharacteristics::Result::Create as it doesn't convert lists of
  // enums correctly.
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  for (apibtle::Characteristic& characteristic : characteristic_list)
    result->Append(apibtle::CharacteristicToValue(&characteristic));

  SetResult(std::move(result));
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetIncludedServicesFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::GetIncludedServices::Params> params(
      apibtle::GetIncludedServices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  BluetoothLowEnergyEventRouter::ServiceList service_list;
  BluetoothLowEnergyEventRouter::Status status =
      event_router->GetIncludedServices(params->service_id, &service_list);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
    SendResponse(false);
    return false;
  }

  results_ = apibtle::GetIncludedServices::Results::Create(service_list);
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetDescriptorFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::GetDescriptor::Params> params(
      apibtle::GetDescriptor::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  apibtle::Descriptor descriptor;
  BluetoothLowEnergyEventRouter::Status status = event_router->GetDescriptor(
      extension(), params->descriptor_id, &descriptor);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
    SendResponse(false);
    return false;
  }

  // Manually construct the result instead of using
  // apibtle::GetDescriptor::Result::Create as it doesn't convert lists of enums
  // correctly.
  SetResult(apibtle::DescriptorToValue(&descriptor));
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyGetDescriptorsFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::GetDescriptors::Params> params(
      apibtle::GetDescriptors::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  BluetoothLowEnergyEventRouter::DescriptorList descriptor_list;
  BluetoothLowEnergyEventRouter::Status status = event_router->GetDescriptors(
      extension(), params->characteristic_id, &descriptor_list);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
    SendResponse(false);
    return false;
  }

  // Manually construct the result instead of using
  // apibtle::GetDescriptors::Result::Create as it doesn't convert lists of
  // enums correctly.
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  for (apibtle::Descriptor& descriptor : descriptor_list)
    result->Append(apibtle::DescriptorToValue(&descriptor));

  SetResult(std::move(result));
  SendResponse(true);

  return true;
}

bool BluetoothLowEnergyReadCharacteristicValueFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::ReadCharacteristicValue::Params> params(
      apibtle::ReadCharacteristicValue::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  instance_id_ = params->characteristic_id;
  event_router->ReadCharacteristicValue(
      extension(),
      instance_id_,
      base::Bind(
          &BluetoothLowEnergyReadCharacteristicValueFunction::SuccessCallback,
          this),
      base::Bind(
          &BluetoothLowEnergyReadCharacteristicValueFunction::ErrorCallback,
          this));

  return true;
}

void BluetoothLowEnergyReadCharacteristicValueFunction::SuccessCallback() {
  // Obtain info on the characteristic and see whether or not the characteristic
  // is still around.
  apibtle::Characteristic characteristic;
  BluetoothLowEnergyEventRouter::Status status =
      GetEventRouter(browser_context())
          ->GetCharacteristic(extension(), instance_id_, &characteristic);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
    SendResponse(false);
    return;
  }

  // Manually construct the result instead of using
  // apibtle::GetCharacteristic::Result::Create as it doesn't convert lists of
  // enums correctly.
  SetResult(apibtle::CharacteristicToValue(&characteristic));
  SendResponse(true);
}

void BluetoothLowEnergyReadCharacteristicValueFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyWriteCharacteristicValueFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::WriteCharacteristicValue::Params> params(
      apibtle::WriteCharacteristicValue::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  std::vector<uint8_t> value(params->value.begin(), params->value.end());
  event_router->WriteCharacteristicValue(
      extension(),
      params->characteristic_id,
      value,
      base::Bind(
          &BluetoothLowEnergyWriteCharacteristicValueFunction::SuccessCallback,
          this),
      base::Bind(
          &BluetoothLowEnergyWriteCharacteristicValueFunction::ErrorCallback,
          this));

  return true;
}

void BluetoothLowEnergyWriteCharacteristicValueFunction::SuccessCallback() {
  results_ = apibtle::WriteCharacteristicValue::Results::Create();
  SendResponse(true);
}

void BluetoothLowEnergyWriteCharacteristicValueFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyStartCharacteristicNotificationsFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::StartCharacteristicNotifications::Params> params(
      apibtle::StartCharacteristicNotifications::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  bool persistent = false;  // Not persistent by default.
  apibtle::NotificationProperties* properties = params.get()->properties.get();
  if (properties)
    persistent = properties->persistent;

  event_router->StartCharacteristicNotifications(
      persistent,
      extension(),
      params->characteristic_id,
      base::Bind(&BluetoothLowEnergyStartCharacteristicNotificationsFunction::
                     SuccessCallback,
                 this),
      base::Bind(&BluetoothLowEnergyStartCharacteristicNotificationsFunction::
                     ErrorCallback,
                 this));

  return true;
}

void
BluetoothLowEnergyStartCharacteristicNotificationsFunction::SuccessCallback() {
  SendResponse(true);
}

void BluetoothLowEnergyStartCharacteristicNotificationsFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyStopCharacteristicNotificationsFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::StopCharacteristicNotifications::Params> params(
      apibtle::StopCharacteristicNotifications::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  event_router->StopCharacteristicNotifications(
      extension(),
      params->characteristic_id,
      base::Bind(&BluetoothLowEnergyStopCharacteristicNotificationsFunction::
                     SuccessCallback,
                 this),
      base::Bind(&BluetoothLowEnergyStopCharacteristicNotificationsFunction::
                     ErrorCallback,
                 this));

  return true;
}

void
BluetoothLowEnergyStopCharacteristicNotificationsFunction::SuccessCallback() {
  SendResponse(true);
}

void BluetoothLowEnergyStopCharacteristicNotificationsFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyReadDescriptorValueFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::ReadDescriptorValue::Params> params(
      apibtle::ReadDescriptorValue::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  instance_id_ = params->descriptor_id;
  event_router->ReadDescriptorValue(
      extension(),
      instance_id_,
      base::Bind(
          &BluetoothLowEnergyReadDescriptorValueFunction::SuccessCallback,
          this),
      base::Bind(&BluetoothLowEnergyReadDescriptorValueFunction::ErrorCallback,
                 this));

  return true;
}

void BluetoothLowEnergyReadDescriptorValueFunction::SuccessCallback() {
  // Obtain info on the descriptor and see whether or not the descriptor is
  // still around.
  apibtle::Descriptor descriptor;
  BluetoothLowEnergyEventRouter::Status status =
      GetEventRouter(browser_context())
          ->GetDescriptor(extension(), instance_id_, &descriptor);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    SetError(StatusToString(status));
    SendResponse(false);
    return;
  }

  // Manually construct the result instead of using
  // apibtle::GetDescriptor::Results::Create as it doesn't convert lists of
  // enums correctly.
  SetResult(apibtle::DescriptorToValue(&descriptor));
  SendResponse(true);
}

void BluetoothLowEnergyReadDescriptorValueFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

bool BluetoothLowEnergyWriteDescriptorValueFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::WriteDescriptorValue::Params> params(
      apibtle::WriteDescriptorValue::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  std::vector<uint8_t> value(params->value.begin(), params->value.end());
  event_router->WriteDescriptorValue(
      extension(),
      params->descriptor_id,
      value,
      base::Bind(
          &BluetoothLowEnergyWriteDescriptorValueFunction::SuccessCallback,
          this),
      base::Bind(&BluetoothLowEnergyWriteDescriptorValueFunction::ErrorCallback,
                 this));

  return true;
}

void BluetoothLowEnergyWriteDescriptorValueFunction::SuccessCallback() {
  results_ = apibtle::WriteDescriptorValue::Results::Create();
  SendResponse(true);
}

void BluetoothLowEnergyWriteDescriptorValueFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  SetError(StatusToString(status));
  SendResponse(false);
}

BluetoothLowEnergyAdvertisementFunction::
    BluetoothLowEnergyAdvertisementFunction()
    : advertisements_manager_(nullptr) {
}

BluetoothLowEnergyAdvertisementFunction::
    ~BluetoothLowEnergyAdvertisementFunction() {
}

int BluetoothLowEnergyAdvertisementFunction::AddAdvertisement(
    BluetoothApiAdvertisement* advertisement) {
  DCHECK(advertisements_manager_);
  return advertisements_manager_->Add(advertisement);
}

BluetoothApiAdvertisement*
BluetoothLowEnergyAdvertisementFunction::GetAdvertisement(
    int advertisement_id) {
  DCHECK(advertisements_manager_);
  return advertisements_manager_->Get(extension_id(), advertisement_id);
}

void BluetoothLowEnergyAdvertisementFunction::RemoveAdvertisement(
    int advertisement_id) {
  DCHECK(advertisements_manager_);
  advertisements_manager_->Remove(extension_id(), advertisement_id);
}

bool BluetoothLowEnergyAdvertisementFunction::RunAsync() {
  Initialize();
  return BluetoothLowEnergyExtensionFunctionDeprecated::RunAsync();
}

void BluetoothLowEnergyAdvertisementFunction::Initialize() {
  advertisements_manager_ =
      ApiResourceManager<BluetoothApiAdvertisement>::Get(browser_context());
}

// RegisterAdvertisement:

bool BluetoothLowEnergyRegisterAdvertisementFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check permissions in manifest.
  if (!BluetoothManifestData::CheckPeripheralPermitted(extension())) {
    error_ = kErrorPermissionDenied;
    SendResponse(false);
    return false;
  }

  // For this API to be available the app has to be either auto
  // launched in Kiosk Mode or the enable-ble-advertisement-in-apps
  // should be set.
  if (!(IsAutoLaunchedKioskApp(extension()->id()) ||
        IsPeripheralFlagEnabled())) {
    error_ = kErrorPermissionDenied;
    SendResponse(false);
    return false;
  }

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    SetError(kErrorAdapterNotInitialized);
    SendResponse(false);
    return false;
  }

  std::unique_ptr<apibtle::RegisterAdvertisement::Params> params(
      apibtle::RegisterAdvertisement::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data(
      new device::BluetoothAdvertisement::Data(
          params->advertisement.type ==
                  apibtle::AdvertisementType::ADVERTISEMENT_TYPE_BROADCAST
              ? device::BluetoothAdvertisement::AdvertisementType::
                    ADVERTISEMENT_TYPE_BROADCAST
              : device::BluetoothAdvertisement::AdvertisementType::
                    ADVERTISEMENT_TYPE_PERIPHERAL));

  advertisement_data->set_service_uuids(
      std::move(params->advertisement.service_uuids));
  advertisement_data->set_solicit_uuids(
      std::move(params->advertisement.solicit_uuids));
  if (params->advertisement.manufacturer_data) {
    advertisement_data->set_manufacturer_data(
        CreateManufacturerData(params->advertisement.manufacturer_data.get()));
  }
  if (params->advertisement.service_data) {
    advertisement_data->set_service_data(
        CreateServiceData(params->advertisement.service_data.get()));
  }

  event_router->adapter()->RegisterAdvertisement(
      std::move(advertisement_data),
      base::Bind(
          &BluetoothLowEnergyRegisterAdvertisementFunction::SuccessCallback,
          this),
      base::Bind(
          &BluetoothLowEnergyRegisterAdvertisementFunction::ErrorCallback,
          this));

  return true;
}

void BluetoothLowEnergyRegisterAdvertisementFunction::SuccessCallback(
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  results_ = apibtle::RegisterAdvertisement::Results::Create(AddAdvertisement(
      new BluetoothApiAdvertisement(extension_id(), advertisement)));
  SendResponse(true);
}

void BluetoothLowEnergyRegisterAdvertisementFunction::ErrorCallback(
    device::BluetoothAdvertisement::ErrorCode status) {
  switch (status) {
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_ALREADY_EXISTS:
      SetError(kStatusAdvertisementAlreadyExists);
      break;
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_INVALID_LENGTH:
      SetError(kErrorInvalidAdvertisementLength);
      break;
    default:
      SetError(kErrorOperationFailed);
  }
  SendResponse(false);
}

// UnregisterAdvertisement:

bool BluetoothLowEnergyUnregisterAdvertisementFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check permission in the manifest.
  if (!BluetoothManifestData::CheckPeripheralPermitted(extension())) {
    error_ = kErrorPermissionDenied;
    SendResponse(false);
    return false;
  }

  // For this API to be available the app has to be either auto
  // launched in Kiosk Mode or the enable-ble-advertisement-in-apps
  // should be set.
  if (!(IsAutoLaunchedKioskApp(extension()->id()) ||
        IsPeripheralFlagEnabled())) {
    error_ = kErrorPermissionDenied;
    SendResponse(false);
    return false;
  }

  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // If we don't have an initialized adapter, unregistering is a no-op.
  if (!event_router->HasAdapter())
    return true;

  std::unique_ptr<apibtle::UnregisterAdvertisement::Params> params(
      apibtle::UnregisterAdvertisement::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  BluetoothApiAdvertisement* advertisement =
      GetAdvertisement(params->advertisement_id);
  if (!advertisement) {
    error_ = kStatusAdvertisementDoesNotExist;
    SendResponse(false);
    return false;
  }

  advertisement->advertisement()->Unregister(
      base::Bind(
          &BluetoothLowEnergyUnregisterAdvertisementFunction::SuccessCallback,
          this, params->advertisement_id),
      base::Bind(
          &BluetoothLowEnergyUnregisterAdvertisementFunction::ErrorCallback,
          this, params->advertisement_id));

  return true;
}

void BluetoothLowEnergyUnregisterAdvertisementFunction::SuccessCallback(
    int advertisement_id) {
  RemoveAdvertisement(advertisement_id);
  SendResponse(true);
}

void BluetoothLowEnergyUnregisterAdvertisementFunction::ErrorCallback(
    int advertisement_id,
    device::BluetoothAdvertisement::ErrorCode status) {
  RemoveAdvertisement(advertisement_id);
  switch (status) {
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_DOES_NOT_EXIST:
      SetError(kStatusAdvertisementDoesNotExist);
      break;
    default:
      SetError(kErrorOperationFailed);
  }
  SendResponse(false);
}

// createService:

template class BLEPeripheralExtensionFunction<apibtle::CreateService::Params>;

void BluetoothLowEnergyCreateServiceFunction::DoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
// Causes link error on Windows. API will never be on Windows, so #ifdefing.
// TODO: Ideally this should be handled by our feature system, so that this
// code doesn't even compile on OSes it isn't being used on, but currently this
// is not possible.
#if !defined(OS_WIN)
  base::WeakPtr<device::BluetoothLocalGattService> service =
      device::BluetoothLocalGattService::Create(
          event_router_->adapter(),
          device::BluetoothUUID(params_->service.uuid),
          params_->service.is_primary, nullptr, event_router_);

  event_router_->AddServiceToApp(extension_id(), service->GetIdentifier());
  Respond(ArgumentList(
      apibtle::CreateService::Results::Create(service->GetIdentifier())));
#else
  Respond(Error(kErrorPlatformNotSupported));
#endif
}

// createCharacteristic:

template class BLEPeripheralExtensionFunction<
    apibtle::CreateCharacteristic::Params>;

void BluetoothLowEnergyCreateCharacteristicFunction::DoWork() {
  device::BluetoothLocalGattService* service =
      event_router_->adapter()->GetGattService(params_->service_id);
  if (!service) {
    Respond(Error(kErrorInvalidServiceId));
    return;
  }

  base::WeakPtr<device::BluetoothLocalGattCharacteristic> characteristic =
      device::BluetoothLocalGattCharacteristic::Create(
          device::BluetoothUUID(params_->characteristic.uuid),
          GetBluetoothProperties(params_->characteristic.properties),
          device::BluetoothGattCharacteristic::Permissions(), service);

  // Keep a track of this characteristic so we can look it up later if a
  // descriptor lists it as its parent.
  event_router_->AddLocalCharacteristic(characteristic->GetIdentifier(),
                                        service->GetIdentifier());

  Respond(ArgumentList(apibtle::CreateCharacteristic::Results::Create(
      characteristic->GetIdentifier())));
}

// createDescriptor:

template class BLEPeripheralExtensionFunction<
    apibtle::CreateDescriptor::Params>;

void BluetoothLowEnergyCreateDescriptorFunction::DoWork() {
  device::BluetoothLocalGattCharacteristic* characteristic =
      event_router_->GetLocalCharacteristic(params_->characteristic_id);
  if (!characteristic) {
    Respond(Error(kErrorInvalidCharacteristicId));
    return;
  }

  base::WeakPtr<device::BluetoothLocalGattDescriptor> descriptor =
      device::BluetoothLocalGattDescriptor::Create(
          device::BluetoothUUID(params_->descriptor.uuid),
          GetBluetoothPermissions(params_->descriptor.permissions),
          characteristic);

  Respond(ArgumentList(
      apibtle::CreateDescriptor::Results::Create(descriptor->GetIdentifier())));
}

// registerService:

template class BLEPeripheralExtensionFunction<apibtle::RegisterService::Params>;

void BluetoothLowEnergyRegisterServiceFunction::DoWork() {
  event_router_->RegisterGattService(
      extension(), params_->service_id,
      base::Bind(&BluetoothLowEnergyRegisterServiceFunction::SuccessCallback,
                 this),
      base::Bind(&BluetoothLowEnergyRegisterServiceFunction::ErrorCallback,
                 this));
}

void BluetoothLowEnergyRegisterServiceFunction::SuccessCallback() {
  Respond(NoArguments());
}

void BluetoothLowEnergyRegisterServiceFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  Respond(Error(StatusToString(status)));
}

// unregisterService:

template class BLEPeripheralExtensionFunction<
    apibtle::UnregisterService::Params>;

void BluetoothLowEnergyUnregisterServiceFunction::DoWork() {
  event_router_->UnregisterGattService(
      extension(), params_->service_id,
      base::Bind(&BluetoothLowEnergyUnregisterServiceFunction::SuccessCallback,
                 this),
      base::Bind(&BluetoothLowEnergyUnregisterServiceFunction::ErrorCallback,
                 this));
}

void BluetoothLowEnergyUnregisterServiceFunction::SuccessCallback() {
  Respond(NoArguments());
}

void BluetoothLowEnergyUnregisterServiceFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  Respond(Error(StatusToString(status)));
}

// notifyCharacteristicValueChanged:

template class BLEPeripheralExtensionFunction<
    apibtle::NotifyCharacteristicValueChanged::Params>;

void BluetoothLowEnergyNotifyCharacteristicValueChangedFunction::DoWork() {
  device::BluetoothLocalGattCharacteristic* characteristic =
      event_router_->GetLocalCharacteristic(params_->characteristic_id);
  if (!characteristic) {
    Respond(Error(kErrorInvalidCharacteristicId));
    return;
  }
  std::vector<uint8_t> uint8_vector;
  uint8_vector.assign(params_->notification.value.begin(),
                      params_->notification.value.end());

  bool indicate = params_->notification.should_indicate.get()
                      ? *params_->notification.should_indicate
                      : false;
  device::BluetoothLocalGattCharacteristic::NotificationStatus status =
      characteristic->NotifyValueChanged(nullptr, uint8_vector, indicate);

  switch (status) {
    case device::BluetoothLocalGattCharacteristic::NOTIFICATION_SUCCESS:
      Respond(NoArguments());
      break;
    case device::BluetoothLocalGattCharacteristic::NOTIFY_PROPERTY_NOT_SET:
      Respond(Error(kErrorNotifyPropertyNotSet));
      break;
    case device::BluetoothLocalGattCharacteristic::INDICATE_PROPERTY_NOT_SET:
      Respond(Error(kErrorIndicatePropertyNotSet));
      break;
    case device::BluetoothLocalGattCharacteristic::SERVICE_NOT_REGISTERED:
      Respond(Error(kErrorServiceNotRegistered));
      break;
    default:
      LOG(ERROR) << "Unknown notification error!";
      Respond(Error(kErrorUnknownNotificationError));
  }
}

// removeService:

template class BLEPeripheralExtensionFunction<apibtle::RemoveService::Params>;

void BluetoothLowEnergyRemoveServiceFunction::DoWork() {
  device::BluetoothLocalGattService* service =
      event_router_->adapter()->GetGattService(params_->service_id);
  if (!service) {
    Respond(Error(kErrorInvalidServiceId));
    return;
  }
  event_router_->RemoveServiceFromApp(extension_id(), service->GetIdentifier());
  service->Delete();
  Respond(NoArguments());
}

// sendRequestResponse:

template class BLEPeripheralExtensionFunction<
    apibtle::SendRequestResponse::Params>;

void BluetoothLowEnergySendRequestResponseFunction::DoWork() {
  std::vector<uint8_t> uint8_vector;
  if (params_->response.value) {
    uint8_vector.assign(params_->response.value->begin(),
                        params_->response.value->end());
  }
  event_router_->HandleRequestResponse(
      extension(), params_->response.request_id, params_->response.is_error,
      uint8_vector);
  Respond(NoArguments());
}

}  // namespace api
}  // namespace extensions
