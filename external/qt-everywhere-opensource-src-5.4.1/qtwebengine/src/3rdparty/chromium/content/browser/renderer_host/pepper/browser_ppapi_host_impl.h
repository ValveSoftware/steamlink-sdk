// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_IMPL_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/pepper/content_browser_pepper_host_factory.h"
#include "content/browser/renderer_host/pepper/ssl_context_helper.h"
#include "content/common/content_export.h"
#include "content/common/pepper_renderer_instance_data.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/common/process_type.h"
#include "ipc/message_filter.h"
#include "ppapi/host/ppapi_host.h"

namespace content {

class CONTENT_EXPORT BrowserPpapiHostImpl : public BrowserPpapiHost {
 public:
  // The creator is responsible for calling set_plugin_process_handle as soon
  // as it is known (we start the process asynchronously so it won't be known
  // when this object is created).
  // |external_plugin| signfies that this is a proxy created for an embedder's
  // plugin, i.e. using BrowserPpapiHost::CreateExternalPluginProcess.
  BrowserPpapiHostImpl(IPC::Sender* sender,
                       const ppapi::PpapiPermissions& permissions,
                       const std::string& plugin_name,
                       const base::FilePath& plugin_path,
                       const base::FilePath& profile_data_directory,
                       bool in_process,
                       bool external_plugin);
  virtual ~BrowserPpapiHostImpl();

  // BrowserPpapiHost.
  virtual ppapi::host::PpapiHost* GetPpapiHost() OVERRIDE;
  virtual base::ProcessHandle GetPluginProcessHandle() const OVERRIDE;
  virtual bool IsValidInstance(PP_Instance instance) const OVERRIDE;
  virtual bool GetRenderFrameIDsForInstance(PP_Instance instance,
                                            int* render_process_id,
                                            int* render_frame_id) const
      OVERRIDE;
  virtual const std::string& GetPluginName() OVERRIDE;
  virtual const base::FilePath& GetPluginPath() OVERRIDE;
  virtual const base::FilePath& GetProfileDataDirectory() OVERRIDE;
  virtual GURL GetDocumentURLForInstance(PP_Instance instance) OVERRIDE;
  virtual GURL GetPluginURLForInstance(PP_Instance instance) OVERRIDE;
  virtual void SetOnKeepaliveCallback(
      const BrowserPpapiHost::OnKeepaliveCallback& callback) OVERRIDE;

  void set_plugin_process_handle(base::ProcessHandle handle) {
    plugin_process_handle_ = handle;
  }

  bool external_plugin() const { return external_plugin_; }

  // These two functions are notifications that an instance has been created
  // or destroyed. They allow us to maintain a mapping of PP_Instance to data
  // associated with the instance including view IDs in the browser process.
  void AddInstance(PP_Instance instance,
                   const PepperRendererInstanceData& instance_data);
  void DeleteInstance(PP_Instance instance);

  scoped_refptr<IPC::MessageFilter> message_filter() {
    return message_filter_;
  }

  const scoped_refptr<SSLContextHelper>& ssl_context_helper() const {
    return ssl_context_helper_;
  }

 private:
  friend class BrowserPpapiHostTest;

  // Implementing MessageFilter on BrowserPpapiHostImpl makes it ref-counted,
  // preventing us from returning these to embedders without holding a
  // reference. To avoid that, define a message filter object.
  class HostMessageFilter : public IPC::MessageFilter {
   public:
    HostMessageFilter(ppapi::host::PpapiHost* ppapi_host,
                      BrowserPpapiHostImpl* browser_ppapi_host_impl);

    // IPC::MessageFilter.
    virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

    void OnHostDestroyed();

   private:
    virtual ~HostMessageFilter();

    void OnKeepalive();
    void OnHostMsgLogInterfaceUsage(int hash) const;

    // Non owning pointers cleared in OnHostDestroyed()
    ppapi::host::PpapiHost* ppapi_host_;
    BrowserPpapiHostImpl* browser_ppapi_host_impl_;
  };

  // Reports plugin activity to the callback set with SetOnKeepaliveCallback.
  void OnKeepalive();

  scoped_ptr<ppapi::host::PpapiHost> ppapi_host_;
  base::ProcessHandle plugin_process_handle_;
  std::string plugin_name_;
  base::FilePath plugin_path_;
  base::FilePath profile_data_directory_;

  // If true, this refers to a plugin running in the renderer process.
  bool in_process_;

  // If true, this is an external plugin, i.e. created by the embedder using
  // BrowserPpapiHost::CreateExternalPluginProcess.
  bool external_plugin_;

  scoped_refptr<SSLContextHelper> ssl_context_helper_;

  // Tracks all PP_Instances in this plugin and associated renderer-related
  // data.
  typedef std::map<PP_Instance, PepperRendererInstanceData> InstanceMap;
  InstanceMap instance_map_;

  scoped_refptr<HostMessageFilter> message_filter_;

  BrowserPpapiHost::OnKeepaliveCallback on_keepalive_callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPpapiHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_IMPL_H_
