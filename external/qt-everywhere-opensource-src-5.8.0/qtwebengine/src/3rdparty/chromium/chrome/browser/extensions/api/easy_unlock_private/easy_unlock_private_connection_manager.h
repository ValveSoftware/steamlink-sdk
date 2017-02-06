// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CONNECTION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CONNECTION_MANAGER_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_connection.h"
#include "chrome/common/extensions/api/easy_unlock_private.h"
#include "components/proximity_auth/connection.h"
#include "components/proximity_auth/connection_observer.h"
#include "components/proximity_auth/wire_message.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class Extension;

// EasyUnlockPrivateConnectionManager is used by the EasyUnlockPrivateAPI to
// interface with proximity_auth::Connection.
class EasyUnlockPrivateConnectionManager
    : public proximity_auth::ConnectionObserver {
 public:
  explicit EasyUnlockPrivateConnectionManager(content::BrowserContext* context);
  ~EasyUnlockPrivateConnectionManager() override;

  // Stores |connection| in the API connection manager. Returns the
  // |connection_id|.
  int AddConnection(const Extension* extension,
                    std::unique_ptr<proximity_auth::Connection> connection,
                    bool persistent);

  // Returns the status of the connection with |connection_id|.
  api::easy_unlock_private::ConnectionStatus ConnectionStatus(
      const Extension* extension,
      int connection_id) const;

  // Disconnects the connection with |connection_id|. Returns true if
  // |connection_id| is valid.
  bool Disconnect(const Extension* extension, int connection_id);

  // Sends |payload| through the connection with |connection_id|. Returns true
  // if |connection_id| is valid.
  bool SendMessage(const Extension* extension,
                   int connection_id,
                   const std::string& payload);

  // Returns the Bluetooth address of the device connected with a given
  // |connection_id|, and an empty string if |connection_id| was not found.
  std::string GetDeviceAddress(const Extension* extension,
                               int connection_id) const;

  // proximity_auth::ConnectionObserver:
  void OnConnectionStatusChanged(
      proximity_auth::Connection* connection,
      proximity_auth::Connection::Status old_status,
      proximity_auth::Connection::Status new_status) override;
  void OnMessageReceived(const proximity_auth::Connection& connection,
                         const proximity_auth::WireMessage& message) override;
  void OnSendCompleted(const proximity_auth::Connection& connection,
                       const proximity_auth::WireMessage& message,
                       bool success) override;

 private:
  // Dispatches |event_name| with |args| to all listeners. Retrieves the
  // |connection_id| corresponding to the event and rewrite the first argument
  // in |args| with it.
  void DispatchConnectionEvent(const std::string& event_name,
                               events::HistogramValue histogram_value,
                               const proximity_auth::Connection* connection,
                               std::unique_ptr<base::ListValue> args);

  // Convenience method to get the API resource manager.
  ApiResourceManager<EasyUnlockPrivateConnection>* GetResourceManager() const;

  // Convenience method to get the connection with |connection_id| created by
  // extension with |extension_id| from the API resource manager.
  proximity_auth::Connection* GetConnection(const std::string& extension_id,
                                            int connection_id) const;

  // Find the connection_id for |connection| owned by |extension_id| from the
  // API resource manager.
  int FindConnectionId(const std::string& extension_id,
                       const proximity_auth::Connection* connection);

  // BrowserContext passed during initialization.
  content::BrowserContext* browser_context_;

  // The set of extensions that have at least one connection.
  std::set<std::string> extensions_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateConnectionManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CONNECTION_MANAGER_H_
