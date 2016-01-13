// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_PEPPER_CONTAINER_APP_PLUGIN_MODULE_H_
#define MOJO_EXAMPLES_PEPPER_CONTAINER_APP_PLUGIN_MODULE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/native_library.h"
#include "base/scoped_native_library.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppp.h"

namespace ppapi {
class CallbackTracker;
}

namespace mojo {
namespace examples {

class PluginInstance;

class PluginModule : public base::RefCounted<PluginModule> {
 public:
  PluginModule();

  scoped_ptr<PluginInstance> CreateInstance();

  const void* GetPluginInterface(const char* name) const;

  PP_Module pp_module() const { return 1; }
  ppapi::CallbackTracker* callback_tracker() { return callback_tracker_.get(); }

 private:
  friend class base::RefCounted<PluginModule>;

  struct EntryPoints {
    EntryPoints();

    PP_GetInterface_Func get_interface;
    PP_InitializeModule_Func initialize_module;
    PP_ShutdownModule_Func shutdown_module;  // Optional, may be NULL.
  };

  ~PluginModule();

  void Initialize();

  base::ScopedNativeLibrary plugin_module_;
  EntryPoints entry_points_;
  scoped_refptr<ppapi::CallbackTracker> callback_tracker_;

  DISALLOW_COPY_AND_ASSIGN(PluginModule);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_PEPPER_CONTAINER_APP_PLUGIN_MODULE_H_
