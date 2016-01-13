// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/pepper_container_app/plugin_module.h"

#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "mojo/examples/pepper_container_app/interface_list.h"
#include "mojo/examples/pepper_container_app/plugin_instance.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/callback_tracker.h"

namespace mojo {
namespace examples {

namespace {

const void* GetInterface(const char* name) {
  const void* interface =
      InterfaceList::GetInstance()->GetBrowserInterface(name);

  if (!interface)
    LOG(WARNING) << "Interface requested " << name;

  return interface;
}

}  // namespace

PluginModule::EntryPoints::EntryPoints() : get_interface(NULL),
                                           initialize_module(NULL),
                                           shutdown_module(NULL) {}

PluginModule::PluginModule() : callback_tracker_(new ppapi::CallbackTracker) {
  Initialize();
}

PluginModule::~PluginModule() {
  callback_tracker_->AbortAll();

  if (entry_points_.shutdown_module)
    entry_points_.shutdown_module();
}

scoped_ptr<PluginInstance> PluginModule::CreateInstance() {
  return make_scoped_ptr(new PluginInstance(this));
}

const void* PluginModule::GetPluginInterface(const char* name) const {
  if (entry_points_.get_interface)
    return entry_points_.get_interface(name);
  return NULL;
}

void PluginModule::Initialize() {
  // Platform-specific filename.
  // TODO(yzshen): Don't hard-code it.
#if defined(OS_WIN)
  static const wchar_t plugin_name[] = L"ppapi_example_gles2_spinning_cube.dll";
#elif defined(OS_MACOSX)
  static const char plugin_name[] = "ppapi_example_gles2_spinning_cube.plugin";
#elif defined(OS_POSIX)
  static const char plugin_name[] = "libppapi_example_gles2_spinning_cube.so";
#endif

  base::FilePath plugin_path(plugin_name);

  base::NativeLibraryLoadError error;
  plugin_module_.Reset(base::LoadNativeLibrary(plugin_path, &error));

  if (!plugin_module_.is_valid()) {
    LOG(WARNING) << "Cannot load " << plugin_path.AsUTF8Unsafe()
                 << ". Error: " << error.ToString();
    return;
  }

  entry_points_.get_interface =
      reinterpret_cast<PP_GetInterface_Func>(
          plugin_module_.GetFunctionPointer("PPP_GetInterface"));
  if (!entry_points_.get_interface) {
    LOG(WARNING) << "No PPP_GetInterface in plugin library";
    return;
  }

  entry_points_.initialize_module =
      reinterpret_cast<PP_InitializeModule_Func>(
          plugin_module_.GetFunctionPointer("PPP_InitializeModule"));
  if (!entry_points_.initialize_module) {
    LOG(WARNING) << "No PPP_InitializeModule in plugin library";
    return;
  }

  // It's okay for PPP_ShutdownModule to not be defined and |shutdown_module| to
  // be NULL.
  entry_points_.shutdown_module =
      reinterpret_cast<PP_ShutdownModule_Func>(
          plugin_module_.GetFunctionPointer("PPP_ShutdownModule"));

  int32_t result = entry_points_.initialize_module(pp_module(),
                                                   &GetInterface);
  if (result != PP_OK)
    LOG(WARNING) << "Initializing module failed with error " << result;
}

}  // namespace examples
}  // namespace mojo
