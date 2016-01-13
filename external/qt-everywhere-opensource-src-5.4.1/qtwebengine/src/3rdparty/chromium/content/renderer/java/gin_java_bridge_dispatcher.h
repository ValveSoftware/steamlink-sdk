// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_H_
#define CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_H_

#include <map>
#include <set>

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/renderer/render_frame_observer.h"

namespace blink {
class WebFrame;
}

namespace content {

class GinJavaBridgeObject;

// This class handles injecting Java objects into the main frame of a
// RenderView. The 'add' and 'remove' messages received from the browser
// process modify the entries in a map of 'pending' objects. These objects are
// bound to the window object of the main frame when that window object is next
// cleared. These objects remain bound until the window object is cleared
// again.
class GinJavaBridgeDispatcher
    : public base::SupportsWeakPtr<GinJavaBridgeDispatcher>,
      public RenderFrameObserver {
 public:
  // GinJavaBridgeObjects are managed by gin. An object gets destroyed
  // when it is no more referenced from JS. As GinJavaBridgeObject reports
  // deletion of self to GinJavaBridgeDispatcher, we would not have stale
  // pointers here.
  typedef IDMap<GinJavaBridgeObject, IDMapExternalPointer> ObjectMap;
  typedef ObjectMap::KeyType ObjectID;

  explicit GinJavaBridgeDispatcher(RenderFrame* render_frame);
  virtual ~GinJavaBridgeDispatcher();

  // RenderFrameObserver override:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidClearWindowObject() OVERRIDE;

  void GetJavaMethods(ObjectID object_id, std::set<std::string>* methods);
  bool HasJavaMethod(ObjectID object_id, const std::string& method_name);
  scoped_ptr<base::Value> InvokeJavaMethod(ObjectID object_id,
                                           const std::string& method_name,
                                           const base::ListValue& arguments);
  GinJavaBridgeObject* GetObject(ObjectID object_id);
  void OnGinJavaBridgeObjectDeleted(ObjectID object_id);

 private:
  void OnAddNamedObject(const std::string& name,
                        ObjectID object_id);
  void OnRemoveNamedObject(const std::string& name);
  void OnSetAllowObjectContentsInspection(bool allow);

  typedef std::map<std::string, ObjectID> NamedObjectMap;
  NamedObjectMap named_objects_;
  ObjectMap objects_;
  bool inside_did_clear_window_object_;

  DISALLOW_COPY_AND_ASSIGN(GinJavaBridgeDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_H_
