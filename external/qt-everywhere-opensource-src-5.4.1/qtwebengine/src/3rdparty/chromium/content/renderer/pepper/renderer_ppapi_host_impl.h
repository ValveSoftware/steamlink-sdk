// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_RENDERER_PPAPI_HOST_IMPL_H_
#define CONTENT_RENDERER_PEPPER_RENDERER_PPAPI_HOST_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/content_renderer_pepper_host_factory.h"
#include "ppapi/host/ppapi_host.h"

namespace IPC {
class Sender;
}

namespace ppapi {

namespace proxy {
class HostDispatcher;
}

namespace thunk {
class ResourceCreationAPI;
}

}  // namespace ppapi

namespace content {

class PepperInProcessRouter;
class PepperPluginInstanceImpl;
class PluginModule;

// This class is attached to a PluginModule which manages our lifetime.
class RendererPpapiHostImpl : public RendererPpapiHost {
 public:
  virtual ~RendererPpapiHostImpl();

  // Factory functions to create in process or out-of-process host impls. The
  // host will be created and associated with the given module, which must not
  // already have embedder state on it.
  //
  // The module will take ownership of the new host impl. The returned value
  // does not pass ownership, it's just for the information of the caller.
  static RendererPpapiHostImpl* CreateOnModuleForOutOfProcess(
      PluginModule* module,
      ppapi::proxy::HostDispatcher* dispatcher,
      const ppapi::PpapiPermissions& permissions);
  static RendererPpapiHostImpl* CreateOnModuleForInProcess(
      PluginModule* module,
      const ppapi::PpapiPermissions& permissions);

  // Returns the RendererPpapiHostImpl associated with the given PP_Instance,
  // or NULL if the instance is invalid.
  static RendererPpapiHostImpl* GetForPPInstance(PP_Instance pp_instance);

  // Returns the router that we use for in-process IPC emulation (see the
  // pepper_in_process_router.h for more). This will be NULL when the plugin
  // is running out-of-process.
  PepperInProcessRouter* in_process_router() {
    return in_process_router_.get();
  }

  // Creates the in-process resource creation API wrapper for the given
  // plugin instance. This object will reference the host impl, so the
  // host impl should outlive the returned pointer. Since the resource
  // creation object is associated with the instance, this will generally
  // happen automatically.
  scoped_ptr<ppapi::thunk::ResourceCreationAPI>
      CreateInProcessResourceCreationAPI(PepperPluginInstanceImpl* instance);

  PepperPluginInstanceImpl* GetPluginInstanceImpl(PP_Instance instance) const;

  bool IsExternalPluginHost() const;

  // RendererPpapiHost implementation.
  virtual ppapi::host::PpapiHost* GetPpapiHost() OVERRIDE;
  virtual bool IsValidInstance(PP_Instance instance) const OVERRIDE;
  virtual PepperPluginInstance* GetPluginInstance(PP_Instance instance) const
      OVERRIDE;
  virtual RenderFrame* GetRenderFrameForInstance(PP_Instance instance) const
      OVERRIDE;
  virtual RenderView* GetRenderViewForInstance(PP_Instance instance) const
      OVERRIDE;
  virtual blink::WebPluginContainer* GetContainerForInstance(
      PP_Instance instance) const OVERRIDE;
  virtual base::ProcessId GetPluginPID() const OVERRIDE;
  virtual bool HasUserGesture(PP_Instance instance) const OVERRIDE;
  virtual int GetRoutingIDForWidget(PP_Instance instance) const OVERRIDE;
  virtual gfx::Point PluginPointToRenderFrame(PP_Instance instance,
                                              const gfx::Point& pt) const
      OVERRIDE;
  virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      bool should_close_source) OVERRIDE;
  virtual bool IsRunningInProcess() const OVERRIDE;
  virtual std::string GetPluginName() const OVERRIDE;
  virtual void SetToExternalPluginHost() OVERRIDE;
  virtual void CreateBrowserResourceHosts(
      PP_Instance instance,
      const std::vector<IPC::Message>& nested_msgs,
      const base::Callback<void(const std::vector<int>&)>& callback) const
      OVERRIDE;
  virtual GURL GetDocumentURL(PP_Instance instance) const OVERRIDE;

 private:
  RendererPpapiHostImpl(PluginModule* module,
                        ppapi::proxy::HostDispatcher* dispatcher,
                        const ppapi::PpapiPermissions& permissions);
  RendererPpapiHostImpl(PluginModule* module,
                        const ppapi::PpapiPermissions& permissions);

  // Retrieves the plugin instance object associated with the given PP_Instance
  // and validates that it is one of the instances associated with our module.
  // Returns NULL on failure.
  //
  // We use this to security check the PP_Instance values sent from a plugin to
  // make sure it's not trying to spoof another instance.
  PepperPluginInstanceImpl* GetAndValidateInstance(PP_Instance instance) const;

  PluginModule* module_;  // Non-owning pointer.

  // The dispatcher we use to send messagse when the plugin is out-of-process.
  // Will be null when running in-process. Non-owning pointer.
  ppapi::proxy::HostDispatcher* dispatcher_;

  scoped_ptr<ppapi::host::PpapiHost> ppapi_host_;

  // Null when running out-of-process.
  scoped_ptr<PepperInProcessRouter> in_process_router_;

  // Whether the plugin is running in process.
  bool is_running_in_process_;

  // Whether this is a host for external plugins.
  bool is_external_plugin_host_;

  DISALLOW_COPY_AND_ASSIGN(RendererPpapiHostImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_RENDERER_PPAPI_HOST_IMPL_H_
