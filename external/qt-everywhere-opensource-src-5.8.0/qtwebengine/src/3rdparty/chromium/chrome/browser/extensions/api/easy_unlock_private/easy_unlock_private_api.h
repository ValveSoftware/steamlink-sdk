// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/browser/api/bluetooth/bluetooth_extension_function.h"
#include "extensions/browser/api/bluetooth_socket/bluetooth_socket_api.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

// Implementations for chrome.easyUnlockPrivate API functions.

namespace base {
class Timer;
}

namespace content {
class BrowserContext;
}

namespace cryptauth {
class ExternalDeviceInfo;
}

namespace proximity_auth {
class Connection;
class BluetoothLowEnergyConnectionFinder;
class BluetoothThrottler;
class SecureMessageDelegate;
}

namespace extensions {

class EasyUnlockPrivateConnectionManager;
class EasyUnlockPrivateCryptoDelegate;

class EasyUnlockPrivateAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>*
      GetFactoryInstance();

  static const bool kServiceRedirectedInIncognito = true;

  explicit EasyUnlockPrivateAPI(content::BrowserContext* context);
  ~EasyUnlockPrivateAPI() override;

  EasyUnlockPrivateCryptoDelegate* GetCryptoDelegate();

  EasyUnlockPrivateConnectionManager* get_connection_manager() {
    return connection_manager_.get();
  }

 private:
  friend class BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "EasyUnlockPrivate"; }

  std::unique_ptr<EasyUnlockPrivateCryptoDelegate> crypto_delegate_;

  std::unique_ptr<EasyUnlockPrivateConnectionManager> connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateAPI);
};

// TODO(tbarzic): Replace SyncExtensionFunction/AsyncExtensionFunction overrides
// with UIThreadExtensionFunction throughout the file.
class EasyUnlockPrivateGetStringsFunction : public SyncExtensionFunction {
 public:
  EasyUnlockPrivateGetStringsFunction();

 protected:
  ~EasyUnlockPrivateGetStringsFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

 private:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getStrings",
                             EASYUNLOCKPRIVATE_GETSTRINGS)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetStringsFunction);
};

class EasyUnlockPrivatePerformECDHKeyAgreementFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivatePerformECDHKeyAgreementFunction();

 protected:
  ~EasyUnlockPrivatePerformECDHKeyAgreementFunction() override;

  bool RunAsync() override;

 private:
  void OnData(const std::string& secret_key);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.performECDHKeyAgreement",
                             EASYUNLOCKPRIVATE_PERFORMECDHKEYAGREEMENT)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivatePerformECDHKeyAgreementFunction);
};

class EasyUnlockPrivateGenerateEcP256KeyPairFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivateGenerateEcP256KeyPairFunction();

 protected:
  ~EasyUnlockPrivateGenerateEcP256KeyPairFunction() override;

  bool RunAsync() override;

 private:
  void OnData(const std::string& public_key,
              const std::string& private_key);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.generateEcP256KeyPair",
                             EASYUNLOCKPRIVATE_GENERATEECP256KEYPAIR)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGenerateEcP256KeyPairFunction);
};

class EasyUnlockPrivateCreateSecureMessageFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivateCreateSecureMessageFunction();

 protected:
  ~EasyUnlockPrivateCreateSecureMessageFunction() override;

  bool RunAsync() override;

 private:
  void OnData(const std::string& message);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.createSecureMessage",
                             EASYUNLOCKPRIVATE_CREATESECUREMESSAGE)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateCreateSecureMessageFunction);
};

class EasyUnlockPrivateUnwrapSecureMessageFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivateUnwrapSecureMessageFunction();

 protected:
  ~EasyUnlockPrivateUnwrapSecureMessageFunction() override;

  bool RunAsync() override;

 private:
  void OnData(const std::string& data);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.unwrapSecureMessage",
                             EASYUNLOCKPRIVATE_UNWRAPSECUREMESSAGE)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateUnwrapSecureMessageFunction);
};

class EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.seekBluetoothDeviceByAddress",
                             EASYUNLOCKPRIVATE_SEEKBLUETOOTHDEVICEBYADDRESS)
  EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction();

 private:
  ~EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction() override;

  // AsyncExtensionFunction:
  bool RunAsync() override;

  // Callbacks that are called when the seek operation succeeds or fails.
  void OnSeekSuccess();
  void OnSeekFailure(const std::string& error_message);

  DISALLOW_COPY_AND_ASSIGN(
      EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction);
};

class EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction
    : public api::BluetoothSocketAbstractConnectFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "easyUnlockPrivate.connectToBluetoothServiceInsecurely",
      EASYUNLOCKPRIVATE_CONNECTTOBLUETOOTHSERVICEINSECURELY)
  EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction();

 private:
  ~EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction() override;

  // BluetoothSocketAbstractConnectFunction:
  void ConnectToService(device::BluetoothDevice* device,
                        const device::BluetoothUUID& uuid) override;

  DISALLOW_COPY_AND_ASSIGN(
      EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction);
};

class EasyUnlockPrivateUpdateScreenlockStateFunction
    : public SyncExtensionFunction {
 public:
  EasyUnlockPrivateUpdateScreenlockStateFunction();

 protected:
  ~EasyUnlockPrivateUpdateScreenlockStateFunction() override;

  bool RunSync() override;

 private:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.updateScreenlockState",
                             EASYUNLOCKPRIVATE_UPDATESCREENLOCKSTATE)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateUpdateScreenlockStateFunction);
};

class EasyUnlockPrivateSetPermitAccessFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setPermitAccess",
                             EASYUNLOCKPRIVATE_SETPERMITACCESS)
  EasyUnlockPrivateSetPermitAccessFunction();

 private:
  ~EasyUnlockPrivateSetPermitAccessFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetPermitAccessFunction);
};

class EasyUnlockPrivateGetPermitAccessFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getPermitAccess",
                             EASYUNLOCKPRIVATE_GETPERMITACCESS)
  EasyUnlockPrivateGetPermitAccessFunction();

 protected:
  ~EasyUnlockPrivateGetPermitAccessFunction() override;

  // Writes the user's public and private key in base64 form to the
  // |user_public_key| and |user_private_key| fields. Exposed for testing.
  virtual void GetKeyPairForExperiment(std::string* user_public_key,
                                       std::string* user_private_key);

 private:
  // SyncExtensionFunction:
  bool RunSync() override;

  // Instead of returning the value set by easyUnlockPrivate.setPermitAccess,
  // return the permit access used by the native CryptAuthEnrollmentManager.
  void ReturnPermitAccessForExperiment();

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetPermitAccessFunction);
};

class EasyUnlockPrivateClearPermitAccessFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.clearPermitAccess",
                             EASYUNLOCKPRIVATE_CLEARPERMITACCESS)
  EasyUnlockPrivateClearPermitAccessFunction();

 private:
  ~EasyUnlockPrivateClearPermitAccessFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateClearPermitAccessFunction);
};

class EasyUnlockPrivateSetRemoteDevicesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setRemoteDevices",
                             EASYUNLOCKPRIVATE_SETREMOTEDEVICES)
  EasyUnlockPrivateSetRemoteDevicesFunction();

 private:
  ~EasyUnlockPrivateSetRemoteDevicesFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetRemoteDevicesFunction);
};

class EasyUnlockPrivateGetRemoteDevicesFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getRemoteDevices",
                             EASYUNLOCKPRIVATE_GETREMOTEDEVICES)
  EasyUnlockPrivateGetRemoteDevicesFunction();

 protected:
  ~EasyUnlockPrivateGetRemoteDevicesFunction() override;

  // Returns the user's private key used for the native experiment.
  // Exposed for testing.
  virtual std::string GetUserPrivateKey();

  // Returns the user's unlock keys used for the native experiment.
  // Exposed for testing.
  virtual std::vector<cryptauth::ExternalDeviceInfo> GetUnlockKeys();

 private:
  // AsyncExtensionFunction:
  bool RunAsync() override;

  // Returns devices managed by the native Chrome component if the
  // kEnableBluetoothLowEnergyDiscovery flag is set.
  void ReturnDevicesForExperiment();

  // Callback when the PSK of a device is derived.
  void OnPSKDerivedForDevice(const cryptauth::ExternalDeviceInfo& device,
                             const std::string& persistent_symmetric_key);

  // The permit id of the user. Used for the native experiment.
  std::string permit_id_;

  // The expected number of devices to return. Used for the native experiment.
  size_t expected_devices_count_;

  // Working list of the devices to return. Used for the native experiment.
  std::unique_ptr<base::ListValue> remote_devices_;

  // Used to derive devices' PSK. Used for the native experiment.
  std::unique_ptr<proximity_auth::SecureMessageDelegate>
      secure_message_delegate_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetRemoteDevicesFunction);
};

class EasyUnlockPrivateGetSignInChallengeFunction :
    public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getSignInChallenge",
                             EASYUNLOCKPRIVATE_GETSIGNINCHALLENGE)
  EasyUnlockPrivateGetSignInChallengeFunction();

 private:
  ~EasyUnlockPrivateGetSignInChallengeFunction() override;

  // AsyncExtensionFunction:
  bool RunAsync() override;

  // Called when the challenge and the signed nonce have been generated.
  void OnDone(const std::string& challenge, const std::string& signed_nonce);

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetSignInChallengeFunction);
};

class EasyUnlockPrivateTrySignInSecretFunction :
    public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.trySignInSecret",
                             EASYUNLOCKPRIVATE_TRYSIGNINSECRET)
  EasyUnlockPrivateTrySignInSecretFunction();

 private:
  ~EasyUnlockPrivateTrySignInSecretFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateTrySignInSecretFunction);
};

class EasyUnlockPrivateGetUserInfoFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getUserInfo",
                             EASYUNLOCKPRIVATE_GETUSERINFO)
  EasyUnlockPrivateGetUserInfoFunction();

 private:
  ~EasyUnlockPrivateGetUserInfoFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetUserInfoFunction);
};

class EasyUnlockPrivateGetConnectionInfoFunction
    : public api::BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getConnectionInfo",
                             EASYUNLOCKPRIVATE_GETCONNECTIONINFO)
  EasyUnlockPrivateGetConnectionInfoFunction();

 private:
  ~EasyUnlockPrivateGetConnectionInfoFunction() override;

  // BluetoothExtensionFunction:
  bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

  void OnConnectionInfo(
      const device::BluetoothDevice::ConnectionInfo& connection_info);

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetConnectionInfoFunction);
};

class EasyUnlockPrivateShowErrorBubbleFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.showErrorBubble",
                             EASYUNLOCKPRIVATE_SHOWERRORBUBBLE)
  EasyUnlockPrivateShowErrorBubbleFunction();

 private:
  ~EasyUnlockPrivateShowErrorBubbleFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateShowErrorBubbleFunction);
};

class EasyUnlockPrivateHideErrorBubbleFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.hideErrorBubble",
                             EASYUNLOCKPRIVATE_HIDEERRORBUBBLE)
  EasyUnlockPrivateHideErrorBubbleFunction();

 private:
  ~EasyUnlockPrivateHideErrorBubbleFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateHideErrorBubbleFunction);
};

class EasyUnlockPrivateSetAutoPairingResultFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setAutoPairingResult",
                             EASYUNLOCKPRIVATE_SETAUTOPAIRINGRESULT)
  EasyUnlockPrivateSetAutoPairingResultFunction();

 private:
  ~EasyUnlockPrivateSetAutoPairingResultFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetAutoPairingResultFunction);
};

class EasyUnlockPrivateFindSetupConnectionFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.findSetupConnection",
                             EASYUNLOCKPRIVATE_FINDSETUPCONNECTION)
  EasyUnlockPrivateFindSetupConnectionFunction();

 private:
  ~EasyUnlockPrivateFindSetupConnectionFunction() override;

  // AsyncExtensionFunction:
  bool RunAsync() override;

  // Called when the connection with the remote device advertising the setup
  // service was found.
  void OnConnectionFound(
      std::unique_ptr<proximity_auth::Connection> connection);

  // Callback when waiting for |connection_finder_| to return.
  void OnConnectionFinderTimedOut();

  // The BLE connection finder instance.
  std::unique_ptr<proximity_auth::BluetoothLowEnergyConnectionFinder>
      connection_finder_;

  // The connection throttler passed to the BLE connection finder.
  std::unique_ptr<proximity_auth::BluetoothThrottler> bluetooth_throttler_;

  // Used for timing out when waiting for the connection finder to return.
  std::unique_ptr<base::Timer> timer_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateFindSetupConnectionFunction);
};

class EasyUnlockPrivateSetupConnectionStatusFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setupConnectionStatus",
                             EASYUNLOCKPRIVATE_SETUPCONNECTIONSTATUS)
  EasyUnlockPrivateSetupConnectionStatusFunction();

 private:
  ~EasyUnlockPrivateSetupConnectionStatusFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetupConnectionStatusFunction);
};

class EasyUnlockPrivateSetupConnectionDisconnectFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setupConnectionDisconnect",
                             EASYUNLOCKPRIVATE_SETUPCONNECTIONDISCONNECT)
  EasyUnlockPrivateSetupConnectionDisconnectFunction();

 private:
  ~EasyUnlockPrivateSetupConnectionDisconnectFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetupConnectionDisconnectFunction);
};

class EasyUnlockPrivateSetupConnectionSendFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setupConnectionSend",
                             EASYUNLOCKPRIVATE_SETUPCONNECTIONSEND)
  EasyUnlockPrivateSetupConnectionSendFunction();

 private:
  ~EasyUnlockPrivateSetupConnectionSendFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetupConnectionSendFunction);
};

class EasyUnlockPrivateSetupConnectionGetDeviceAddressFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "easyUnlockPrivate.setupConnectionGetDeviceAddress",
      EASYUNLOCKPRIVATE_SETUPCONNECTIONGETDEVICEADDRESS)
  EasyUnlockPrivateSetupConnectionGetDeviceAddressFunction();

 private:
  ~EasyUnlockPrivateSetupConnectionGetDeviceAddressFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(
      EasyUnlockPrivateSetupConnectionGetDeviceAddressFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_
