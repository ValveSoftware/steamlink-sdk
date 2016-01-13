// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_MANAGER_H_

#include <map>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_observer.h"

struct NPObject;

namespace content {
class JavaBridgeDispatcherHost;
class RenderFrameHost;

// This class handles injecting Java objects into all of the RenderFrames
// associated with a WebContents. It manages a set of JavaBridgeDispatcherHost
// objects, one per RenderFrameHost.
class JavaBridgeDispatcherHostManager
    : public WebContentsObserver,
      public base::SupportsWeakPtr<JavaBridgeDispatcherHostManager> {
 public:
  JavaBridgeDispatcherHostManager(WebContents* web_contents,
                                  jobject retained_object_set);
  virtual ~JavaBridgeDispatcherHostManager();

  // These methods add or remove the object to each JavaBridgeDispatcherHost.
  // Each one holds a reference to the NPObject while the object is bound to
  // the corresponding RenderFrame. See JavaBridgeDispatcherHost for details.
  void AddNamedObject(const base::string16& name, NPObject* object);
  void RemoveNamedObject(const base::string16& name);

  // WebContentsObserver overrides
  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void RenderFrameDeleted(RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void DocumentAvailableInMainFrame() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 RenderFrameHost* render_frame_host) OVERRIDE;

  void JavaBoundObjectCreated(const base::android::JavaRef<jobject>& object);
  void JavaBoundObjectDestroyed(const base::android::JavaRef<jobject>& object);

  bool GetAllowObjectContentsInspection() const {
    return allow_object_contents_inspection_;
  }
  void SetAllowObjectContentsInspection(bool allow);

 private:
  typedef std::map<RenderFrameHost*, scoped_refptr<JavaBridgeDispatcherHost> >
      InstanceMap;
  InstanceMap instances_;
  typedef std::map<base::string16, NPObject*> ObjectMap;
  ObjectMap objects_;
  // Every time a JavaBoundObject backed by a real Java object is
  // created/destroyed, we insert/remove a strong ref to that Java object into
  // this set so that it doesn't get garbage collected while it's still
  // potentially in use. Although the set is managed native side, it's owned
  // and defined in Java so that pushing refs into it does not create new GC
  // roots that would prevent ContentViewCore from being garbage collected.
  JavaObjectWeakGlobalRef retained_object_set_;
  bool allow_object_contents_inspection_;

  DISALLOW_COPY_AND_ASSIGN(JavaBridgeDispatcherHostManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_MANAGER_H_
