// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_webplugin_impl.h"

#include <stddef.h>
#include <cmath>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/pepper/message_channel.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_instance_throttler_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/v8object_var.h"
#include "content/renderer/render_frame_impl.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebPrintParams.h"
#include "third_party/WebKit/public/web/WebPrintPresetOptions.h"
#include "third_party/WebKit/public/web/WebPrintScalingOption.h"
#include "url/gurl.h"

using ppapi::V8ObjectVar;
using blink::WebCanvas;
using blink::WebPlugin;
using blink::WebPluginContainer;
using blink::WebPluginParams;
using blink::WebPoint;
using blink::WebPrintParams;
using blink::WebRect;
using blink::WebSize;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

struct PepperWebPluginImpl::InitData {
  scoped_refptr<PluginModule> module;
  RenderFrameImpl* render_frame;
  std::vector<std::string> arg_names;
  std::vector<std::string> arg_values;
  GURL url;
};

PepperWebPluginImpl::PepperWebPluginImpl(
    PluginModule* plugin_module,
    const WebPluginParams& params,
    RenderFrameImpl* render_frame,
    std::unique_ptr<PluginInstanceThrottlerImpl> throttler)
    : init_data_(new InitData()),
      full_frame_(params.loadManually),
      throttler_(std::move(throttler)),
      instance_object_(PP_MakeUndefined()),
      container_(nullptr),
      destroyed_(false) {
  DCHECK(plugin_module);
  init_data_->module = plugin_module;
  init_data_->render_frame = render_frame;
  for (size_t i = 0; i < params.attributeNames.size(); ++i) {
    init_data_->arg_names.push_back(params.attributeNames[i].utf8());
    init_data_->arg_values.push_back(params.attributeValues[i].utf8());
  }
  init_data_->url = params.url;

  // Set subresource URL for crash reporting.
  base::debug::SetCrashKeyValue("subresource_url", init_data_->url.spec());

  if (throttler_)
    throttler_->SetWebPlugin(this);
}

PepperWebPluginImpl::~PepperWebPluginImpl() {}

blink::WebPluginContainer* PepperWebPluginImpl::container() const {
  return container_;
}

bool PepperWebPluginImpl::initialize(WebPluginContainer* container) {
  DCHECK(container);
  DCHECK_EQ(this, container->plugin());

  container_ = container;

  // The plugin delegate may have gone away.
  instance_ = init_data_->module->CreateInstance(
      init_data_->render_frame, container, init_data_->url);
  if (!instance_)
    return false;

  if (!instance_->Initialize(init_data_->arg_names, init_data_->arg_values,
                             full_frame_, std::move(throttler_))) {
    // If |container_| is nullptr, this object has already been synchronously
    // destroy()-ed during |instance_|'s Initialize call. In that case, we early
    // exit. We neither create a replacement plugin nor destroy() ourselves.
    if (!container_)
      return false;

    DCHECK(!destroyed_);

    DCHECK(instance_);
    ppapi::PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(instance_object_);
    instance_object_ = PP_MakeUndefined();
    instance_->Delete();
    instance_ = nullptr;

    blink::WebPlugin* replacement_plugin =
        GetContentClient()->renderer()->CreatePluginReplacement(
            init_data_->render_frame, init_data_->module->path());
    if (!replacement_plugin)
      return false;

    // The replacement plugin, if it exists, must never fail to initialize.
    container->setPlugin(replacement_plugin);
    CHECK(replacement_plugin->initialize(container));

    DCHECK(container->plugin() == replacement_plugin);
    DCHECK(replacement_plugin->container() == container);

    // Since the container now owns the replacement plugin instead of this
    // object, we must schedule ourselves for deletion.
    destroy();

    return true;
  }

  init_data_.reset();
  return true;
}

void PepperWebPluginImpl::destroy() {
  // TODO(tommycli): Remove once we fix https://crbug.com/588624.
  CHECK(!destroyed_);
  destroyed_ = true;

  container_ = nullptr;

  if (instance_) {
    ppapi::PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(instance_object_);
    instance_object_ = PP_MakeUndefined();
    instance_->Delete();
    instance_ = nullptr;
  }

  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

v8::Local<v8::Object> PepperWebPluginImpl::v8ScriptableObject(
      v8::Isolate* isolate) {
  // Re-entrancy may cause JS to try to execute script on the plugin before it
  // is fully initialized. See e.g. crbug.com/503401.
  if (!instance_)
    return v8::Local<v8::Object>();
  // Call through the plugin to get its instance object. The plugin should pass
  // us a reference which we release in destroy().
  if (instance_object_.type == PP_VARTYPE_UNDEFINED)
    instance_object_ = instance_->GetInstanceObject(isolate);
  // GetInstanceObject talked to the plugin which may have removed the instance
  // from the DOM, in which case instance_ would be nullptr now.
  if (!instance_)
    return v8::Local<v8::Object>();

  scoped_refptr<V8ObjectVar> object_var(
      V8ObjectVar::FromPPVar(instance_object_));
  // If there's an InstanceObject, tell the Instance's MessageChannel to pass
  // any non-postMessage calls to it.
  if (object_var) {
    MessageChannel* message_channel = instance_->message_channel();
    if (message_channel)
      message_channel->SetPassthroughObject(object_var->GetHandle());
  }

  v8::Local<v8::Object> result = instance_->GetMessageChannelObject();
  return result;
}

void PepperWebPluginImpl::paint(WebCanvas* canvas, const WebRect& rect) {
  if (!instance_->FlashIsFullscreenOrPending())
    instance_->Paint(canvas, plugin_rect_, rect);
}

void PepperWebPluginImpl::updateGeometry(
    const WebRect& window_rect,
    const WebRect& clip_rect,
    const WebRect& unobscured_rect,
    const WebVector<WebRect>& cut_outs_rects,
    bool is_visible) {
  plugin_rect_ = window_rect;
  if (instance_ && !instance_->FlashIsFullscreenOrPending())
    instance_->ViewChanged(plugin_rect_, clip_rect, unobscured_rect);
}

void PepperWebPluginImpl::updateFocus(bool focused,
                                      blink::WebFocusType focus_type) {
  instance_->SetWebKitFocus(focused);
}

void PepperWebPluginImpl::updateVisibility(bool visible) {}

blink::WebInputEventResult PepperWebPluginImpl::handleInputEvent(
    const blink::WebInputEvent& event,
    blink::WebCursorInfo& cursor_info) {
  if (instance_->FlashIsFullscreenOrPending())
    return blink::WebInputEventResult::NotHandled;
  return instance_->HandleInputEvent(event, &cursor_info)
             ? blink::WebInputEventResult::HandledApplication
             : blink::WebInputEventResult::NotHandled;
}

void PepperWebPluginImpl::didReceiveResponse(
    const blink::WebURLResponse& response) {
  DCHECK(!instance_->document_loader());
  instance_->HandleDocumentLoad(response);
}

void PepperWebPluginImpl::didReceiveData(const char* data, int data_length) {
  blink::WebURLLoaderClient* document_loader = instance_->document_loader();
  if (document_loader)
    document_loader->didReceiveData(nullptr, data, data_length, 0);
}

void PepperWebPluginImpl::didFinishLoading() {
  blink::WebURLLoaderClient* document_loader = instance_->document_loader();
  if (document_loader)
    document_loader->didFinishLoading(
        nullptr, 0.0, blink::WebURLLoaderClient::kUnknownEncodedDataLength);
}

void PepperWebPluginImpl::didFailLoading(const blink::WebURLError& error) {
  blink::WebURLLoaderClient* document_loader = instance_->document_loader();
  if (document_loader)
    document_loader->didFail(nullptr, error);
}

bool PepperWebPluginImpl::hasSelection() const {
  return !selectionAsText().isEmpty();
}

WebString PepperWebPluginImpl::selectionAsText() const {
  return instance_->GetSelectedText(false);
}

WebString PepperWebPluginImpl::selectionAsMarkup() const {
  return instance_->GetSelectedText(true);
}

WebURL PepperWebPluginImpl::linkAtPosition(const WebPoint& position) const {
  return GURL(instance_->GetLinkAtPosition(position));
}

bool PepperWebPluginImpl::startFind(const blink::WebString& search_text,
                                    bool case_sensitive,
                                    int identifier) {
  return instance_->StartFind(search_text, case_sensitive, identifier);
}

void PepperWebPluginImpl::selectFindResult(bool forward, int identifier) {
  instance_->SelectFindResult(forward, identifier);
}

void PepperWebPluginImpl::stopFind() { instance_->StopFind(); }

bool PepperWebPluginImpl::supportsPaginatedPrint() {
  return instance_->SupportsPrintInterface();
}

bool PepperWebPluginImpl::isPrintScalingDisabled() {
  return instance_->IsPrintScalingDisabled();
}

int PepperWebPluginImpl::printBegin(const WebPrintParams& print_params) {
  return instance_->PrintBegin(print_params);
}

void PepperWebPluginImpl::printPage(int page_number, blink::WebCanvas* canvas) {
  instance_->PrintPage(page_number, canvas);
}

void PepperWebPluginImpl::printEnd() { instance_->PrintEnd(); }

bool PepperWebPluginImpl::getPrintPresetOptionsFromDocument(
    blink::WebPrintPresetOptions* preset_options) {
  return instance_->GetPrintPresetOptionsFromDocument(preset_options);
}

bool PepperWebPluginImpl::canRotateView() { return instance_->CanRotateView(); }

void PepperWebPluginImpl::rotateView(RotationType type) {
  instance_->RotateView(type);
}

bool PepperWebPluginImpl::isPlaceholder() { return false; }

}  // namespace content
