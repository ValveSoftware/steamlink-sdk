// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/pepper_container_app/mojo_ppapi_globals.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/time/time.h"
#include "mojo/examples/pepper_container_app/plugin_instance.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppb_message_loop_shared.h"

namespace mojo {
namespace examples {

namespace {

const PP_Instance kInstanceId = 1;

}  // namespace

// A non-abstract subclass of ppapi::MessageLoopShared that represents the
// message loop of the main thread.
// TODO(yzshen): Build a more general ppapi::MessageLoopShared subclass to fully
// support PPB_MessageLoop.
class MojoPpapiGlobals::MainThreadMessageLoopResource
    : public ppapi::MessageLoopShared {
 public:
  explicit MainThreadMessageLoopResource(
      base::MessageLoopProxy* main_thread_message_loop)
      : MessageLoopShared(ForMainThread()),
        main_thread_message_loop_(main_thread_message_loop) {}

  // ppapi::MessageLoopShared implementation.
  virtual void PostClosure(const tracked_objects::Location& from_here,
                           const base::Closure& closure,
                           int64 delay_ms) OVERRIDE {
    main_thread_message_loop_->PostDelayedTask(
        from_here, closure, base::TimeDelta::FromMilliseconds(delay_ms));
  }

  virtual base::MessageLoopProxy* GetMessageLoopProxy() OVERRIDE {
    return main_thread_message_loop_.get();
  }

  // ppapi::thunk::PPB_MessageLoop_API implementation.
  virtual int32_t AttachToCurrentThread() OVERRIDE {
    NOTIMPLEMENTED();
    return PP_ERROR_FAILED;
  }

  virtual int32_t Run() OVERRIDE {
    NOTIMPLEMENTED();
    return PP_ERROR_FAILED;
  }

  virtual int32_t PostWork(PP_CompletionCallback callback,
                           int64_t delay_ms) OVERRIDE {
    NOTIMPLEMENTED();
    return PP_ERROR_FAILED;
  }

  virtual int32_t PostQuit(PP_Bool should_destroy) OVERRIDE {
    NOTIMPLEMENTED();
    return PP_ERROR_FAILED;
  }

 private:
  virtual ~MainThreadMessageLoopResource() {}

  scoped_refptr<base::MessageLoopProxy> main_thread_message_loop_;
  DISALLOW_COPY_AND_ASSIGN(MainThreadMessageLoopResource);
};

MojoPpapiGlobals::MojoPpapiGlobals(Delegate* delegate)
    : delegate_(delegate),
      plugin_instance_(NULL),
      resource_tracker_(ppapi::ResourceTracker::THREAD_SAFE) {}

MojoPpapiGlobals::~MojoPpapiGlobals() {}

PP_Instance MojoPpapiGlobals::AddInstance(PluginInstance* instance) {
  DCHECK(!plugin_instance_);
  plugin_instance_ = instance;
  resource_tracker_.DidCreateInstance(kInstanceId);
  return kInstanceId;
}

void MojoPpapiGlobals::InstanceDeleted(PP_Instance instance) {
  DCHECK_EQ(instance, kInstanceId);
  DCHECK(plugin_instance_);
  resource_tracker_.DidDeleteInstance(instance);
  plugin_instance_ = NULL;
}

PluginInstance* MojoPpapiGlobals::GetInstance(PP_Instance instance) {
  if (instance == kInstanceId)
    return plugin_instance_;
  return NULL;
}

ScopedMessagePipeHandle MojoPpapiGlobals::CreateGLES2Context() {
  return delegate_->CreateGLES2Context();
}

ppapi::ResourceTracker* MojoPpapiGlobals::GetResourceTracker() {
  return &resource_tracker_;
}

ppapi::VarTracker* MojoPpapiGlobals::GetVarTracker() {
  NOTIMPLEMENTED();
  return NULL;
}

ppapi::CallbackTracker* MojoPpapiGlobals::GetCallbackTrackerForInstance(
    PP_Instance instance) {
  if (instance == kInstanceId && plugin_instance_)
    return plugin_instance_->plugin_module()->callback_tracker();
  return NULL;
}

void MojoPpapiGlobals::LogWithSource(PP_Instance instance,
                                     PP_LogLevel level,
                                     const std::string& source,
                                     const std::string& value) {
  NOTIMPLEMENTED();
}

void MojoPpapiGlobals::BroadcastLogWithSource(PP_Module module,
                                              PP_LogLevel level,
                                              const std::string& source,
                                              const std::string& value) {
  NOTIMPLEMENTED();
}

ppapi::thunk::PPB_Instance_API* MojoPpapiGlobals::GetInstanceAPI(
    PP_Instance instance) {
  if (instance == kInstanceId && plugin_instance_)
    return plugin_instance_;
  return NULL;
}

ppapi::thunk::ResourceCreationAPI* MojoPpapiGlobals::GetResourceCreationAPI(
    PP_Instance instance) {
  if (instance == kInstanceId && plugin_instance_)
    return plugin_instance_->resource_creation();
  return NULL;
}

PP_Module MojoPpapiGlobals::GetModuleForInstance(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

ppapi::MessageLoopShared* MojoPpapiGlobals::GetCurrentMessageLoop() {
  if (base::MessageLoopProxy::current().get() == GetMainThreadMessageLoop()) {
    if (!main_thread_message_loop_resource_) {
      main_thread_message_loop_resource_ = new MainThreadMessageLoopResource(
          GetMainThreadMessageLoop());
    }
    return main_thread_message_loop_resource_.get();
  }

  NOTIMPLEMENTED();
  return NULL;
}

base::TaskRunner* MojoPpapiGlobals::GetFileTaskRunner() {
  NOTIMPLEMENTED();
  return NULL;
}

std::string MojoPpapiGlobals::GetCmdLine() {
  NOTIMPLEMENTED();
  return std::string();
}

void MojoPpapiGlobals::PreCacheFontForFlash(const void* logfontw) {
  NOTIMPLEMENTED();
}

}  // namespace examples
}  // namespace mojo
