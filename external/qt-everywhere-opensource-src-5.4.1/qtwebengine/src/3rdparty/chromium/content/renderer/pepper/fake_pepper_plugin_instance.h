// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_FAKE_PEPPER_PLUGIN_INSTANCE_H_
#define CONTENT_RENDERER_PEPPER_FAKE_PEPPER_PLUGIN_INSTANCE_H_

#include "content/public/renderer/pepper_plugin_instance.h"
#include "url/gurl.h"

namespace content {

class FakePepperPluginInstance : public PepperPluginInstance {
 public:
  virtual ~FakePepperPluginInstance();

  // PepperPluginInstance overrides.
  virtual content::RenderView* GetRenderView() OVERRIDE;
  virtual blink::WebPluginContainer* GetContainer() OVERRIDE;
  virtual v8::Isolate* GetIsolate() const OVERRIDE;
  virtual ppapi::VarTracker* GetVarTracker() OVERRIDE;
  virtual const GURL& GetPluginURL() OVERRIDE;
  virtual base::FilePath GetModulePath() OVERRIDE;
  virtual PP_Resource CreateImage(gfx::ImageSkia* source_image,
                                  float scale) OVERRIDE;
  virtual PP_ExternalPluginResult SwitchToOutOfProcessProxy(
      const base::FilePath& file_path,
      ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id) OVERRIDE;
  virtual void SetAlwaysOnTop(bool on_top) OVERRIDE;
  virtual bool IsFullPagePlugin() OVERRIDE;
  virtual bool FlashSetFullscreen(bool fullscreen, bool delay_report) OVERRIDE;
  virtual bool IsRectTopmost(const gfx::Rect& rect) OVERRIDE;
  virtual int32_t Navigate(const ppapi::URLRequestInfoData& request,
                           const char* target,
                           bool from_user_action) OVERRIDE;
  virtual int MakePendingFileRefRendererHost(const base::FilePath& path)
      OVERRIDE;
  virtual void SetEmbedProperty(PP_Var key, PP_Var value) OVERRIDE;
  virtual void SetSelectedText(const base::string16& selected_text) OVERRIDE;
  virtual void SetLinkUnderCursor(const std::string& url) OVERRIDE;
  virtual void SetTextInputType(ui::TextInputType type) OVERRIDE;
  virtual void PostMessageToJavaScript(PP_Var message) OVERRIDE;

 private:
  GURL gurl_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_FAKE_PEPPER_PLUGIN_INSTANCE_H_
