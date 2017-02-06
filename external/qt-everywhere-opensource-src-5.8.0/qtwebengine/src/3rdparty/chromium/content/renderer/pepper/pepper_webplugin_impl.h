// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_WEBPLUGIN_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_WEBPLUGIN_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "ppapi/c/pp_var.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "ui/gfx/geometry/rect.h"

struct _NPP;

namespace blink {
struct WebPluginParams;
struct WebPrintParams;
}

namespace content {

class PepperPluginInstanceImpl;
class PluginInstanceThrottlerImpl;
class PluginModule;
class PPB_URLLoader_Impl;
class RenderFrameImpl;

class PepperWebPluginImpl : public blink::WebPlugin {
 public:
  PepperWebPluginImpl(PluginModule* module,
                      const blink::WebPluginParams& params,
                      RenderFrameImpl* render_frame,
                      std::unique_ptr<PluginInstanceThrottlerImpl> throttler);

  PepperPluginInstanceImpl* instance() { return instance_.get(); }

  // blink::WebPlugin implementation.
  blink::WebPluginContainer* container() const override;
  bool initialize(blink::WebPluginContainer* container) override;
  void destroy() override;
  v8::Local<v8::Object> v8ScriptableObject(v8::Isolate* isolate) override;
  void updateAllLifecyclePhases() override {}
  void paint(blink::WebCanvas* canvas, const blink::WebRect& rect) override;
  void updateGeometry(const blink::WebRect& window_rect,
                      const blink::WebRect& clip_rect,
                      const blink::WebRect& unobscured_rect,
                      const blink::WebVector<blink::WebRect>& cut_outs_rects,
                      bool is_visible) override;
  void updateFocus(bool focused, blink::WebFocusType focus_type) override;
  void updateVisibility(bool visible) override;
  blink::WebInputEventResult handleInputEvent(
      const blink::WebInputEvent& event,
      blink::WebCursorInfo& cursor_info) override;
  void didReceiveResponse(const blink::WebURLResponse& response) override;
  void didReceiveData(const char* data, int data_length) override;
  void didFinishLoading() override;
  void didFailLoading(const blink::WebURLError&) override;
  bool hasSelection() const override;
  blink::WebString selectionAsText() const override;
  blink::WebString selectionAsMarkup() const override;
  blink::WebURL linkAtPosition(const blink::WebPoint& position) const override;
  bool getPrintPresetOptionsFromDocument(
      blink::WebPrintPresetOptions* preset_options) override;
  bool startFind(const blink::WebString& search_text,
                 bool case_sensitive,
                 int identifier) override;
  void selectFindResult(bool forward, int identifier) override;
  void stopFind() override;
  bool supportsPaginatedPrint() override;
  bool isPrintScalingDisabled() override;

  int printBegin(const blink::WebPrintParams& print_params) override;
  void printPage(int page_number, blink::WebCanvas* canvas) override;
  void printEnd() override;

  bool canRotateView() override;
  void rotateView(RotationType type) override;
  bool isPlaceholder() override;

 private:
  friend class base::DeleteHelper<PepperWebPluginImpl>;

  virtual ~PepperWebPluginImpl();
  struct InitData;

  std::unique_ptr<InitData>
      init_data_;  // Cleared upon successful initialization.
  // True if the instance represents the entire document in a frame instead of
  // being an embedded resource.
  bool full_frame_;
  std::unique_ptr<PluginInstanceThrottlerImpl> throttler_;
  scoped_refptr<PepperPluginInstanceImpl> instance_;
  gfx::Rect plugin_rect_;
  PP_Var instance_object_;
  blink::WebPluginContainer* container_;

  // TODO(tommycli): Remove once we fix https://crbug.com/588624.
  bool destroyed_;

  DISALLOW_COPY_AND_ASSIGN(PepperWebPluginImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_WEBPLUGIN_IMPL_H_
