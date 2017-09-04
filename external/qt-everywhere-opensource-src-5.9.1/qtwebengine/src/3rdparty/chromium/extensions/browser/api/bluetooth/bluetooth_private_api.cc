// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth/bluetooth_private_api.h"

#include <stdint.h>

#include <utility>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "extensions/browser/api/bluetooth/bluetooth_api.h"
#include "extensions/browser/api/bluetooth/bluetooth_api_pairing_delegate.h"
#include "extensions/browser/api/bluetooth/bluetooth_event_router.h"
#include "extensions/common/api/bluetooth_private.h"

namespace bt_private = extensions::api::bluetooth_private;
namespace SetDiscoveryFilter = bt_private::SetDiscoveryFilter;

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<BluetoothPrivateAPI>>
    g_factory = LAZY_INSTANCE_INITIALIZER;

namespace {

std::string GetListenerId(const EventListenerInfo& details) {
  return !details.extension_id.empty() ? details.extension_id
                                       : details.listener_url.host();
}

}  // namespace

// static
BrowserContextKeyedAPIFactory<BluetoothPrivateAPI>*
BluetoothPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

BluetoothPrivateAPI::BluetoothPrivateAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter::Get(browser_context_)
      ->RegisterObserver(this, bt_private::OnPairing::kEventName);
}

BluetoothPrivateAPI::~BluetoothPrivateAPI() {}

void BluetoothPrivateAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void BluetoothPrivateAPI::OnListenerAdded(const EventListenerInfo& details) {
  // This function can be called multiple times for the same JS listener, for
  // example, once for the addListener call and again if it is a lazy listener.
  if (!details.browser_context)
    return;

  BluetoothAPI::Get(browser_context_)
      ->event_router()
      ->AddPairingDelegate(GetListenerId(details));
}

void BluetoothPrivateAPI::OnListenerRemoved(const EventListenerInfo& details) {
  // This function can be called multiple times for the same JS listener, for
  // example, once for the addListener call and again if it is a lazy listener.
  if (!details.browser_context)
    return;

  BluetoothAPI::Get(browser_context_)
      ->event_router()
      ->RemovePairingDelegate(GetListenerId(details));
}

namespace api {

namespace {

const char kNameProperty[] = "name";
const char kPoweredProperty[] = "powered";
const char kDiscoverableProperty[] = "discoverable";
const char kSetAdapterPropertyError[] = "Error setting adapter properties: $1";
const char kDeviceNotFoundError[] = "Invalid Bluetooth device";
const char kDeviceNotConnectedError[] = "Device not connected";
const char kPairingNotEnabled[] = "Pairing not enabled";
const char kInvalidPairingResponseOptions[] =
    "Invalid pairing response options";
const char kAdapterNotPresent[] = "Failed to find a Bluetooth adapter";
const char kDisconnectError[] = "Failed to disconnect device";
const char kForgetDeviceError[] = "Failed to forget device";
const char kSetDiscoveryFilterFailed[] = "Failed to set discovery filter";
const char kPairingFailed[] = "Pairing failed";

// Returns true if the pairing response options passed into the
// setPairingResponse function are valid.
bool ValidatePairingResponseOptions(
    const device::BluetoothDevice* device,
    const bt_private::SetPairingResponseOptions& options) {
  bool response = options.response != bt_private::PAIRING_RESPONSE_NONE;
  bool pincode = options.pincode != nullptr;
  bool passkey = options.passkey != nullptr;

  if (!response && !pincode && !passkey)
    return false;
  if (pincode && passkey)
    return false;
  if (options.response != bt_private::PAIRING_RESPONSE_CONFIRM &&
      (pincode || passkey))
    return false;

  if (options.response == bt_private::PAIRING_RESPONSE_CANCEL)
    return true;

  // Check the BluetoothDevice is in expecting the correct response.
  if (!device->ExpectingConfirmation() && !device->ExpectingPinCode() &&
      !device->ExpectingPasskey())
    return false;
  if (pincode && !device->ExpectingPinCode())
    return false;
  if (passkey && !device->ExpectingPasskey())
    return false;
  if (options.response == bt_private::PAIRING_RESPONSE_CONFIRM && !pincode &&
      !passkey && !device->ExpectingConfirmation())
    return false;

  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateSetAdapterStateFunction::
    BluetoothPrivateSetAdapterStateFunction() {}

BluetoothPrivateSetAdapterStateFunction::
    ~BluetoothPrivateSetAdapterStateFunction() {}

bool BluetoothPrivateSetAdapterStateFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  std::unique_ptr<bt_private::SetAdapterState::Params> params(
      bt_private::SetAdapterState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!adapter->IsPresent()) {
    SetError(kAdapterNotPresent);
    SendResponse(false);
    return true;
  }

  const bt_private::NewAdapterState& new_state = params->adapter_state;

  // These properties are not owned.
  std::string* name = new_state.name.get();
  bool* powered = new_state.powered.get();
  bool* discoverable = new_state.discoverable.get();

  if (name && adapter->GetName() != *name) {
    pending_properties_.insert(kNameProperty);
    adapter->SetName(*name, CreatePropertySetCallback(kNameProperty),
                     CreatePropertyErrorCallback(kNameProperty));
  }

  if (powered && adapter->IsPowered() != *powered) {
    pending_properties_.insert(kPoweredProperty);
    adapter->SetPowered(*powered, CreatePropertySetCallback(kPoweredProperty),
                        CreatePropertyErrorCallback(kPoweredProperty));
  }

  if (discoverable && adapter->IsDiscoverable() != *discoverable) {
    pending_properties_.insert(kDiscoverableProperty);
    adapter->SetDiscoverable(
        *discoverable, CreatePropertySetCallback(kDiscoverableProperty),
        CreatePropertyErrorCallback(kDiscoverableProperty));
  }

  parsed_ = true;

  if (pending_properties_.empty())
    SendResponse(true);
  return true;
}

base::Closure
BluetoothPrivateSetAdapterStateFunction::CreatePropertySetCallback(
    const std::string& property_name) {
  return base::Bind(
      &BluetoothPrivateSetAdapterStateFunction::OnAdapterPropertySet, this,
      property_name);
}

base::Closure
BluetoothPrivateSetAdapterStateFunction::CreatePropertyErrorCallback(
    const std::string& property_name) {
  return base::Bind(
      &BluetoothPrivateSetAdapterStateFunction::OnAdapterPropertyError, this,
      property_name);
}

void BluetoothPrivateSetAdapterStateFunction::OnAdapterPropertySet(
    const std::string& property) {
  DCHECK(pending_properties_.find(property) != pending_properties_.end());
  DCHECK(failed_properties_.find(property) == failed_properties_.end());

  pending_properties_.erase(property);
  if (pending_properties_.empty() && parsed_) {
    if (failed_properties_.empty())
      SendResponse(true);
    else
      SendError();
  }
}

void BluetoothPrivateSetAdapterStateFunction::OnAdapterPropertyError(
    const std::string& property) {
  DCHECK(pending_properties_.find(property) != pending_properties_.end());
  DCHECK(failed_properties_.find(property) == failed_properties_.end());
  pending_properties_.erase(property);
  failed_properties_.insert(property);
  if (pending_properties_.empty() && parsed_)
    SendError();
}

void BluetoothPrivateSetAdapterStateFunction::SendError() {
  DCHECK(pending_properties_.empty());
  DCHECK(!failed_properties_.empty());

  std::vector<std::string> failed_vector;
  std::copy(failed_properties_.begin(), failed_properties_.end(),
            std::back_inserter(failed_vector));

  std::vector<std::string> replacements(1);
  replacements[0] = base::JoinString(failed_vector, ", ");
  std::string error = base::ReplaceStringPlaceholders(kSetAdapterPropertyError,
                                                      replacements, nullptr);
  SetError(error);
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateSetPairingResponseFunction::
    BluetoothPrivateSetPairingResponseFunction() {}

BluetoothPrivateSetPairingResponseFunction::
    ~BluetoothPrivateSetPairingResponseFunction() {}

bool BluetoothPrivateSetPairingResponseFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  std::unique_ptr<bt_private::SetPairingResponse::Params> params(
      bt_private::SetPairingResponse::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const bt_private::SetPairingResponseOptions& options = params->options;

  BluetoothEventRouter* router =
      BluetoothAPI::Get(browser_context())->event_router();
  if (!router->GetPairingDelegate(GetExtensionId())) {
    SetError(kPairingNotEnabled);
    SendResponse(false);
    return true;
  }

  const std::string& device_address = options.device.address;
  device::BluetoothDevice* device = adapter->GetDevice(device_address);
  if (!device) {
    SetError(kDeviceNotFoundError);
    SendResponse(false);
    return true;
  }

  if (!ValidatePairingResponseOptions(device, options)) {
    SetError(kInvalidPairingResponseOptions);
    SendResponse(false);
    return true;
  }

  if (options.pincode.get()) {
    device->SetPinCode(*options.pincode);
  } else if (options.passkey.get()) {
    device->SetPasskey(*options.passkey);
  } else {
    switch (options.response) {
      case bt_private::PAIRING_RESPONSE_CONFIRM:
        device->ConfirmPairing();
        break;
      case bt_private::PAIRING_RESPONSE_REJECT:
        device->RejectPairing();
        break;
      case bt_private::PAIRING_RESPONSE_CANCEL:
        device->CancelPairing();
        break;
      default:
        NOTREACHED();
    }
  }

  SendResponse(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateDisconnectAllFunction::BluetoothPrivateDisconnectAllFunction() {
}

BluetoothPrivateDisconnectAllFunction::
    ~BluetoothPrivateDisconnectAllFunction() {}

bool BluetoothPrivateDisconnectAllFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  std::unique_ptr<bt_private::DisconnectAll::Params> params(
      bt_private::DisconnectAll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  device::BluetoothDevice* device = adapter->GetDevice(params->device_address);
  if (!device) {
    SetError(kDeviceNotFoundError);
    SendResponse(false);
    return true;
  }

  if (!device->IsConnected()) {
    SetError(kDeviceNotConnectedError);
    SendResponse(false);
    return true;
  }

  device->Disconnect(
      base::Bind(&BluetoothPrivateDisconnectAllFunction::OnSuccessCallback,
                 this),
      base::Bind(&BluetoothPrivateDisconnectAllFunction::OnErrorCallback, this,
                 adapter, params->device_address));

  return true;
}

void BluetoothPrivateDisconnectAllFunction::OnSuccessCallback() {
  SendResponse(true);
}

void BluetoothPrivateDisconnectAllFunction::OnErrorCallback(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address) {
  // The call to Disconnect may report an error if the device was disconnected
  // due to an external reason. In this case, report "Not Connected" as the
  // error.
  device::BluetoothDevice* device = adapter->GetDevice(device_address);
  if (device && !device->IsConnected())
    SetError(kDeviceNotConnectedError);
  else
    SetError(kDisconnectError);

  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateForgetDeviceFunction::BluetoothPrivateForgetDeviceFunction() {}

BluetoothPrivateForgetDeviceFunction::~BluetoothPrivateForgetDeviceFunction() {}

bool BluetoothPrivateForgetDeviceFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  std::unique_ptr<bt_private::ForgetDevice::Params> params(
      bt_private::ForgetDevice::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  device::BluetoothDevice* device = adapter->GetDevice(params->device_address);
  if (!device) {
    SetError(kDeviceNotFoundError);
    SendResponse(false);
    return true;
  }

  device->Forget(
      base::Bind(&BluetoothPrivateForgetDeviceFunction::OnSuccessCallback,
                 this),
      base::Bind(&BluetoothPrivateForgetDeviceFunction::OnErrorCallback, this,
                 adapter, params->device_address));

  return true;
}

void BluetoothPrivateForgetDeviceFunction::OnSuccessCallback() {
  SendResponse(true);
}

void BluetoothPrivateForgetDeviceFunction::OnErrorCallback(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address) {
  SetError(kForgetDeviceError);
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////

bool BluetoothPrivateSetDiscoveryFilterFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  std::unique_ptr<SetDiscoveryFilter::Params> params(
      SetDiscoveryFilter::Params::Create(*args_));
  auto& df_param = params->discovery_filter;

  std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter;

  // If all filter fields are empty, we are clearing filter. If any field is
  // set, then create proper filter.
  if (df_param.uuids.get() || df_param.rssi.get() || df_param.pathloss.get() ||
      df_param.transport != bt_private::TransportType::TRANSPORT_TYPE_NONE) {
    device::BluetoothTransport transport;

    switch (df_param.transport) {
      case bt_private::TransportType::TRANSPORT_TYPE_LE:
        transport = device::BLUETOOTH_TRANSPORT_LE;
        break;
      case bt_private::TransportType::TRANSPORT_TYPE_BREDR:
        transport = device::BLUETOOTH_TRANSPORT_CLASSIC;
        break;
      default:  // TRANSPORT_TYPE_NONE is included here
        transport = device::BLUETOOTH_TRANSPORT_DUAL;
        break;
    }

    discovery_filter.reset(new device::BluetoothDiscoveryFilter(transport));

    if (df_param.uuids.get()) {
      std::vector<device::BluetoothUUID> uuids;
      if (df_param.uuids->as_string.get()) {
        discovery_filter->AddUUID(
            device::BluetoothUUID(*df_param.uuids->as_string));
      } else if (df_param.uuids->as_strings.get()) {
        for (const auto& iter : *df_param.uuids->as_strings) {
          discovery_filter->AddUUID(device::BluetoothUUID(iter));
        }
      }
    }

    if (df_param.rssi.get())
      discovery_filter->SetRSSI(*df_param.rssi);

    if (df_param.pathloss.get())
      discovery_filter->SetPathloss(*df_param.pathloss);
  }

  BluetoothAPI::Get(browser_context())
      ->event_router()
      ->SetDiscoveryFilter(
          std::move(discovery_filter), adapter.get(), GetExtensionId(),
          base::Bind(
              &BluetoothPrivateSetDiscoveryFilterFunction::OnSuccessCallback,
              this),
          base::Bind(
              &BluetoothPrivateSetDiscoveryFilterFunction::OnErrorCallback,
              this));
  return true;
}

void BluetoothPrivateSetDiscoveryFilterFunction::OnSuccessCallback() {
  SendResponse(true);
}

void BluetoothPrivateSetDiscoveryFilterFunction::OnErrorCallback() {
  SetError(kSetDiscoveryFilterFailed);
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateConnectFunction::BluetoothPrivateConnectFunction() {}

BluetoothPrivateConnectFunction::~BluetoothPrivateConnectFunction() {}

bool BluetoothPrivateConnectFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  std::unique_ptr<bt_private::Connect::Params> params(
      bt_private::Connect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  device::BluetoothDevice* device = adapter->GetDevice(params->device_address);
  if (!device) {
    SetError(kDeviceNotFoundError);
    SendResponse(false);
    return true;
  }

  if (device->IsConnected()) {
    results_ = bt_private::Connect::Results::Create(
        bt_private::CONNECT_RESULT_TYPE_ALREADYCONNECTED);
    SendResponse(true);
    return true;
  }

  // pairing_delegate may be null for connect.
  device::BluetoothDevice::PairingDelegate* pairing_delegate =
      BluetoothAPI::Get(browser_context())
          ->event_router()
          ->GetPairingDelegate(GetExtensionId());
  device->Connect(
      pairing_delegate,
      base::Bind(&BluetoothPrivateConnectFunction::OnSuccessCallback, this),
      base::Bind(&BluetoothPrivateConnectFunction::OnErrorCallback, this));
  return true;
}

void BluetoothPrivateConnectFunction::OnSuccessCallback() {
  results_ = bt_private::Connect::Results::Create(
      bt_private::CONNECT_RESULT_TYPE_SUCCESS);
  SendResponse(true);
}

void BluetoothPrivateConnectFunction::OnErrorCallback(
    device::BluetoothDevice::ConnectErrorCode error) {
  bt_private::ConnectResultType result = bt_private::CONNECT_RESULT_TYPE_NONE;
  switch (error) {
    case device::BluetoothDevice::ERROR_ATTRIBUTE_LENGTH_INVALID:
      result = bt_private::CONNECT_RESULT_TYPE_ATTRIBUTELENGTHINVALID;
      break;
    case device::BluetoothDevice::ERROR_AUTH_CANCELED:
      result = bt_private::CONNECT_RESULT_TYPE_AUTHCANCELED;
      break;
    case device::BluetoothDevice::ERROR_AUTH_FAILED:
      result = bt_private::CONNECT_RESULT_TYPE_AUTHFAILED;
      break;
    case device::BluetoothDevice::ERROR_AUTH_REJECTED:
      result = bt_private::CONNECT_RESULT_TYPE_AUTHREJECTED;
      break;
    case device::BluetoothDevice::ERROR_AUTH_TIMEOUT:
      result = bt_private::CONNECT_RESULT_TYPE_AUTHTIMEOUT;
      break;
    case device::BluetoothDevice::ERROR_CONNECTION_CONGESTED:
      result = bt_private::CONNECT_RESULT_TYPE_CONNECTIONCONGESTED;
      break;
    case device::BluetoothDevice::ERROR_FAILED:
      result = bt_private::CONNECT_RESULT_TYPE_FAILED;
      break;
    case device::BluetoothDevice::ERROR_INPROGRESS:
      result = bt_private::CONNECT_RESULT_TYPE_INPROGRESS;
      break;
    case device::BluetoothDevice::ERROR_INSUFFICIENT_ENCRYPTION:
      result = bt_private::CONNECT_RESULT_TYPE_INSUFFICIENTENCRYPTION;
      break;
    case device::BluetoothDevice::ERROR_OFFSET_INVALID:
      result = bt_private::CONNECT_RESULT_TYPE_OFFSETINVALID;
      break;
    case device::BluetoothDevice::ERROR_READ_NOT_PERMITTED:
      result = bt_private::CONNECT_RESULT_TYPE_READNOTPERMITTED;
      break;
    case device::BluetoothDevice::ERROR_REQUEST_NOT_SUPPORTED:
      result = bt_private::CONNECT_RESULT_TYPE_REQUESTNOTSUPPORTED;
      break;
    case device::BluetoothDevice::ERROR_UNKNOWN:
      result = bt_private::CONNECT_RESULT_TYPE_UNKNOWNERROR;
      break;
    case device::BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      result = bt_private::CONNECT_RESULT_TYPE_UNSUPPORTEDDEVICE;
      break;
    case device::BluetoothDevice::ERROR_WRITE_NOT_PERMITTED:
      result = bt_private::CONNECT_RESULT_TYPE_WRITENOTPERMITTED;
      break;
    case device::BluetoothDevice::NUM_CONNECT_ERROR_CODES:
      NOTREACHED();
      break;
  }
  // Set the result type and respond with true (success).
  results_ = bt_private::Connect::Results::Create(result);
  SendResponse(true);
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivatePairFunction::BluetoothPrivatePairFunction() {}

BluetoothPrivatePairFunction::~BluetoothPrivatePairFunction() {}

bool BluetoothPrivatePairFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  std::unique_ptr<bt_private::Pair::Params> params(
      bt_private::Pair::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  device::BluetoothDevice* device = adapter->GetDevice(params->device_address);
  if (!device) {
    SetError(kDeviceNotFoundError);
    SendResponse(false);
    return true;
  }

  device::BluetoothDevice::PairingDelegate* pairing_delegate =
      BluetoothAPI::Get(browser_context())
          ->event_router()
          ->GetPairingDelegate(GetExtensionId());

  // pairing_delegate must be set (by adding an onPairing listener) before
  // any calls to pair().
  if (!pairing_delegate) {
    SetError(kPairingNotEnabled);
    SendResponse(false);
    return true;
  }

  device->Pair(
      pairing_delegate,
      base::Bind(&BluetoothPrivatePairFunction::OnSuccessCallback, this),
      base::Bind(&BluetoothPrivatePairFunction::OnErrorCallback, this));
  return true;
}

void BluetoothPrivatePairFunction::OnSuccessCallback() {
  SendResponse(true);
}

void BluetoothPrivatePairFunction::OnErrorCallback(
    device::BluetoothDevice::ConnectErrorCode error) {
  SetError(kPairingFailed);
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////

}  // namespace api

}  // namespace extensions
