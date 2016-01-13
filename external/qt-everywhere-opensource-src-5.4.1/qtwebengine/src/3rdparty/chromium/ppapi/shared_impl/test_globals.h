// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_TEST_GLOBALS_H_
#define PPAPI_SHARED_IMPL_TEST_GLOBALS_H_

#include "base/compiler_specific.h"
#include "base/memory/shared_memory.h"
#include "ppapi/shared_impl/callback_tracker.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

class TestVarTracker : public VarTracker {
 public:
  TestVarTracker() : VarTracker(THREAD_SAFE) {}
  virtual ~TestVarTracker() {}
  virtual PP_Var MakeResourcePPVarFromMessage(
      PP_Instance instance,
      const IPC::Message& creation_message,
      int pending_renderer_id,
      int pending_browser_id) OVERRIDE {
    return PP_MakeNull();
  }
  virtual ResourceVar* MakeResourceVar(PP_Resource pp_resource) OVERRIDE {
    return NULL;
  }
  virtual ArrayBufferVar* CreateArrayBuffer(uint32 size_in_bytes) OVERRIDE {
    return NULL;
  }
  virtual ArrayBufferVar* CreateShmArrayBuffer(uint32 size_in_bytes,
                                               base::SharedMemoryHandle handle)
      OVERRIDE {
    return NULL;
  }
  virtual void DidDeleteInstance(PP_Instance instance) OVERRIDE {}
  virtual int TrackSharedMemoryHandle(PP_Instance instance,
                                      base::SharedMemoryHandle handle,
                                      uint32 size_in_bytes) OVERRIDE {
    return -1;
  }
  virtual bool StopTrackingSharedMemoryHandle(int id,
                                              PP_Instance instance,
                                              base::SharedMemoryHandle* handle,
                                              uint32* size_in_bytes) OVERRIDE {
    return false;
  }
};

// Implementation of PpapiGlobals for tests that don't need either the host- or
// plugin-specific implementations.
class TestGlobals : public PpapiGlobals {
 public:
  TestGlobals();
  explicit TestGlobals(PpapiGlobals::PerThreadForTest);
  virtual ~TestGlobals();

  // PpapiGlobals implementation.
  virtual ResourceTracker* GetResourceTracker() OVERRIDE;
  virtual VarTracker* GetVarTracker() OVERRIDE;
  virtual CallbackTracker* GetCallbackTrackerForInstance(PP_Instance instance)
      OVERRIDE;
  virtual thunk::PPB_Instance_API* GetInstanceAPI(PP_Instance instance)
      OVERRIDE;
  virtual thunk::ResourceCreationAPI* GetResourceCreationAPI(
      PP_Instance instance) OVERRIDE;
  virtual PP_Module GetModuleForInstance(PP_Instance instance) OVERRIDE;
  virtual std::string GetCmdLine() OVERRIDE;
  virtual void PreCacheFontForFlash(const void* logfontw) OVERRIDE;
  virtual void LogWithSource(PP_Instance instance,
                             PP_LogLevel level,
                             const std::string& source,
                             const std::string& value) OVERRIDE;
  virtual void BroadcastLogWithSource(PP_Module module,
                                      PP_LogLevel level,
                                      const std::string& source,
                                      const std::string& value) OVERRIDE;
  virtual MessageLoopShared* GetCurrentMessageLoop() OVERRIDE;
  virtual base::TaskRunner* GetFileTaskRunner() OVERRIDE;

  // PpapiGlobals overrides:
  virtual bool IsHostGlobals() const OVERRIDE;

 private:
  ResourceTracker resource_tracker_;
  TestVarTracker var_tracker_;
  scoped_refptr<CallbackTracker> callback_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TestGlobals);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_TEST_GLOBALS_H_
