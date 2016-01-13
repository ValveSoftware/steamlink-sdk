// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_PEPPER_CONTAINER_APP_MOJO_PPAPI_GLOBALS_H_
#define MOJO_EXAMPLES_PEPPER_CONTAINER_APP_MOJO_PPAPI_GLOBALS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/system/core.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace base {
class MessageLoopProxy;
}

namespace mojo {
namespace examples {

class PluginInstance;

class MojoPpapiGlobals : public ppapi::PpapiGlobals {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual ScopedMessagePipeHandle CreateGLES2Context() = 0;
  };

  // |delegate| must live longer than this object.
  explicit MojoPpapiGlobals(Delegate* delegate);
  virtual ~MojoPpapiGlobals();

  inline static MojoPpapiGlobals* Get() {
    return static_cast<MojoPpapiGlobals*>(PpapiGlobals::Get());
  }

  PP_Instance AddInstance(PluginInstance* instance);
  void InstanceDeleted(PP_Instance instance);
  PluginInstance* GetInstance(PP_Instance instance);

  ScopedMessagePipeHandle CreateGLES2Context();

  // ppapi::PpapiGlobals implementation.
  virtual ppapi::ResourceTracker* GetResourceTracker() OVERRIDE;
  virtual ppapi::VarTracker* GetVarTracker() OVERRIDE;
  virtual ppapi::CallbackTracker* GetCallbackTrackerForInstance(
      PP_Instance instance) OVERRIDE;
  virtual void LogWithSource(PP_Instance instance,
                             PP_LogLevel level,
                             const std::string& source,
                             const std::string& value) OVERRIDE;
  virtual void BroadcastLogWithSource(PP_Module module,
                                      PP_LogLevel level,
                                      const std::string& source,
                                      const std::string& value) OVERRIDE;
  virtual ppapi::thunk::PPB_Instance_API* GetInstanceAPI(
      PP_Instance instance) OVERRIDE;
  virtual ppapi::thunk::ResourceCreationAPI* GetResourceCreationAPI(
      PP_Instance instance) OVERRIDE;
  virtual PP_Module GetModuleForInstance(PP_Instance instance) OVERRIDE;
  virtual ppapi::MessageLoopShared* GetCurrentMessageLoop() OVERRIDE;
  virtual base::TaskRunner* GetFileTaskRunner() OVERRIDE;
  virtual std::string GetCmdLine() OVERRIDE;
  virtual void PreCacheFontForFlash(const void* logfontw) OVERRIDE;

 private:
  class MainThreadMessageLoopResource;

  // Non-owning pointer.
  Delegate* const delegate_;

  // Non-owning pointer.
  PluginInstance* plugin_instance_;

  ppapi::ResourceTracker resource_tracker_;

  scoped_refptr<MainThreadMessageLoopResource>
      main_thread_message_loop_resource_;

  DISALLOW_COPY_AND_ASSIGN(MojoPpapiGlobals);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_PEPPER_CONTAINER_APP_MOJO_PPAPI_GLOBALS_H_
