// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/np_channel_base.h"

#include "base/auto_reset.h"
#include "base/containers/hash_tables.h"
#include "base/files/scoped_file.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_local.h"
#include "ipc/ipc_sync_message.h"

#if defined(OS_POSIX)
#include "base/file_util.h"
#include "ipc/ipc_channel_posix.h"
#endif

namespace content {

namespace {

typedef base::hash_map<std::string, scoped_refptr<NPChannelBase> > ChannelMap;

struct ChannelGlobals {
  ChannelMap channel_map;
  scoped_refptr<NPChannelBase> current_channel;
};

#if defined(OS_ANDROID)
// Workaround for http://crbug.com/298179 - NPChannelBase is only intended
// for use on one thread per process. Using TLS to store the globals removes the
// worst thread hostility in this class, especially needed for webview which
// runs in single-process mode. TODO(joth): Make a complete fix, most likely
// as part of addressing http://crbug.com/258510.
base::LazyInstance<base::ThreadLocalPointer<ChannelGlobals> >::Leaky
    g_channels_tls_ptr = LAZY_INSTANCE_INITIALIZER;

ChannelGlobals* GetChannelGlobals() {
  ChannelGlobals* globals = g_channels_tls_ptr.Get().Get();
  if (!globals) {
    globals = new ChannelGlobals;
    g_channels_tls_ptr.Get().Set(globals);
  }
  return globals;
}

#else

base::LazyInstance<ChannelGlobals>::Leaky g_channels_globals =
    LAZY_INSTANCE_INITIALIZER;

ChannelGlobals* GetChannelGlobals() { return g_channels_globals.Pointer(); }

#endif  // OS_ANDROID

ChannelMap* GetChannelMap() {
  return &GetChannelGlobals()->channel_map;
}

}  // namespace

NPChannelBase* NPChannelBase::GetChannel(
    const IPC::ChannelHandle& channel_handle, IPC::Channel::Mode mode,
    ChannelFactory factory, base::MessageLoopProxy* ipc_message_loop,
    bool create_pipe_now, base::WaitableEvent* shutdown_event) {
#if defined(OS_POSIX)
  // On POSIX the channel_handle conveys an FD (socket) which is duped by the
  // kernel during the IPC message exchange (via the SCM_RIGHTS mechanism).
  // Ensure we do not leak this FD.
  base::ScopedFD fd(channel_handle.socket.auto_close ?
                    channel_handle.socket.fd : -1);
#endif

  scoped_refptr<NPChannelBase> channel;
  std::string channel_key = channel_handle.name;
  ChannelMap::const_iterator iter = GetChannelMap()->find(channel_key);
  if (iter == GetChannelMap()->end()) {
    channel = factory();
  } else {
    channel = iter->second;
  }

  DCHECK(channel.get() != NULL);

  if (!channel->channel_valid()) {
    channel->channel_handle_ = channel_handle;
#if defined(OS_POSIX)
    ignore_result(fd.release());
#endif
    if (mode & IPC::Channel::MODE_SERVER_FLAG) {
      channel->channel_handle_.name =
          IPC::Channel::GenerateVerifiedChannelID(channel_key);
    }
    channel->mode_ = mode;
    if (channel->Init(ipc_message_loop, create_pipe_now, shutdown_event)) {
      (*GetChannelMap())[channel_key] = channel;
    } else {
      channel = NULL;
    }
  }

  return channel.get();
}

void NPChannelBase::Broadcast(IPC::Message* message) {
  for (ChannelMap::iterator iter = GetChannelMap()->begin();
       iter != GetChannelMap()->end();
       ++iter) {
    iter->second->Send(new IPC::Message(*message));
  }
  delete message;
}

NPChannelBase::NPChannelBase()
    : mode_(IPC::Channel::MODE_NONE),
      non_npobject_count_(0),
      peer_pid_(0),
      in_remove_route_(false),
      default_owner_(NULL),
      channel_valid_(false),
      in_unblock_dispatch_(0),
      send_unblocking_only_during_unblock_dispatch_(false) {
}

NPChannelBase::~NPChannelBase() {
  // TODO(wez): Establish why these would ever be non-empty at teardown.
  //DCHECK(npobject_listeners_.empty());
  //DCHECK(proxy_map_.empty());
  //DCHECK(stub_map_.empty());
  DCHECK(owner_to_route_.empty());
  DCHECK(route_to_owner_.empty());
}

NPChannelBase* NPChannelBase::GetCurrentChannel() {
  return GetChannelGlobals()->current_channel.get();
}

void NPChannelBase::CleanupChannels() {
  // Make a copy of the references as we can't iterate the map since items will
  // be removed from it as we clean them up.
  std::vector<scoped_refptr<NPChannelBase> > channels;
  for (ChannelMap::const_iterator iter = GetChannelMap()->begin();
       iter != GetChannelMap()->end();
       ++iter) {
    channels.push_back(iter->second);
  }

  for (size_t i = 0; i < channels.size(); ++i)
    channels[i]->CleanUp();

  // This will clean up channels added to the map for which subsequent
  // AddRoute wasn't called
  GetChannelMap()->clear();
}

NPObjectBase* NPChannelBase::GetNPObjectListenerForRoute(int route_id) {
  ListenerMap::iterator iter = npobject_listeners_.find(route_id);
  if (iter == npobject_listeners_.end()) {
    DLOG(WARNING) << "Invalid route id passed in:" << route_id;
    return NULL;
  }
  return iter->second;
}

base::WaitableEvent* NPChannelBase::GetModalDialogEvent(int render_view_id) {
  return NULL;
}

bool NPChannelBase::Init(base::MessageLoopProxy* ipc_message_loop,
                         bool create_pipe_now,
                         base::WaitableEvent* shutdown_event) {
#if defined(OS_POSIX)
  // Attempting to initialize with an invalid channel handle.
  // See http://crbug.com/97285 for details.
  if (mode_ == IPC::Channel::MODE_CLIENT && -1 == channel_handle_.socket.fd)
    return false;
#endif

  channel_ = IPC::SyncChannel::Create(
      channel_handle_, mode_, this, ipc_message_loop, create_pipe_now,
      shutdown_event);

#if defined(OS_POSIX)
  // Check the validity of fd for bug investigation.  Remove after fixed.
  // See crbug.com/97285 for details.
  if (mode_ == IPC::Channel::MODE_SERVER)
    CHECK_NE(-1, channel_->GetClientFileDescriptor());
#endif

  channel_valid_ = true;
  return true;
}

bool NPChannelBase::Send(IPC::Message* message) {
  if (!channel_) {
    VLOG(1) << "Channel is NULL; dropping message";
    delete message;
    return false;
  }

  if (send_unblocking_only_during_unblock_dispatch_ &&
      in_unblock_dispatch_ == 0 &&
      message->is_sync()) {
    message->set_unblock(false);
  }

  return channel_->Send(message);
}

int NPChannelBase::Count() {
  return static_cast<int>(GetChannelMap()->size());
}

bool NPChannelBase::OnMessageReceived(const IPC::Message& message) {
  // Push this channel as the current channel being processed. This also forms
  // a stack of scoped_refptr avoiding ourselves (or any instance higher
  // up the callstack) from being deleted while processing a message.
  base::AutoReset<scoped_refptr<NPChannelBase> > keep_alive(
      &GetChannelGlobals()->current_channel, this);

  bool handled;
  if (message.should_unblock())
    in_unblock_dispatch_++;
  if (message.routing_id() == MSG_ROUTING_CONTROL) {
    handled = OnControlMessageReceived(message);
  } else {
    handled = router_.RouteMessage(message);
    if (!handled && message.is_sync()) {
      // The listener has gone away, so we must respond or else the caller will
      // hang waiting for a reply.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
      reply->set_reply_error();
      Send(reply);
    }
  }
  if (message.should_unblock())
    in_unblock_dispatch_--;

  return handled;
}

void NPChannelBase::OnChannelConnected(int32 peer_pid) {
  peer_pid_ = peer_pid;
}

void NPChannelBase::AddRoute(int route_id,
                             IPC::Listener* listener,
                             NPObjectBase* npobject) {
  if (npobject) {
    npobject_listeners_[route_id] = npobject;
  } else {
    non_npobject_count_++;
  }

  router_.AddRoute(route_id, listener);
}

void NPChannelBase::RemoveRoute(int route_id) {
  router_.RemoveRoute(route_id);

  ListenerMap::iterator iter = npobject_listeners_.find(route_id);
  if (iter != npobject_listeners_.end()) {
    // This was an NPObject proxy or stub, it's not involved in the refcounting.

    // If this RemoveRoute call from the NPObject is a result of us calling
    // OnChannelError below, don't call erase() here because that'll corrupt
    // the iterator below.
    if (in_remove_route_) {
      iter->second = NULL;
    } else {
      npobject_listeners_.erase(iter);
    }

    return;
  }

  non_npobject_count_--;
  DCHECK(non_npobject_count_ >= 0);

  if (!non_npobject_count_) {
    base::AutoReset<bool> auto_reset_in_remove_route(&in_remove_route_, true);
    for (ListenerMap::iterator npobj_iter = npobject_listeners_.begin();
         npobj_iter != npobject_listeners_.end(); ++npobj_iter) {
      if (npobj_iter->second) {
        npobj_iter->second->GetChannelListener()->OnChannelError();
      }
    }

    for (ChannelMap::iterator iter = GetChannelMap()->begin();
         iter != GetChannelMap()->end(); ++iter) {
      if (iter->second.get() == this) {
        GetChannelMap()->erase(iter);
        return;
      }
    }

    NOTREACHED();
  }
}

bool NPChannelBase::OnControlMessageReceived(const IPC::Message& msg) {
  NOTREACHED() <<
      "should override in subclass if you care about control messages";
  return false;
}

void NPChannelBase::OnChannelError() {
  channel_valid_ = false;

  // TODO(shess): http://crbug.com/97285
  // Once an error is seen on a channel, remap the channel to prevent
  // it from being vended again.  Keep the channel in the map so
  // RemoveRoute() can clean things up correctly.
  for (ChannelMap::iterator iter = GetChannelMap()->begin();
       iter != GetChannelMap()->end(); ++iter) {
    if (iter->second.get() == this) {
      // Insert new element before invalidating |iter|.
      (*GetChannelMap())[iter->first + "-error"] = iter->second;
      GetChannelMap()->erase(iter);
      break;
    }
  }
}

void NPChannelBase::AddMappingForNPObjectProxy(int route_id,
                                               NPObject* object) {
  proxy_map_[route_id] = object;
}

void NPChannelBase::RemoveMappingForNPObjectProxy(int route_id) {
  proxy_map_.erase(route_id);
}

void NPChannelBase::AddMappingForNPObjectStub(int route_id,
                                              NPObject* object) {
  DCHECK(object != NULL);
  stub_map_[object] = route_id;
}

void NPChannelBase::RemoveMappingForNPObjectStub(int route_id,
                                                 NPObject* object) {
  DCHECK(object != NULL);
  stub_map_.erase(object);
}

void NPChannelBase::AddMappingForNPObjectOwner(int route_id,
                                               struct _NPP* owner) {
  DCHECK(owner != NULL);
  route_to_owner_[route_id] = owner;
  owner_to_route_[owner] = route_id;
}

void NPChannelBase::SetDefaultNPObjectOwner(struct _NPP* owner) {
  DCHECK(owner != NULL);
  default_owner_ = owner;
}

void NPChannelBase::RemoveMappingForNPObjectOwner(int route_id) {
  DCHECK(route_to_owner_.find(route_id) != route_to_owner_.end());
  owner_to_route_.erase(route_to_owner_[route_id]);
  route_to_owner_.erase(route_id);
}

NPObject* NPChannelBase::GetExistingNPObjectProxy(int route_id) {
  ProxyMap::iterator iter = proxy_map_.find(route_id);
  return iter != proxy_map_.end() ? iter->second : NULL;
}

int NPChannelBase::GetExistingRouteForNPObjectStub(NPObject* npobject) {
  StubMap::iterator iter = stub_map_.find(npobject);
  return iter != stub_map_.end() ? iter->second : MSG_ROUTING_NONE;
}

NPP NPChannelBase::GetExistingNPObjectOwner(int route_id) {
  RouteToOwnerMap::iterator iter = route_to_owner_.find(route_id);
  return iter != route_to_owner_.end() ? iter->second : default_owner_;
}

int NPChannelBase::GetExistingRouteForNPObjectOwner(NPP owner) {
  OwnerToRouteMap::iterator iter = owner_to_route_.find(owner);
  return iter != owner_to_route_.end() ? iter->second : MSG_ROUTING_NONE;
}

}  // namespace content
