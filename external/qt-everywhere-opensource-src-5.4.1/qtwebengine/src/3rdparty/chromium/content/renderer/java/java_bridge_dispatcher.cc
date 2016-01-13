// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/java/java_bridge_dispatcher.h"

#include "content/child/child_process.h"
#include "content/child/npapi/npobject_util.h"  // For CreateNPVariant()
#include "content/common/java_bridge_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/java/java_bridge_channel.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace content {

JavaBridgeDispatcher::JavaBridgeDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
}

void JavaBridgeDispatcher::EnsureChannelIsSetUp() {
  if (channel_.get()) {
    return;
  }

  IPC::ChannelHandle channel_handle;
  Send(new JavaBridgeHostMsg_GetChannelHandle(routing_id(), &channel_handle));

  channel_ = JavaBridgeChannel::GetJavaBridgeChannel(
      channel_handle, ChildProcess::current()->io_message_loop_proxy());
}

JavaBridgeDispatcher::~JavaBridgeDispatcher() {
  for (ObjectMap::const_iterator iter = objects_.begin();
      iter != objects_.end(); ++iter) {
    blink::WebBindings::releaseObject(NPVARIANT_TO_OBJECT(iter->second));
  }
}

bool JavaBridgeDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(JavaBridgeDispatcher, msg)
    IPC_MESSAGE_HANDLER(JavaBridgeMsg_AddNamedObject, OnAddNamedObject)
    IPC_MESSAGE_HANDLER(JavaBridgeMsg_RemoveNamedObject,
                        OnRemoveNamedObject)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void JavaBridgeDispatcher::DidClearWindowObject() {
  // Note that we have to (re)bind all objects, as they will have been unbound
  // when the window object was cleared.
  for (ObjectMap::const_iterator iter = objects_.begin();
       iter != objects_.end();
       ++iter) {
    NPObject* object = NPVARIANT_TO_OBJECT(iter->second);
    // De-associate from the existing V8 wrapper, so we don't pull any
    // of the wrapper's custom properties into the context of the page we
    // have navigated to.
    blink::WebBindings::dropV8WrapperForObject(object);
    // This refs the NPObject. This reference is dropped when either the window
    // object is later cleared, or the object is GC'ed. So the object may be
    // deleted at any time after OnRemoveNamedObject() is called.
    render_frame()->GetWebFrame()->bindToWindowObject(iter->first, object);
  }
}

void JavaBridgeDispatcher::OnAddNamedObject(
    const base::string16& name,
    const NPVariant_Param& variant_param) {
  DCHECK_EQ(variant_param.type, NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID);

  EnsureChannelIsSetUp();
  if (!channel_.get()) {
    // It's possible for |channel_| to be NULL if the RenderView is going away.
    return;
  }

  // This creates an NPObject, wrapped as an NPVariant. Pass 0 for the for
  // containing window, as this is only used by plugins to pump the window
  // message queue when a method on a renderer-side object causes a dialog to
  // be displayed, and the Java Bridge does not need this functionality. The
  // page URL is also not required.
  NPVariant variant;
  bool created =
      CreateNPVariant(variant_param, channel_.get(), &variant, 0, GURL());
  DCHECK(created);
  DCHECK_EQ(variant.type, NPVariantType_Object);

  // The NPObject is created with a ref count of one, which we remove when
  // OnRemoveNamedObject() is called for that object.
  ObjectMap::iterator iter = objects_.find(name);
  if (iter != objects_.end()) {
    blink::WebBindings::releaseObject(NPVARIANT_TO_OBJECT(iter->second));
  }
  objects_[name] = variant;
}

void JavaBridgeDispatcher::OnRemoveNamedObject(const base::string16& name) {
  if (!channel_.get()) {
    DCHECK(objects_.empty());
    return;
  }

  // Removing an object does not unbind it from JavaScript until the window
  // object is next cleared. Note that the browser checks that the named object
  // is present.
  ObjectMap::iterator iter = objects_.find(name);
  DCHECK(iter != objects_.end());
  blink::WebBindings::releaseObject(NPVARIANT_TO_OBJECT(iter->second));
  objects_.erase(iter);
}

}  // namespace content
