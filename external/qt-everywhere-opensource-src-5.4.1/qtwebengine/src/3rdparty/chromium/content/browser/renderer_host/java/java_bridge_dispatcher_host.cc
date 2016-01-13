// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/java/java_bridge_dispatcher_host.h"

#include "base/android/java_handler_thread.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "content/browser/renderer_host/java/java_bridge_channel_host.h"
#include "content/child/child_process.h"
#include "content/child/npapi/npobject_stub.h"
#include "content/child/npapi/npobject_util.h"  // For CreateNPVariantParam()
#include "content/common/java_bridge_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/WebKit/public/web/WebBindings.h"

#if !defined(OS_ANDROID)
#error "JavaBridge only supports OS_ANDROID"
#endif

namespace content {

namespace {
// The JavaBridge needs to use a Java thread so the callback
// will happen on a thread with a prepared Looper.
class JavaBridgeThread : public base::android::JavaHandlerThread {
 public:
  JavaBridgeThread() : base::android::JavaHandlerThread("JavaBridge") {
    Start();
  }
  virtual ~JavaBridgeThread() {
    Stop();
  }
};

void CleanUpStubs(const std::vector<base::WeakPtr<NPObjectStub> > & stubs) {
  for (size_t i = 0; i < stubs.size(); ++i) {
    if (stubs[i]) {
      stubs[i]->DeleteSoon();
    }
  }
}

base::LazyInstance<JavaBridgeThread> g_background_thread =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

JavaBridgeDispatcherHost::JavaBridgeDispatcherHost(
    RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
}

JavaBridgeDispatcherHost::~JavaBridgeDispatcherHost() {
  g_background_thread.Get().message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CleanUpStubs, stubs_));
}

void JavaBridgeDispatcherHost::AddNamedObject(const base::string16& name,
                                              NPObject* object) {
  NPVariant_Param variant_param;
  CreateNPVariantParam(object, &variant_param);

  Send(new JavaBridgeMsg_AddNamedObject(
      render_frame_host_->GetRoutingID(), name, variant_param));
}

void JavaBridgeDispatcherHost::RemoveNamedObject(const base::string16& name) {
  // On receipt of this message, the JavaBridgeDispatcher will drop its
  // reference to the corresponding proxy object. When the last reference is
  // removed, the proxy object will delete its NPObjectProxy, which will cause
  // the NPObjectStub to be deleted, which will drop its reference to the
  // original NPObject.
  Send(new JavaBridgeMsg_RemoveNamedObject(
      render_frame_host_->GetRoutingID(), name));
}

void JavaBridgeDispatcherHost::RenderFrameDeleted() {
  render_frame_host_ = NULL;
}

void JavaBridgeDispatcherHost::OnGetChannelHandle(IPC::Message* reply_msg) {
  g_background_thread.Get().message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&JavaBridgeDispatcherHost::GetChannelHandle, this, reply_msg));
}

void JavaBridgeDispatcherHost::Send(IPC::Message* msg) {
  if (render_frame_host_) {
    render_frame_host_->Send(msg);
    return;
  }

  delete msg;
}

void JavaBridgeDispatcherHost::GetChannelHandle(IPC::Message* reply_msg) {
  // The channel creates the channel handle based on the renderer ID we passed
  // to GetJavaBridgeChannelHost() and, on POSIX, the file descriptor used by
  // the underlying channel.
  JavaBridgeHostMsg_GetChannelHandle::WriteReplyParams(
      reply_msg,
      channel_->channel_handle());

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&JavaBridgeDispatcherHost::Send, this, reply_msg));
}

void JavaBridgeDispatcherHost::CreateNPVariantParam(NPObject* object,
                                                    NPVariant_Param* param) {
  // The JavaBridgeChannelHost needs to be created on the background thread, as
  // that is where Java objects will live, and CreateNPVariantParam() needs the
  // channel to create the NPObjectStub. To avoid blocking here until the
  // channel is ready, create the NPVariant_Param by hand, then post a message
  // to the background thread to set up the channel and create the corresponding
  // NPObjectStub. Post that message before doing any IPC, to make sure that
  // the channel and object proxies are ready before responses are received
  // from the renderer.

  // Create an NPVariantParam suitable for serialization over IPC from our
  // NPVariant. See CreateNPVariantParam() in npobject_utils.
  param->type = NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID;
  int route_id = JavaBridgeChannelHost::ThreadsafeGenerateRouteID();
  param->npobject_routing_id = route_id;

  blink::WebBindings::retainObject(object);
  g_background_thread.Get().message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&JavaBridgeDispatcherHost::CreateObjectStub, this, object,
                 render_frame_host_->GetProcess()->GetID(), route_id));
}

void JavaBridgeDispatcherHost::CreateObjectStub(NPObject* object,
                                                int render_process_id,
                                                int route_id) {
  DCHECK_EQ(g_background_thread.Get().message_loop(),
            base::MessageLoop::current());
  if (!channel_.get()) {
    channel_ = JavaBridgeChannelHost::GetJavaBridgeChannelHost(
        render_process_id,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  }

  // In a typical scenario, the lifetime of each NPObjectStub is governed by
  // that of the NPObjectProxy in the renderer, via the channel. However,
  // we cannot guaranteed that the renderer always terminates cleanly
  // (crashes / sometimes just unavoidable). We keep a weak reference to
  // it now and schedule a delete on it when this host is getting deleted.

  // Pass 0 for the containing window, as it's only used by plugins to pump the
  // window message queue when a method on a renderer-side object causes a
  // dialog to be displayed, and the Java Bridge does not need this
  // functionality. The page URL is also not required.
  stubs_.push_back((new NPObjectStub(
      object, channel_.get(), route_id, 0, GURL()))->AsWeakPtr());

  // The NPObjectStub takes a reference to the NPObject. Release the ref added
  // in CreateNPVariantParam().
  blink::WebBindings::releaseObject(object);
}

}  // namespace content
