// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/pepper_container_app/interface_list.h"

#include "base/memory/singleton.h"
#include "mojo/examples/pepper_container_app/thunk.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/c/ppb_view.h"
#include "ppapi/thunk/thunk.h"

namespace mojo {
namespace examples {

InterfaceList::InterfaceList() {
  browser_interfaces_[PPB_CORE_INTERFACE_1_0] = GetPPB_Core_1_0_Thunk();
  browser_interfaces_[PPB_GRAPHICS_3D_INTERFACE_1_0] =
      ppapi::thunk::GetPPB_Graphics3D_1_0_Thunk();
  browser_interfaces_[PPB_OPENGLES2_INTERFACE_1_0] =
      GetPPB_OpenGLES2_Thunk();
  browser_interfaces_[PPB_INSTANCE_INTERFACE_1_0] =
      ppapi::thunk::GetPPB_Instance_1_0_Thunk();
  browser_interfaces_[PPB_VIEW_INTERFACE_1_0] =
      ppapi::thunk::GetPPB_View_1_0_Thunk();
  browser_interfaces_[PPB_VIEW_INTERFACE_1_1] =
      ppapi::thunk::GetPPB_View_1_1_Thunk();
}

InterfaceList::~InterfaceList() {}

// static
InterfaceList* InterfaceList::GetInstance() {
  return Singleton<InterfaceList>::get();
}

const void* InterfaceList::GetBrowserInterface(const std::string& name) const {
  NameToInterfaceMap::const_iterator iter = browser_interfaces_.find(name);

  if (iter == browser_interfaces_.end())
    return NULL;

  return iter->second;
}

}  // namespace examples
}  // namespace mojo
