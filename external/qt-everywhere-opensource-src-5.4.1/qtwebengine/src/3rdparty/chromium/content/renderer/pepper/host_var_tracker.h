// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_HOST_VAR_TRACKER_H_
#define CONTENT_RENDERER_PEPPER_HOST_VAR_TRACKER_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var_tracker.h"

typedef struct NPObject NPObject;

namespace ppapi {
class ArrayBufferVar;
class NPObjectVar;
class Var;
}

namespace content {

// Adds NPObject var tracking to the standard PPAPI VarTracker for use in the
// renderer.
class HostVarTracker : public ppapi::VarTracker {
 public:
  HostVarTracker();
  virtual ~HostVarTracker();

  // Tracks all live NPObjectVar. This is so we can map between instance +
  // NPObject and get the NPObjectVar corresponding to it. This Add/Remove
  // function is called by the NPObjectVar when it is created and
  // destroyed.
  void AddNPObjectVar(ppapi::NPObjectVar* object_var);
  void RemoveNPObjectVar(ppapi::NPObjectVar* object_var);

  // Looks up a previously registered NPObjectVar for the given NPObject and
  // instance. Returns NULL if there is no NPObjectVar corresponding to the
  // given NPObject for the given instance. See AddNPObjectVar above.
  ppapi::NPObjectVar* NPObjectVarForNPObject(PP_Instance instance,
                                             NPObject* np_object);

  // Returns the number of NPObjectVar's associated with the given instance.
  // Returns 0 if the instance isn't known.
  CONTENT_EXPORT int GetLiveNPObjectVarsForInstance(PP_Instance instance) const;

  // VarTracker public implementation.
  virtual PP_Var MakeResourcePPVarFromMessage(
      PP_Instance instance,
      const IPC::Message& creation_message,
      int pending_renderer_id,
      int pending_browser_id) OVERRIDE;
  virtual ppapi::ResourceVar* MakeResourceVar(PP_Resource pp_resource) OVERRIDE;
  virtual void DidDeleteInstance(PP_Instance instance) OVERRIDE;

  virtual int TrackSharedMemoryHandle(PP_Instance instance,
                                      base::SharedMemoryHandle file,
                                      uint32 size_in_bytes) OVERRIDE;
  virtual bool StopTrackingSharedMemoryHandle(int id,
                                              PP_Instance instance,
                                              base::SharedMemoryHandle* handle,
                                              uint32* size_in_bytes) OVERRIDE;

 private:
  // VarTracker private implementation.
  virtual ppapi::ArrayBufferVar* CreateArrayBuffer(uint32 size_in_bytes)
      OVERRIDE;
  virtual ppapi::ArrayBufferVar* CreateShmArrayBuffer(
      uint32 size_in_bytes,
      base::SharedMemoryHandle handle) OVERRIDE;

  // Clear the reference count of the given object and remove it from
  // live_vars_.
  void ForceReleaseNPObject(ppapi::NPObjectVar* object_var);

  typedef std::map<NPObject*, ppapi::NPObjectVar*> NPObjectToNPObjectVarMap;

  // Lists all known NPObjects, first indexed by the corresponding instance,
  // then by the NPObject*. This allows us to look up an NPObjectVar given
  // these two pieces of information.
  //
  // The instance map is lazily managed, so we'll add the
  // NPObjectToNPObjectVarMap lazily when the first NPObject var is created,
  // and delete it when it's empty.
  typedef std::map<PP_Instance, linked_ptr<NPObjectToNPObjectVarMap> >
      InstanceMap;
  InstanceMap instance_map_;

  // Tracks all shared memory handles used for transmitting array buffers.
  struct SharedMemoryMapEntry {
    PP_Instance instance;
    base::SharedMemoryHandle handle;
    uint32 size_in_bytes;
  };
  typedef std::map<int, SharedMemoryMapEntry> SharedMemoryMap;
  SharedMemoryMap shared_memory_map_;
  uint32_t last_shared_memory_map_id_;

  DISALLOW_COPY_AND_ASSIGN(HostVarTracker);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_HOST_VAR_TRACKER_H_
