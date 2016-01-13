// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_INTERFACE_LIST_H_
#define PPAPI_PROXY_INTERFACE_LIST_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT InterfaceList {
 public:
  InterfaceList();
  ~InterfaceList();

  static InterfaceList* GetInstance();

  // Sets the permissions that the interface list will use to compute
  // whether an interface is available to the current process. By default,
  // this will be "no permissions", which will give only access to public
  // stable interfaces via GetInterface.
  //
  // IMPORTANT: This is not a security boundary. Malicious plugins can bypass
  // this check since they run in the same address space as this code in the
  // plugin process. A real security check is required for all IPC messages.
  // This check just allows us to return NULL for interfaces you "shouldn't" be
  // using to keep honest plugins honest.
  static void SetProcessGlobalPermissions(const PpapiPermissions& permissions);

  // Looks up the factory function for the given ID. Returns NULL if not
  // supported.
  InterfaceProxy::Factory GetFactoryForID(ApiID id) const;

  // Returns the interface pointer for the given browser or plugin interface,
  // or NULL if it's not supported.
  const void* GetInterfaceForPPB(const std::string& name);
  const void* GetInterfaceForPPP(const std::string& name) const;

 private:
  friend class InterfaceListTest;

  struct InterfaceInfo {
    InterfaceInfo()
        : iface(NULL),
          required_permission(PERMISSION_NONE),
          interface_logged(false) {
    }
    InterfaceInfo(const void* in_interface, Permission in_perm)
        : iface(in_interface),
          required_permission(in_perm),
          interface_logged(false) {
    }

    const void* iface;

    // Permission required to return non-null for this interface. This will
    // be checked with the value set via SetProcessGlobalPermissionBits when
    // an interface is requested.
    Permission required_permission;

    // Interface usage is logged just once per-interface-per-plugin-process.
    bool interface_logged;
  };

  typedef std::map<std::string, InterfaceInfo> NameToInterfaceInfoMap;

  void AddProxy(ApiID id, InterfaceProxy::Factory factory);

  // Permissions is the type of permission required to access the corresponding
  // interface. Currently this must be just one unique permission (rather than
  // a bitfield).
  void AddPPB(const char* name, const void* iface, Permission permission);
  void AddPPP(const char* name, const void* iface);

  // Hash the interface name for UMA logging.
  static int HashInterfaceName(const std::string& name);

  PpapiPermissions permissions_;

  NameToInterfaceInfoMap name_to_browser_info_;
  NameToInterfaceInfoMap name_to_plugin_info_;

  InterfaceProxy::Factory id_to_factory_[API_ID_COUNT];

  DISALLOW_COPY_AND_ASSIGN(InterfaceList);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_INTERFACE_LIST_H_

