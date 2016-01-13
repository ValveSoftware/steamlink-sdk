// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_message_loop_proxy.h"

#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_message_loop.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/thunk/enter.h"

using ppapi::thunk::PPB_MessageLoop_API;

namespace ppapi {
namespace proxy {

namespace {
typedef thunk::EnterResource<PPB_MessageLoop_API> EnterMessageLoop;
}

MessageLoopResource::MessageLoopResource(PP_Instance instance)
    : MessageLoopShared(instance),
      nested_invocations_(0),
      destroyed_(false),
      should_destroy_(false),
      is_main_thread_loop_(false) {
}

MessageLoopResource::MessageLoopResource(ForMainThread for_main_thread)
    : MessageLoopShared(for_main_thread),
      nested_invocations_(0),
      destroyed_(false),
      should_destroy_(false),
      is_main_thread_loop_(true) {
  // We attach the main thread immediately. We can't use AttachToCurrentThread,
  // because the MessageLoop already exists.

  // This must be called only once, so the slot must be empty.
  CHECK(!PluginGlobals::Get()->msg_loop_slot());
  // We don't add a reference for TLS here, so we don't release it. Instead,
  // this loop is owned by PluginGlobals. Contrast with AttachToCurrentThread
  // where we register ReleaseMessageLoop with TLS and call AddRef.
  base::ThreadLocalStorage::Slot* slot = new base::ThreadLocalStorage::Slot();
  PluginGlobals::Get()->set_msg_loop_slot(slot);

  slot->Set(this);

  loop_proxy_ = base::MessageLoopProxy::current();
}


MessageLoopResource::~MessageLoopResource() {
}

PPB_MessageLoop_API* MessageLoopResource::AsPPB_MessageLoop_API() {
  return this;
}

int32_t MessageLoopResource::AttachToCurrentThread() {
  if (is_main_thread_loop_)
    return PP_ERROR_INPROGRESS;

  PluginGlobals* globals = PluginGlobals::Get();

  base::ThreadLocalStorage::Slot* slot = globals->msg_loop_slot();
  if (!slot) {
    slot = new base::ThreadLocalStorage::Slot(&ReleaseMessageLoop);
    globals->set_msg_loop_slot(slot);
  } else {
    if (slot->Get())
      return PP_ERROR_INPROGRESS;
  }
  // TODO(dmichael) check that the current thread can support a message loop.

  // Take a ref to the MessageLoop on behalf of the TLS. Note that this is an
  // internal ref and not a plugin ref so the plugin can't accidentally
  // release it. This is released by ReleaseMessageLoop().
  AddRef();
  slot->Set(this);

  loop_.reset(new base::MessageLoop);
  loop_proxy_ = base::MessageLoopProxy::current();

  // Post all pending work to the message loop.
  for (size_t i = 0; i < pending_tasks_.size(); i++) {
    const TaskInfo& info = pending_tasks_[i];
    PostClosure(info.from_here, info.closure, info.delay_ms);
  }
  pending_tasks_.clear();

  return PP_OK;
}

int32_t MessageLoopResource::Run() {
  if (!IsCurrent())
    return PP_ERROR_WRONG_THREAD;
  if (is_main_thread_loop_)
    return PP_ERROR_INPROGRESS;

  nested_invocations_++;
  CallWhileUnlocked(
      base::Bind(&base::MessageLoop::Run, base::Unretained(loop_.get())));
  nested_invocations_--;

  if (should_destroy_ && nested_invocations_ == 0) {
    loop_proxy_ = NULL;
    loop_.reset();
    destroyed_ = true;
  }
  return PP_OK;
}

int32_t MessageLoopResource::PostWork(PP_CompletionCallback callback,
                                      int64_t delay_ms) {
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;
  if (destroyed_)
    return PP_ERROR_FAILED;
  PostClosure(FROM_HERE,
              base::Bind(callback.func, callback.user_data,
                         static_cast<int32_t>(PP_OK)),
              delay_ms);
  return PP_OK;
}

int32_t MessageLoopResource::PostQuit(PP_Bool should_destroy) {
  if (is_main_thread_loop_)
    return PP_ERROR_WRONG_THREAD;

  if (PP_ToBool(should_destroy))
    should_destroy_ = true;

  if (IsCurrent() && nested_invocations_ > 0)
    loop_->Quit();
  else
    PostClosure(FROM_HERE, base::MessageLoop::QuitClosure(), 0);
  return PP_OK;
}

// static
MessageLoopResource* MessageLoopResource::GetCurrent() {
  PluginGlobals* globals = PluginGlobals::Get();
  if (!globals->msg_loop_slot())
    return NULL;
  return reinterpret_cast<MessageLoopResource*>(
      globals->msg_loop_slot()->Get());
}

void MessageLoopResource::DetachFromThread() {
  // Note that the message loop must be destroyed on the thread it was created
  // on.
  loop_proxy_ = NULL;
  loop_.reset();

  // Cancel out the AddRef in AttachToCurrentThread().
  Release();
  // DANGER: may delete this.
}

bool MessageLoopResource::IsCurrent() const {
  PluginGlobals* globals = PluginGlobals::Get();
  if (!globals->msg_loop_slot())
    return false;  // Can't be current if there's nothing in the slot.
  return static_cast<const void*>(globals->msg_loop_slot()->Get()) ==
         static_cast<const void*>(this);
}

void MessageLoopResource::PostClosure(
    const tracked_objects::Location& from_here,
    const base::Closure& closure,
    int64 delay_ms) {
  if (loop_proxy_.get()) {
    loop_proxy_->PostDelayedTask(
        from_here, closure, base::TimeDelta::FromMilliseconds(delay_ms));
  } else {
    TaskInfo info;
    info.from_here = FROM_HERE;
    info.closure = closure;
    info.delay_ms = delay_ms;
    pending_tasks_.push_back(info);
  }
}

base::MessageLoopProxy* MessageLoopResource::GetMessageLoopProxy() {
  return loop_proxy_.get();
}

// static
void MessageLoopResource::ReleaseMessageLoop(void* value) {
  static_cast<MessageLoopResource*>(value)->DetachFromThread();
}

// -----------------------------------------------------------------------------

PP_Resource Create(PP_Instance instance) {
  ProxyAutoLock lock;
  // Validate the instance.
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;
  return (new MessageLoopResource(instance))->GetReference();
}

PP_Resource GetForMainThread() {
  ProxyAutoLock lock;
  return PluginGlobals::Get()->loop_for_main_thread()->GetReference();
}

PP_Resource GetCurrent() {
  ProxyAutoLock lock;
  Resource* resource = MessageLoopResource::GetCurrent();
  if (resource)
    return resource->GetReference();
  return 0;
}

int32_t AttachToCurrentThread(PP_Resource message_loop) {
  EnterMessageLoop enter(message_loop, true);
  if (enter.succeeded())
    return enter.object()->AttachToCurrentThread();
  return PP_ERROR_BADRESOURCE;
}

int32_t Run(PP_Resource message_loop) {
  EnterMessageLoop enter(message_loop, true);
  if (enter.succeeded())
    return enter.object()->Run();
  return PP_ERROR_BADRESOURCE;
}

int32_t PostWork(PP_Resource message_loop,
                 PP_CompletionCallback callback,
                 int64_t delay_ms) {
  EnterMessageLoop enter(message_loop, true);
  if (enter.succeeded())
    return enter.object()->PostWork(callback, delay_ms);
  return PP_ERROR_BADRESOURCE;
}

int32_t PostQuit(PP_Resource message_loop, PP_Bool should_destroy) {
  EnterMessageLoop enter(message_loop, true);
  if (enter.succeeded())
    return enter.object()->PostQuit(should_destroy);
  return PP_ERROR_BADRESOURCE;
}

const PPB_MessageLoop_1_0 ppb_message_loop_interface = {
  &Create,
  &GetForMainThread,
  &GetCurrent,
  &AttachToCurrentThread,
  &Run,
  &PostWork,
  &PostQuit
};

PPB_MessageLoop_Proxy::PPB_MessageLoop_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_MessageLoop_Proxy::~PPB_MessageLoop_Proxy() {
}

// static
const PPB_MessageLoop_1_0* PPB_MessageLoop_Proxy::GetInterface() {
  return &ppb_message_loop_interface;
}

}  // namespace proxy
}  // namespace ppapi
