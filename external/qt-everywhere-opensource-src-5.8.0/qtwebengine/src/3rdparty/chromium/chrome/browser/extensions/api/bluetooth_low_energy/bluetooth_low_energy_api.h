// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_API_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_api_advertisement.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_event_router.h"
#include "content/public/browser/browser_context.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {
class BluetoothApiAdvertisement;
class BluetoothLowEnergyEventRouter;

namespace api {
namespace bluetooth_low_energy {
namespace CreateService {
struct Params;
}
namespace CreateCharacteristic {
struct Params;
}
namespace CreateDescriptor {
struct Params;
}
namespace RegisterService {
struct Params;
}
namespace UnregisterService {
struct Params;
}
namespace NotifyCharacteristicValueChanged {
struct Params;
}
namespace RemoveService {
struct Params;
}
namespace SendRequestResponse {
struct Params;
}
}  // namespace bluetooth_low_energy
}  // namespace api

// The profile-keyed service that manages the bluetoothLowEnergy extension API.
class BluetoothLowEnergyAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI>*
      GetFactoryInstance();

  // Convenience method to get the BluetoothLowEnergy API for a browser context.
  static BluetoothLowEnergyAPI* Get(content::BrowserContext* context);

  explicit BluetoothLowEnergyAPI(content::BrowserContext* context);
  ~BluetoothLowEnergyAPI() override;

  // KeyedService implementation..
  void Shutdown() override;

  BluetoothLowEnergyEventRouter* event_router() const {
    return event_router_.get();
  }

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BluetoothLowEnergyAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

 private:
  friend class BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI>;

  std::unique_ptr<BluetoothLowEnergyEventRouter> event_router_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyAPI);
};

namespace api {

// Base class for bluetoothLowEnergy API functions. This class handles some of
// the common logic involved in all API functions, such as checking for
// platform support and returning the correct error.
//
// DEPRECATED: This inherits from AsyncExtensionFunction, which we're trying to
// get rid of for various reasons. Please inherit from the
// BluetoothLowEnergyExtensionFunction class instead.
class BluetoothLowEnergyExtensionFunctionDeprecated
    : public AsyncExtensionFunction {
 public:
  BluetoothLowEnergyExtensionFunctionDeprecated();

 protected:
  ~BluetoothLowEnergyExtensionFunctionDeprecated() override;

  // AsyncExtensionFunction override.
  bool RunAsync() override;

  // Implemented by individual bluetoothLowEnergy extension functions to perform
  // the body of the function. This invoked asynchonously after RunAsync after
  // the BluetoothLowEnergyEventRouter has obtained a handle on the
  // BluetoothAdapter.
  virtual bool DoWork() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyExtensionFunctionDeprecated);
};

// Replacement for BluetoothLowEnergyExtensionFunctionDeprecated. Has the same
// functionality except that instead of the SendResponse/return combo, we'll
// return our response with Respond().
class BluetoothLowEnergyExtensionFunction : public UIThreadExtensionFunction {
 public:
  BluetoothLowEnergyExtensionFunction();

 protected:
  ~BluetoothLowEnergyExtensionFunction() override;

  // ExtensionFunction override.
  ResponseAction Run() override;

  // Implemented by individual bluetoothLowEnergy extension functions to perform
  // the body of the function. This invoked asynchonously after Run after
  // the BluetoothLowEnergyEventRouter has obtained a handle on the
  // BluetoothAdapter.
  virtual void DoWork() = 0;

  BluetoothLowEnergyEventRouter* event_router_;

 private:
  // Internal method to do common setup before actual DoWork is called.
  void PreDoWork();

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyExtensionFunction);
};

// Base class for bluetoothLowEnergy API peripheral mode functions. This class
// handles some of the common logic involved in all API peripheral mode
// functions, such as checking for peripheral permissions and returning the
// correct error.
template <typename Params>
class BLEPeripheralExtensionFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  BLEPeripheralExtensionFunction();

 protected:
  ~BLEPeripheralExtensionFunction() override;

  // ExtensionFunction override.
  ResponseAction Run() override;

// Causes link error on Windows. API will never be on Windows, so #ifdefing.
#if !defined(OS_WIN)
  std::unique_ptr<Params> params_;
#else
  Params* params_;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(BLEPeripheralExtensionFunction);
};

class BluetoothLowEnergyConnectFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.connect",
                             BLUETOOTHLOWENERGY_CONNECT);

 protected:
  ~BluetoothLowEnergyConnectFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::Connect.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyDisconnectFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.disconnect",
                             BLUETOOTHLOWENERGY_DISCONNECT);

 protected:
  ~BluetoothLowEnergyDisconnectFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::Disconnect.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyGetServiceFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getService",
                             BLUETOOTHLOWENERGY_GETSERVICE);

 protected:
  ~BluetoothLowEnergyGetServiceFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;
};

class BluetoothLowEnergyGetServicesFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getServices",
                             BLUETOOTHLOWENERGY_GETSERVICES);

 protected:
  ~BluetoothLowEnergyGetServicesFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;
};

class BluetoothLowEnergyGetCharacteristicFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getCharacteristic",
                             BLUETOOTHLOWENERGY_GETCHARACTERISTIC);

 protected:
  ~BluetoothLowEnergyGetCharacteristicFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;
};

class BluetoothLowEnergyGetCharacteristicsFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getCharacteristics",
                             BLUETOOTHLOWENERGY_GETCHARACTERISTICS);

 protected:
  ~BluetoothLowEnergyGetCharacteristicsFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;
};

class BluetoothLowEnergyGetIncludedServicesFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getIncludedServices",
                             BLUETOOTHLOWENERGY_GETINCLUDEDSERVICES);

 protected:
  ~BluetoothLowEnergyGetIncludedServicesFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;
};

class BluetoothLowEnergyGetDescriptorFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getDescriptor",
                             BLUETOOTHLOWENERGY_GETDESCRIPTOR);

 protected:
  ~BluetoothLowEnergyGetDescriptorFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;
};

class BluetoothLowEnergyGetDescriptorsFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getDescriptors",
                             BLUETOOTHLOWENERGY_GETDESCRIPTORS);

 protected:
  ~BluetoothLowEnergyGetDescriptorsFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;
};

class BluetoothLowEnergyReadCharacteristicValueFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.readCharacteristicValue",
                             BLUETOOTHLOWENERGY_READCHARACTERISTICVALUE);

 protected:
  ~BluetoothLowEnergyReadCharacteristicValueFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::ReadCharacteristicValue.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);

  // The instance ID of the requested characteristic.
  std::string instance_id_;
};

class BluetoothLowEnergyWriteCharacteristicValueFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.writeCharacteristicValue",
                             BLUETOOTHLOWENERGY_WRITECHARACTERISTICVALUE);

 protected:
  ~BluetoothLowEnergyWriteCharacteristicValueFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::WriteCharacteristicValue.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);

  // The instance ID of the requested characteristic.
  std::string instance_id_;
};

class BluetoothLowEnergyStartCharacteristicNotificationsFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "bluetoothLowEnergy.startCharacteristicNotifications",
      BLUETOOTHLOWENERGY_STARTCHARACTERISTICNOTIFICATIONS);

 protected:
  ~BluetoothLowEnergyStartCharacteristicNotificationsFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::StartCharacteristicNotifications.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyStopCharacteristicNotificationsFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "bluetoothLowEnergy.stopCharacteristicNotifications",
      BLUETOOTHLOWENERGY_STOPCHARACTERISTICNOTIFICATIONS);

 protected:
  ~BluetoothLowEnergyStopCharacteristicNotificationsFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::StopCharacteristicNotifications.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyReadDescriptorValueFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.readDescriptorValue",
                             BLUETOOTHLOWENERGY_READDESCRIPTORVALUE);

 protected:
  ~BluetoothLowEnergyReadDescriptorValueFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::ReadDescriptorValue.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);

  // The instance ID of the requested descriptor.
  std::string instance_id_;
};

class BluetoothLowEnergyWriteDescriptorValueFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.writeDescriptorValue",
                             BLUETOOTHLOWENERGY_WRITEDESCRIPTORVALUE);

 protected:
  ~BluetoothLowEnergyWriteDescriptorValueFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::WriteDescriptorValue.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);

  // The instance ID of the requested descriptor.
  std::string instance_id_;
};

class BluetoothLowEnergyAdvertisementFunction
    : public BluetoothLowEnergyExtensionFunctionDeprecated {
 public:
  BluetoothLowEnergyAdvertisementFunction();

 protected:
  ~BluetoothLowEnergyAdvertisementFunction() override;

  // Takes ownership.
  int AddAdvertisement(BluetoothApiAdvertisement* advertisement);
  BluetoothApiAdvertisement* GetAdvertisement(int advertisement_id);
  void RemoveAdvertisement(int advertisement_id);

  // ExtensionFunction override.
  bool RunAsync() override;

 private:
  void Initialize();

  ApiResourceManager<BluetoothApiAdvertisement>* advertisements_manager_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyAdvertisementFunction);
};

class BluetoothLowEnergyRegisterAdvertisementFunction
    : public BluetoothLowEnergyAdvertisementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.registerAdvertisement",
                             BLUETOOTHLOWENERGY_REGISTERADVERTISEMENT);

 protected:
  ~BluetoothLowEnergyRegisterAdvertisementFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;

 private:
  void SuccessCallback(scoped_refptr<device::BluetoothAdvertisement>);
  void ErrorCallback(device::BluetoothAdvertisement::ErrorCode status);

  // The instance ID of the requested descriptor.
  std::string instance_id_;
};

class BluetoothLowEnergyUnregisterAdvertisementFunction
    : public BluetoothLowEnergyAdvertisementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.unregisterAdvertisement",
                             BLUETOOTHLOWENERGY_UNREGISTERADVERTISEMENT);

 protected:
  ~BluetoothLowEnergyUnregisterAdvertisementFunction() override {}

  // BluetoothLowEnergyExtensionFunctionDeprecated override.
  bool DoWork() override;

 private:
  void SuccessCallback(int advertisement_id);
  void ErrorCallback(int advertisement_id,
                     device::BluetoothAdvertisement::ErrorCode status);

  // The instance ID of the requested descriptor.
  std::string instance_id_;
};

class BluetoothLowEnergyCreateServiceFunction
    : public BLEPeripheralExtensionFunction<
          extensions::api::bluetooth_low_energy::CreateService::Params> {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.createService",
                             BLUETOOTHLOWENERGY_CREATESERVICE);

 protected:
  ~BluetoothLowEnergyCreateServiceFunction() override {}

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
};

class BluetoothLowEnergyCreateCharacteristicFunction
    : public BLEPeripheralExtensionFunction<
          extensions::api::bluetooth_low_energy::CreateCharacteristic::Params> {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.createCharacteristic",
                             BLUETOOTHLOWENERGY_CREATECHARACTERISTIC);

 protected:
  ~BluetoothLowEnergyCreateCharacteristicFunction() override {}

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
};

class BluetoothLowEnergyNotifyCharacteristicValueChangedFunction
    : public BLEPeripheralExtensionFunction<
          extensions::api::bluetooth_low_energy::
              NotifyCharacteristicValueChanged::Params> {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "bluetoothLowEnergy.notifyCharacteristicValueChanged",
      BLUETOOTHLOWENERGY_NOTIFYCHARACTERISTICVALUECHANGED);

 protected:
  ~BluetoothLowEnergyNotifyCharacteristicValueChangedFunction() override {}

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
};

class BluetoothLowEnergyCreateDescriptorFunction
    : public BLEPeripheralExtensionFunction<
          extensions::api::bluetooth_low_energy::CreateDescriptor::Params> {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.createDescriptor",
                             BLUETOOTHLOWENERGY_CREATEDESCRIPTOR);

 protected:
  ~BluetoothLowEnergyCreateDescriptorFunction() override {}

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
};

class BluetoothLowEnergyRegisterServiceFunction
    : public BLEPeripheralExtensionFunction<
          extensions::api::bluetooth_low_energy::RegisterService::Params> {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.registerService",
                             BLUETOOTHLOWENERGY_REGISTERSERVICE);

 protected:
  ~BluetoothLowEnergyRegisterServiceFunction() override {}

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;

 private:
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyUnregisterServiceFunction
    : public BLEPeripheralExtensionFunction<
          extensions::api::bluetooth_low_energy::UnregisterService::Params> {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.unregisterService",
                             BLUETOOTHLOWENERGY_UNREGISTERSERVICE);

 protected:
  ~BluetoothLowEnergyUnregisterServiceFunction() override {}

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::RegisterService.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyRemoveServiceFunction
    : public BLEPeripheralExtensionFunction<
          extensions::api::bluetooth_low_energy::RemoveService::Params> {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.removeService",
                             BLUETOOTHLOWENERGY_REMOVESERVICE);

 protected:
  ~BluetoothLowEnergyRemoveServiceFunction() override {}

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
};

class BluetoothLowEnergySendRequestResponseFunction
    : public BLEPeripheralExtensionFunction<
          extensions::api::bluetooth_low_energy::SendRequestResponse::Params> {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.sendRequestResponse",
                             BLUETOOTHLOWENERGY_SENDREQUESTRESPONSE);

 protected:
  ~BluetoothLowEnergySendRequestResponseFunction() override {}

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_API_H_
