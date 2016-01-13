// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_DBUS_SERVICE_LOADER_H_
#define MOJO_SHELL_DBUS_SERVICE_LOADER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/shell/keep_alive.h"
#include "url/gurl.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace mojo {
namespace shell {

class Context;

// An implementation of ServiceLoader that contacts a system service
// and bootstraps a Mojo connection to it over DBus.
//
// In order to allow the externally-running service to accept connections from
// a Mojo shell, we need to get it a ShellHandle. This class creates a
// dedicated MessagePipe, passes a handle to one end to the desired service
// over DBus, and then passes the ShellHandle over that pipe.
//
// This class assumes the following:
// 1) Your service is already running.
// 2) Your service implements the Mojo ExternalService API
//    (from external_service.mojom).
// 3) Your service exports an object that implements the org.chromium.Mojo DBus
//    interface:
//    <interface name="org.chromium.Mojo">
//      <method name="ConnectChannel">
//        <arg type="h" name="file_descriptor" direction="in" />
//      </method>
//    </interface>
class DBusServiceLoader : public ServiceLoader {
 public:
  DBusServiceLoader(Context* context);
  virtual ~DBusServiceLoader();

  // URL for DBus services are of the following format:
  // dbus:tld.domain.ServiceName/path/to/DBusObject
  //
  // This is simply the scheme (dbus:) and then the DBus service name followed
  // by the DBus object path of an object that implements the org.chromium.Mojo
  // interface as discussed above.
  //
  // Example:
  //   dbus:org.chromium.EchoService/org/chromium/MojoImpl
  //
  // This will tell DBusServiceLoader to reach out to a service with
  // the name "org.chromium.EchoService" and invoke the method
  // "org.chromium.Mojo.ConnectChannel" on the object exported at
  // "/org/chromium/MojoImpl".
  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedMessagePipeHandle service_handle) OVERRIDE;

  virtual void OnServiceError(ServiceManager* manager, const GURL& url)
      OVERRIDE;

 private:
  class LoadContext;

  // Tosses out connection-related state to service at given URL.
  void ForgetService(const GURL& url);

  Context* const context_;
  scoped_refptr<dbus::Bus> bus_;

  typedef std::map<GURL, LoadContext*> LoadContextMap;
  LoadContextMap url_to_load_context_;

  DISALLOW_COPY_AND_ASSIGN(DBusServiceLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_DBUS_SERVICE_LOADER_H_
