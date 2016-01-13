// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/java/java_bridge_dispatcher_host_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/java/java_bound_object.h"
#include "content/browser/renderer_host/java/java_bridge_dispatcher_host.h"
#include "content/common/android/hash_set.h"
#include "content/common/java_bridge_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/WebKit/public/web/WebBindings.h"

namespace content {

JavaBridgeDispatcherHostManager::JavaBridgeDispatcherHostManager(
    WebContents* web_contents,
    jobject retained_object_set)
    : WebContentsObserver(web_contents),
      retained_object_set_(base::android::AttachCurrentThread(),
                           retained_object_set),
      allow_object_contents_inspection_(true) {
  DCHECK(retained_object_set);
}

JavaBridgeDispatcherHostManager::~JavaBridgeDispatcherHostManager() {
  for (ObjectMap::iterator iter = objects_.begin(); iter != objects_.end();
      ++iter) {
    blink::WebBindings::releaseObject(iter->second);
  }
  DCHECK_EQ(0U, instances_.size());
}

void JavaBridgeDispatcherHostManager::AddNamedObject(const base::string16& name,
                                                     NPObject* object) {
  // Record this object in a map so that we can add it into RenderViewHosts
  // created later. The JavaBridgeDispatcherHost instances will take a
  // reference to the object, but we take one too, because this method can be
  // called before there are any such instances.
  blink::WebBindings::retainObject(object);
  objects_[name] = object;

  for (InstanceMap::iterator iter = instances_.begin();
      iter != instances_.end(); ++iter) {
    iter->second->AddNamedObject(name, object);
  }
}

void JavaBridgeDispatcherHostManager::RemoveNamedObject(
    const base::string16& name) {
  ObjectMap::iterator iter = objects_.find(name);
  if (iter == objects_.end()) {
    return;
  }

  blink::WebBindings::releaseObject(iter->second);
  objects_.erase(iter);

  for (InstanceMap::iterator iter = instances_.begin();
      iter != instances_.end(); ++iter) {
    iter->second->RemoveNamedObject(name);
  }
}

void JavaBridgeDispatcherHostManager::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  // Creates a JavaBridgeDispatcherHost for the specified RenderViewHost and
  // adds all currently registered named objects to the new instance.
  scoped_refptr<JavaBridgeDispatcherHost> instance =
      new JavaBridgeDispatcherHost(render_frame_host);

  for (ObjectMap::const_iterator iter = objects_.begin();
      iter != objects_.end(); ++iter) {
    instance->AddNamedObject(iter->first, iter->second);
  }

  instances_[render_frame_host] = instance;
}

void JavaBridgeDispatcherHostManager::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (!instances_.count(render_frame_host))  // Needed for tests.
    return;
  instances_[render_frame_host]->RenderFrameDeleted();
  instances_.erase(render_frame_host);
}

void JavaBridgeDispatcherHostManager::DocumentAvailableInMainFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Called when the window object has been cleared in the main frame.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    JNI_Java_HashSet_clear(env, retained_object_set);

    // We also need to add back the named objects we have so far as they
    // should survive navigations.
    ObjectMap::iterator it = objects_.begin();
    for (; it != objects_.end(); ++it) {
      JNI_Java_HashSet_add(env, retained_object_set,
                           JavaBoundObject::GetJavaObject(it->second));
    }
  }
}

bool JavaBridgeDispatcherHostManager::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);
  if (!instances_.count(render_frame_host))
    return false;
  scoped_refptr<JavaBridgeDispatcherHost> instance =
      instances_[render_frame_host];
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(JavaBridgeDispatcherHostManager, message)
    IPC_MESSAGE_FORWARD_DELAY_REPLY(
        JavaBridgeHostMsg_GetChannelHandle,
        instance.get(),
        JavaBridgeDispatcherHost::OnGetChannelHandle)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void JavaBridgeDispatcherHostManager::JavaBoundObjectCreated(
    const base::android::JavaRef<jobject>& object) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    JNI_Java_HashSet_add(env, retained_object_set, object);
  }
}

void JavaBridgeDispatcherHostManager::JavaBoundObjectDestroyed(
    const base::android::JavaRef<jobject>& object) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    JNI_Java_HashSet_remove(env, retained_object_set, object);
  }
}

void JavaBridgeDispatcherHostManager::SetAllowObjectContentsInspection(
    bool allow) {
  allow_object_contents_inspection_ = allow;
}

}  // namespace content
