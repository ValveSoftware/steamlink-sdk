// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_MOCK_RENDERER_PPAPI_HOST_H_
#define CONTENT_RENDERER_PEPPER_MOCK_RENDERER_PPAPI_HOST_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/content_renderer_pepper_host_factory.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/resource_message_test_sink.h"

namespace content {
class FakePepperPluginInstance;
class PluginModule;

// A mock RendererPpapiHost for testing resource hosts. Messages sent by
// resources through this will get added to the test sink.
class MockRendererPpapiHost : public RendererPpapiHost {
 public:
  // This function takes the RenderView and instance that the mock resource
  // host will be associated with.
  MockRendererPpapiHost(RenderView* render_view, PP_Instance instance);
  virtual ~MockRendererPpapiHost();

  ppapi::proxy::ResourceMessageTestSink& sink() { return sink_; }
  PP_Instance pp_instance() const { return pp_instance_; }

  // Sets whether there is currently a user gesture. Defaults to false.
  void set_has_user_gesture(bool gesture) { has_user_gesture_ = gesture; }

  // RendererPpapiHost.
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
  ppapi::proxy::ResourceMessageTestSink sink_;
  ppapi::host::PpapiHost ppapi_host_;

  RenderView* render_view_;
  PP_Instance pp_instance_;

  bool has_user_gesture_;

  scoped_ptr<FakePepperPluginInstance> plugin_instance_;

  DISALLOW_COPY_AND_ASSIGN(MockRendererPpapiHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_MOCK_RENDERER_PPAPI_HOST_H_
