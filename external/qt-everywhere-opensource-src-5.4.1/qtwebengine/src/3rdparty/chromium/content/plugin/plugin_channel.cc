// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/plugin/plugin_channel.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/child/npapi/plugin_instance.h"
#include "content/child/npapi/webplugin_delegate_impl.h"
#include "content/child/plugin_messages.h"
#include "content/common/plugin_process_messages.h"
#include "content/plugin/plugin_thread.h"
#include "content/plugin/webplugin_delegate_stub.h"
#include "content/plugin/webplugin_proxy.h"
#include "content/public/common/content_switches.h"
#include "ipc/message_filter.h"
#include "third_party/WebKit/public/web/WebBindings.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

using blink::WebBindings;

namespace content {

namespace {

// How long we wait before releasing the plugin process.
const int kPluginReleaseTimeMinutes = 5;

}  // namespace

// If a sync call to the renderer results in a modal dialog, we need to have a
// way to know so that we can run a nested message loop to simulate what would
// happen in a single process browser and avoid deadlock.
class PluginChannel::MessageFilter : public IPC::MessageFilter {
 public:
  MessageFilter() : sender_(NULL) { }

  base::WaitableEvent* GetModalDialogEvent(int render_view_id) {
    base::AutoLock auto_lock(modal_dialog_event_map_lock_);
    if (!modal_dialog_event_map_.count(render_view_id)) {
      NOTREACHED();
      return NULL;
    }

    return modal_dialog_event_map_[render_view_id].event;
  }

  // Decrement the ref count associated with the modal dialog event for the
  // given tab.
  void ReleaseModalDialogEvent(int render_view_id) {
    base::AutoLock auto_lock(modal_dialog_event_map_lock_);
    if (!modal_dialog_event_map_.count(render_view_id)) {
      NOTREACHED();
      return;
    }

    if (--(modal_dialog_event_map_[render_view_id].refcount))
      return;

    // Delete the event when the stack unwinds as it could be in use now.
    base::MessageLoop::current()->DeleteSoon(
        FROM_HERE, modal_dialog_event_map_[render_view_id].event);
    modal_dialog_event_map_.erase(render_view_id);
  }

  bool Send(IPC::Message* message) {
    // Need this function for the IPC_MESSAGE_HANDLER_DELAY_REPLY macro.
    return sender_->Send(message);
  }

  // IPC::MessageFilter:
  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE {
    sender_ = sender;
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    IPC_BEGIN_MESSAGE_MAP(PluginChannel::MessageFilter, message)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(PluginMsg_Init, OnInit)
      IPC_MESSAGE_HANDLER(PluginMsg_SignalModalDialogEvent,
                          OnSignalModalDialogEvent)
      IPC_MESSAGE_HANDLER(PluginMsg_ResetModalDialogEvent,
                          OnResetModalDialogEvent)
    IPC_END_MESSAGE_MAP()
    return message.type() == PluginMsg_SignalModalDialogEvent::ID ||
           message.type() == PluginMsg_ResetModalDialogEvent::ID;
  }

 protected:
  virtual ~MessageFilter() {
    // Clean up in case of renderer crash.
    for (ModalDialogEventMap::iterator i = modal_dialog_event_map_.begin();
        i != modal_dialog_event_map_.end(); ++i) {
      delete i->second.event;
    }
  }

 private:
  void OnInit(const PluginMsg_Init_Params& params, IPC::Message* reply_msg) {
    base::AutoLock auto_lock(modal_dialog_event_map_lock_);
    if (modal_dialog_event_map_.count(params.host_render_view_routing_id)) {
      modal_dialog_event_map_[params.host_render_view_routing_id].refcount++;
      return;
    }

    WaitableEventWrapper wrapper;
    wrapper.event = new base::WaitableEvent(true, false);
    wrapper.refcount = 1;
    modal_dialog_event_map_[params.host_render_view_routing_id] = wrapper;
  }

  void OnSignalModalDialogEvent(int render_view_id) {
    base::AutoLock auto_lock(modal_dialog_event_map_lock_);
    if (modal_dialog_event_map_.count(render_view_id))
      modal_dialog_event_map_[render_view_id].event->Signal();
  }

  void OnResetModalDialogEvent(int render_view_id) {
    base::AutoLock auto_lock(modal_dialog_event_map_lock_);
    if (modal_dialog_event_map_.count(render_view_id))
      modal_dialog_event_map_[render_view_id].event->Reset();
  }

  struct WaitableEventWrapper {
    base::WaitableEvent* event;
    int refcount;  // There could be multiple plugin instances per tab.
  };
  typedef std::map<int, WaitableEventWrapper> ModalDialogEventMap;
  ModalDialogEventMap modal_dialog_event_map_;
  base::Lock modal_dialog_event_map_lock_;

  IPC::Sender* sender_;
};

PluginChannel* PluginChannel::GetPluginChannel(
    int renderer_id, base::MessageLoopProxy* ipc_message_loop) {
  // Map renderer ID to a (single) channel to that process.
  std::string channel_key = base::StringPrintf(
      "%d.r%d", base::GetCurrentProcId(), renderer_id);

  PluginChannel* channel =
      static_cast<PluginChannel*>(NPChannelBase::GetChannel(
          channel_key,
          IPC::Channel::MODE_SERVER,
          ClassFactory,
          ipc_message_loop,
          false,
          ChildProcess::current()->GetShutDownEvent()));

  if (channel)
    channel->renderer_id_ = renderer_id;

  return channel;
}

// static
void PluginChannel::NotifyRenderersOfPendingShutdown() {
  Broadcast(new PluginHostMsg_PluginShuttingDown());
}

bool PluginChannel::Send(IPC::Message* msg) {
  in_send_++;
  if (log_messages_) {
    VLOG(1) << "sending message @" << msg << " on channel @" << this
            << " with type " << msg->type();
  }
  bool result = NPChannelBase::Send(msg);
  in_send_--;
  return result;
}

bool PluginChannel::OnMessageReceived(const IPC::Message& msg) {
  if (log_messages_) {
    VLOG(1) << "received message @" << &msg << " on channel @" << this
            << " with type " << msg.type();
  }
  return NPChannelBase::OnMessageReceived(msg);
}

void PluginChannel::OnChannelError() {
  NPChannelBase::OnChannelError();
  CleanUp();
}

int PluginChannel::GenerateRouteID() {
  static int last_id = 0;
  return ++last_id;
}

base::WaitableEvent* PluginChannel::GetModalDialogEvent(int render_view_id) {
  return filter_->GetModalDialogEvent(render_view_id);
}

PluginChannel::~PluginChannel() {
  PluginThread::current()->Send(new PluginProcessHostMsg_ChannelDestroyed(
      renderer_id_));
  process_ref_.ReleaseWithDelay(
      base::TimeDelta::FromMinutes(kPluginReleaseTimeMinutes));
}

void PluginChannel::CleanUp() {
  // We need to clean up the stubs so that they call NPPDestroy.  This will
  // also lead to them releasing their reference on this object so that it can
  // be deleted.
  for (size_t i = 0; i < plugin_stubs_.size(); ++i)
    RemoveRoute(plugin_stubs_[i]->instance_id());

  // Need to addref this object temporarily because otherwise removing the last
  // stub will cause the destructor of this object to be called, however at
  // that point plugin_stubs_ will have one element and its destructor will be
  // called twice.
  scoped_refptr<PluginChannel> me(this);

  while (!plugin_stubs_.empty()) {
    // Separate vector::erase and ~WebPluginDelegateStub.
    // See https://code.google.com/p/chromium/issues/detail?id=314088
    scoped_refptr<WebPluginDelegateStub> stub = plugin_stubs_[0];
    plugin_stubs_.erase(plugin_stubs_.begin());
  }
}

bool PluginChannel::Init(base::MessageLoopProxy* ipc_message_loop,
                         bool create_pipe_now,
                         base::WaitableEvent* shutdown_event) {
  if (!NPChannelBase::Init(ipc_message_loop, create_pipe_now, shutdown_event))
    return false;

  channel_->AddFilter(filter_.get());
  return true;
}

PluginChannel::PluginChannel()
    : renderer_id_(-1),
      in_send_(0),
      incognito_(false),
      filter_(new MessageFilter()),
      npp_(new struct _NPP) {
  set_send_unblocking_only_during_unblock_dispatch();
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  log_messages_ = command_line->HasSwitch(switches::kLogPluginMessages);

  // Register |npp_| as the default owner for any object we receive via IPC,
  // and register it with WebBindings as a valid owner.
  SetDefaultNPObjectOwner(npp_.get());
  WebBindings::registerObjectOwner(npp_.get());
}

bool PluginChannel::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginChannel, msg)
    IPC_MESSAGE_HANDLER(PluginMsg_CreateInstance, OnCreateInstance)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PluginMsg_DestroyInstance,
                                    OnDestroyInstance)
    IPC_MESSAGE_HANDLER(PluginMsg_GenerateRouteID, OnGenerateRouteID)
    IPC_MESSAGE_HANDLER(PluginProcessMsg_ClearSiteData, OnClearSiteData)
    IPC_MESSAGE_HANDLER(PluginHostMsg_DidAbortLoading, OnDidAbortLoading)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void PluginChannel::OnCreateInstance(const std::string& mime_type,
                                     int* instance_id) {
  *instance_id = GenerateRouteID();
  scoped_refptr<WebPluginDelegateStub> stub(new WebPluginDelegateStub(
      mime_type, *instance_id, this));
  AddRoute(*instance_id, stub.get(), NULL);
  plugin_stubs_.push_back(stub);
}

void PluginChannel::OnDestroyInstance(int instance_id,
                                      IPC::Message* reply_msg) {
  for (size_t i = 0; i < plugin_stubs_.size(); ++i) {
    if (plugin_stubs_[i]->instance_id() == instance_id) {
      scoped_refptr<MessageFilter> filter(filter_);
      int render_view_id =
          plugin_stubs_[i]->webplugin()->host_render_view_routing_id();
      // Separate vector::erase and ~WebPluginDelegateStub.
      // See https://code.google.com/p/chromium/issues/detail?id=314088
      scoped_refptr<WebPluginDelegateStub> stub = plugin_stubs_[i];
      plugin_stubs_.erase(plugin_stubs_.begin() + i);
      stub = NULL;

      Send(reply_msg);
      RemoveRoute(instance_id);
      // NOTE: *this* might be deleted as a result of calling RemoveRoute.
      // Don't release the modal dialog event right away, but do it after the
      // stack unwinds since the plugin can be destroyed later if it's in use
      // right now.
      base::MessageLoop::current()->PostNonNestableTask(
          FROM_HERE,
          base::Bind(&MessageFilter::ReleaseModalDialogEvent,
                     filter.get(),
                     render_view_id));
      return;
    }
  }

  NOTREACHED() << "Couldn't find WebPluginDelegateStub to destroy";
}

void PluginChannel::OnGenerateRouteID(int* route_id) {
  *route_id = GenerateRouteID();
}

void PluginChannel::OnClearSiteData(const std::string& site,
                                    uint64 flags,
                                    uint64 max_age) {
  bool success = false;
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  base::FilePath path = command_line->GetSwitchValuePath(switches::kPluginPath);
  scoped_refptr<PluginLib> plugin_lib(PluginLib::CreatePluginLib(path));
  if (plugin_lib.get()) {
    NPError err = plugin_lib->NP_Initialize();
    if (err == NPERR_NO_ERROR) {
      const char* site_str = site.empty() ? NULL : site.c_str();
      err = plugin_lib->NP_ClearSiteData(site_str, flags, max_age);
      std::string site_name =
          site.empty() ? "NULL"
                       : base::StringPrintf("\"%s\"", site_str);
      VLOG(1) << "NPP_ClearSiteData(" << site_name << ", " << flags << ", "
              << max_age << ") returned " << err;
      success = (err == NPERR_NO_ERROR);
    }
  }
  Send(new PluginProcessHostMsg_ClearSiteDataResult(success));
}

void PluginChannel::OnDidAbortLoading(int render_view_id) {
  for (size_t i = 0; i < plugin_stubs_.size(); ++i) {
    if (plugin_stubs_[i]->webplugin()->host_render_view_routing_id() ==
            render_view_id) {
      plugin_stubs_[i]->delegate()->instance()->CloseStreams();
    }
  }
}

}  // namespace content
