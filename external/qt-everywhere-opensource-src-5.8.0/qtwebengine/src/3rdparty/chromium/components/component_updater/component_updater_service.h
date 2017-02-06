// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_
#define COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/version.h"
#include "components/update_client/update_client.h"
#include "url/gurl.h"

class ComponentsUI;
class SupervisedUserWhitelistService;

namespace base {
class DictionaryValue;
class FilePath;
class SequencedTaskRunner;
}

namespace content {
class ResourceThrottle;
}

namespace net {
class URLRequestContextGetter;
class URLRequest;
}

namespace update_client {
class ComponentInstaller;
class Configurator;
struct CrxComponent;
struct CrxUpdateItem;
}

namespace component_updater {

class OnDemandUpdater;

using Configurator = update_client::Configurator;
using CrxComponent = update_client::CrxComponent;
using CrxUpdateItem = update_client::CrxUpdateItem;

// The component update service is in charge of installing or upgrading
// select parts of chrome. Each part is called a component and managed by
// instances of CrxComponent registered using RegisterComponent(). On the
// server, each component is packaged as a CRX which is the same format used
// to package extensions. To the update service each component is identified
// by its public key hash (CrxComponent::pk_hash). If there is an update
// available and its version is bigger than (CrxComponent::version), it will
// be downloaded, verified and unpacked. Then component-specific installer
// ComponentInstaller::Install (of CrxComponent::installer) will be called.
//
// During the normal operation of the component updater some specific
// notifications are fired, like COMPONENT_UPDATER_STARTED and
// COMPONENT_UPDATE_FOUND. See notification_type.h for more details.
//
// All methods are safe to call ONLY from the browser's main thread.
class ComponentUpdateService {
 public:
  using Observer = update_client::UpdateClient::Observer;

  // Adds an observer for this class. An observer should not be added more
  // than once. The caller retains the ownership of the observer object.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer. It is safe for an observer to be removed while
  // the observers are being notified.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Add component to be checked for updates.
  virtual bool RegisterComponent(const CrxComponent& component) = 0;

  // Unregisters the component with the given ID. This means that the component
  // is not going to be included in future update checks. If a download or
  // update operation for the component is currently in progress, it will
  // silently finish without triggering the next step.
  // Note that the installer for the component is responsible for removing any
  // existing versions of the component from disk. Returns true if the
  // uninstall has completed successfully and the component files have been
  // removed, or if the uninstalled has been deferred because the component
  // is being updated. Returns false if the component id is not known or the
  /// uninstall encountered an error.
  virtual bool UnregisterComponent(const std::string& id) = 0;

  // Returns a list of registered components.
  virtual std::vector<std::string> GetComponentIDs() const = 0;

  // Returns an interface for on-demand updates. On-demand updates are
  // proactively triggered outside the normal component update service schedule.
  virtual OnDemandUpdater& GetOnDemandUpdater() = 0;

  // This method is used to trigger an on-demand update for component |id|.
  // This can be used when loading a resource that depends on this component.
  //
  // |callback| is called on the main thread once the on-demand update is
  // complete, regardless of success. |callback| may be called immediately
  // within the method body.
  //
  // Additionally, this function implements an embedder-defined cooldown
  // interval between on demand update attempts. This behavior is intended
  // to be defensive against programming bugs, usually triggered by web fetches,
  // where the on-demand functionality is invoked too often. If this function
  // is called while still on cooldown, |callback| will be called immediately.
  virtual void MaybeThrottle(const std::string& id,
                             const base::Closure& callback) = 0;

  // Returns a task runner suitable for use by component installers.
  virtual scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner() = 0;

  virtual ~ComponentUpdateService() {}

 private:
  // Returns details about registered component in the |item| parameter. The
  // function returns true in case of success and false in case of errors.
  virtual bool GetComponentDetails(const std::string& id,
                                   CrxUpdateItem* item) const = 0;

  friend class ::ComponentsUI;
  FRIEND_TEST_ALL_PREFIXES(DefaultComponentInstallerTest, RegisterComponent);
};

using ServiceObserver = ComponentUpdateService::Observer;

class OnDemandUpdater {
 public:
  virtual ~OnDemandUpdater() {}

 private:
  friend class OnDemandTester;
  friend class SupervisedUserWhitelistInstaller;
  friend class ::ComponentsUI;

  // Triggers an update check for a component. |id| is a value
  // returned by GetCrxComponentID(). If an update for this component is already
  // in progress, the function returns |kInProgress|. If an update is available,
  // the update will be applied. The caller can subscribe to component update
  // service notifications to get an indication about the outcome of the
  // on-demand update. The function does not implement any cooldown interval.
  // TODO(sorin): improve this API so that the result of this non-blocking
  // call is provided by a callback.
  virtual bool OnDemandUpdate(const std::string& id) = 0;
};

// Creates the component updater.
std::unique_ptr<ComponentUpdateService> ComponentUpdateServiceFactory(
    const scoped_refptr<Configurator>& config);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_
