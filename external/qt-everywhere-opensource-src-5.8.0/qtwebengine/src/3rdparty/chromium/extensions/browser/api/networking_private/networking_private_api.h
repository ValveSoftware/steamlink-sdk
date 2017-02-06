// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_API_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace networking_private {

extern const char kErrorInvalidNetworkGuid[];
extern const char kErrorInvalidNetworkOperation[];
extern const char kErrorNetworkUnavailable[];
extern const char kErrorEncryptionError[];
extern const char kErrorNotReady[];
extern const char kErrorNotSupported[];
extern const char kErrorSimLocked[];

}  // namespace networking_private

// Implements the chrome.networkingPrivate.getProperties method.
class NetworkingPrivateGetPropertiesFunction : public AsyncExtensionFunction {
 public:
  NetworkingPrivateGetPropertiesFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getProperties",
                             NETWORKINGPRIVATE_GETPROPERTIES);

 protected:
  ~NetworkingPrivateGetPropertiesFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void Success(std::unique_ptr<base::DictionaryValue> result);
  void Failure(const std::string& error_name);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateGetPropertiesFunction);
};

// Implements the chrome.networkingPrivate.getManagedProperties method.
class NetworkingPrivateGetManagedPropertiesFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateGetManagedPropertiesFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getManagedProperties",
                             NETWORKINGPRIVATE_GETMANAGEDPROPERTIES);

 protected:
  ~NetworkingPrivateGetManagedPropertiesFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void Success(std::unique_ptr<base::DictionaryValue> result);
  void Failure(const std::string& error);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateGetManagedPropertiesFunction);
};

// Implements the chrome.networkingPrivate.getState method.
class NetworkingPrivateGetStateFunction : public AsyncExtensionFunction {
 public:
  NetworkingPrivateGetStateFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getState",
                             NETWORKINGPRIVATE_GETSTATE);

 protected:
  ~NetworkingPrivateGetStateFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void Success(std::unique_ptr<base::DictionaryValue> result);
  void Failure(const std::string& error);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateGetStateFunction);
};

// Implements the chrome.networkingPrivate.setProperties method.
class NetworkingPrivateSetPropertiesFunction : public AsyncExtensionFunction {
 public:
  NetworkingPrivateSetPropertiesFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.setProperties",
                             NETWORKINGPRIVATE_SETPROPERTIES);

 protected:
  ~NetworkingPrivateSetPropertiesFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void Success();
  void Failure(const std::string& error);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateSetPropertiesFunction);
};

// Implements the chrome.networkingPrivate.createNetwork method.
class NetworkingPrivateCreateNetworkFunction : public AsyncExtensionFunction {
 public:
  NetworkingPrivateCreateNetworkFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.createNetwork",
                             NETWORKINGPRIVATE_CREATENETWORK);

 protected:
  ~NetworkingPrivateCreateNetworkFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void Success(const std::string& guid);
  void Failure(const std::string& error);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateCreateNetworkFunction);
};

// Implements the chrome.networkingPrivate.createNetwork method.
class NetworkingPrivateForgetNetworkFunction : public AsyncExtensionFunction {
 public:
  NetworkingPrivateForgetNetworkFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.forgetNetwork",
                             NETWORKINGPRIVATE_FORGETNETWORK);

 protected:
  ~NetworkingPrivateForgetNetworkFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void Success();
  void Failure(const std::string& error);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateForgetNetworkFunction);
};

// Implements the chrome.networkingPrivate.getNetworks method.
class NetworkingPrivateGetNetworksFunction : public AsyncExtensionFunction {
 public:
  NetworkingPrivateGetNetworksFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getNetworks",
                             NETWORKINGPRIVATE_GETNETWORKS);

 protected:
  ~NetworkingPrivateGetNetworksFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void Success(std::unique_ptr<base::ListValue> network_list);
  void Failure(const std::string& error);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateGetNetworksFunction);
};

// Implements the chrome.networkingPrivate.getVisibleNetworks method.
class NetworkingPrivateGetVisibleNetworksFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateGetVisibleNetworksFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getVisibleNetworks",
                             NETWORKINGPRIVATE_GETVISIBLENETWORKS);

 protected:
  ~NetworkingPrivateGetVisibleNetworksFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void Success(std::unique_ptr<base::ListValue> network_list);
  void Failure(const std::string& error);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateGetVisibleNetworksFunction);
};

// Implements the chrome.networkingPrivate.getEnabledNetworkTypes method.
class NetworkingPrivateGetEnabledNetworkTypesFunction
    : public SyncExtensionFunction {
 public:
  NetworkingPrivateGetEnabledNetworkTypesFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getEnabledNetworkTypes",
                             NETWORKINGPRIVATE_GETENABLEDNETWORKTYPES);

 protected:
  ~NetworkingPrivateGetEnabledNetworkTypesFunction() override;

  // SyncExtensionFunction overrides.
  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateGetEnabledNetworkTypesFunction);
};

// Implements the chrome.networkingPrivate.getDeviceStates method.
class NetworkingPrivateGetDeviceStatesFunction : public SyncExtensionFunction {
 public:
  NetworkingPrivateGetDeviceStatesFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getDeviceStates",
                             NETWORKINGPRIVATE_GETDEVICESTATES);

 protected:
  ~NetworkingPrivateGetDeviceStatesFunction() override;

  // SyncExtensionFunction overrides.
  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateGetDeviceStatesFunction);
};

// Implements the chrome.networkingPrivate.enableNetworkType method.
class NetworkingPrivateEnableNetworkTypeFunction
    : public SyncExtensionFunction {
 public:
  NetworkingPrivateEnableNetworkTypeFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.enableNetworkType",
                             NETWORKINGPRIVATE_ENABLENETWORKTYPE);

 protected:
  ~NetworkingPrivateEnableNetworkTypeFunction() override;

  // SyncExtensionFunction overrides.
  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateEnableNetworkTypeFunction);
};

// Implements the chrome.networkingPrivate.disableNetworkType method.
class NetworkingPrivateDisableNetworkTypeFunction
    : public SyncExtensionFunction {
 public:
  NetworkingPrivateDisableNetworkTypeFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.disableNetworkType",
                             NETWORKINGPRIVATE_DISABLENETWORKTYPE);

 protected:
  ~NetworkingPrivateDisableNetworkTypeFunction() override;

  // SyncExtensionFunction overrides.
  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateDisableNetworkTypeFunction);
};

// Implements the chrome.networkingPrivate.requestNetworkScan method.
class NetworkingPrivateRequestNetworkScanFunction
    : public SyncExtensionFunction {
 public:
  NetworkingPrivateRequestNetworkScanFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.requestNetworkScan",
                             NETWORKINGPRIVATE_REQUESTNETWORKSCAN);

 protected:
  ~NetworkingPrivateRequestNetworkScanFunction() override;

  // SyncExtensionFunction overrides.
  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateRequestNetworkScanFunction);
};

// Implements the chrome.networkingPrivate.startConnect method.
class NetworkingPrivateStartConnectFunction : public AsyncExtensionFunction {
 public:
  NetworkingPrivateStartConnectFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.startConnect",
                             NETWORKINGPRIVATE_STARTCONNECT);

 protected:
  ~NetworkingPrivateStartConnectFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void Success();
  void Failure(const std::string& error);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateStartConnectFunction);
};

// Implements the chrome.networkingPrivate.startDisconnect method.
class NetworkingPrivateStartDisconnectFunction : public AsyncExtensionFunction {
 public:
  NetworkingPrivateStartDisconnectFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.startDisconnect",
                             NETWORKINGPRIVATE_STARTDISCONNECT);

 protected:
  ~NetworkingPrivateStartDisconnectFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void Success();
  void Failure(const std::string& error);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateStartDisconnectFunction);
};

// Implements the chrome.networkingPrivate.startActivate method.
class NetworkingPrivateStartActivateFunction : public AsyncExtensionFunction {
 public:
  NetworkingPrivateStartActivateFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.startActivate",
                             NETWORKINGPRIVATE_STARTACTIVATE);

 protected:
  ~NetworkingPrivateStartActivateFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 private:
  void Success();
  void Failure(const std::string& error);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateStartActivateFunction);
};

// Implements the chrome.networkingPrivate.verifyDestination method.
class NetworkingPrivateVerifyDestinationFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateVerifyDestinationFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.verifyDestination",
                             NETWORKINGPRIVATE_VERIFYDESTINATION);

 protected:
  ~NetworkingPrivateVerifyDestinationFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

  void Success(bool result);
  void Failure(const std::string& error);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateVerifyDestinationFunction);
};

// Implements the chrome.networkingPrivate.verifyAndEncryptCredentials method.
class NetworkingPrivateVerifyAndEncryptCredentialsFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateVerifyAndEncryptCredentialsFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.verifyAndEncryptCredentials",
                             NETWORKINGPRIVATE_VERIFYANDENCRYPTCREDENTIALS);

 protected:
  ~NetworkingPrivateVerifyAndEncryptCredentialsFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

  void Success(const std::string& result);
  void Failure(const std::string& error);

 private:
  DISALLOW_COPY_AND_ASSIGN(
      NetworkingPrivateVerifyAndEncryptCredentialsFunction);
};

// Implements the chrome.networkingPrivate.verifyAndEncryptData method.
class NetworkingPrivateVerifyAndEncryptDataFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateVerifyAndEncryptDataFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.verifyAndEncryptData",
                             NETWORKINGPRIVATE_VERIFYANDENCRYPTDATA);

 protected:
  ~NetworkingPrivateVerifyAndEncryptDataFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

  void Success(const std::string& result);
  void Failure(const std::string& error);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateVerifyAndEncryptDataFunction);
};

// Implements the chrome.networkingPrivate.setWifiTDLSEnabledState method.
class NetworkingPrivateSetWifiTDLSEnabledStateFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateSetWifiTDLSEnabledStateFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.setWifiTDLSEnabledState",
                             NETWORKINGPRIVATE_SETWIFITDLSENABLEDSTATE);

 protected:
  ~NetworkingPrivateSetWifiTDLSEnabledStateFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

  void Success(const std::string& result);
  void Failure(const std::string& error);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateSetWifiTDLSEnabledStateFunction);
};

// Implements the chrome.networkingPrivate.getWifiTDLSStatus method.
class NetworkingPrivateGetWifiTDLSStatusFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateGetWifiTDLSStatusFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getWifiTDLSStatus",
                             NETWORKINGPRIVATE_GETWIFITDLSSTATUS);

 protected:
  ~NetworkingPrivateGetWifiTDLSStatusFunction() override;

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

  void Success(const std::string& result);
  void Failure(const std::string& error);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateGetWifiTDLSStatusFunction);
};

class NetworkingPrivateGetCaptivePortalStatusFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateGetCaptivePortalStatusFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getCaptivePortalStatus",
                             NETWORKINGPRIVATE_GETCAPTIVEPORTALSTATUS);

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 protected:
  ~NetworkingPrivateGetCaptivePortalStatusFunction() override;

 private:
  void Success(const std::string& result);
  void Failure(const std::string& error);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateGetCaptivePortalStatusFunction);
};

class NetworkingPrivateUnlockCellularSimFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateUnlockCellularSimFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.unlockCellularSim",
                             NETWORKINGPRIVATE_UNLOCKCELLULARSIM);

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 protected:
  ~NetworkingPrivateUnlockCellularSimFunction() override;

 private:
  void Success();
  void Failure(const std::string& error);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateUnlockCellularSimFunction);
};

class NetworkingPrivateSetCellularSimStateFunction
    : public AsyncExtensionFunction {
 public:
  NetworkingPrivateSetCellularSimStateFunction() {}
  DECLARE_EXTENSION_FUNCTION("networkingPrivate.setCellularSimState",
                             NETWORKINGPRIVATE_SETCELLULARSIMSTATE);

  // AsyncExtensionFunction overrides.
  bool RunAsync() override;

 protected:
  ~NetworkingPrivateSetCellularSimStateFunction() override;

 private:
  void Success();
  void Failure(const std::string& error);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateSetCellularSimStateFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_API_H_
