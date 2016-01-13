// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_webplugin_impl.h"

#include <cmath>

#include "base/debug/crash_logging.h"
#include "base/message_loop/message_loop.h"
#include "content/public/common/page_zoom.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/pepper/message_channel.h"
#include "content/renderer/pepper/npobject_var.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/render_frame_impl.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebPrintParams.h"
#include "third_party/WebKit/public/web/WebPrintScalingOption.h"
#include "url/gurl.h"

using ppapi::NPObjectVar;
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

PepperWebPluginImpl::PepperWebPluginImpl(PluginModule* plugin_module,
                                         const WebPluginParams& params,
                                         RenderFrameImpl* render_frame)
    : init_data_(new InitData()),
      full_frame_(params.loadManually),
      instance_object_(PP_MakeUndefined()),
      container_(NULL) {
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
}

PepperWebPluginImpl::~PepperWebPluginImpl() {}

blink::WebPluginContainer* PepperWebPluginImpl::container() const {
  return container_;
}

bool PepperWebPluginImpl::initialize(WebPluginContainer* container) {
  // The plugin delegate may have gone away.
  instance_ = init_data_->module->CreateInstance(
      init_data_->render_frame, container, init_data_->url);
  if (!instance_.get())
    return false;

  // Enable script objects for this plugin.
  container->allowScriptObjects();

  bool success = instance_->Initialize(
      init_data_->arg_names, init_data_->arg_values, full_frame_);
  if (!success) {
    instance_->Delete();
    instance_ = NULL;

    blink::WebPlugin* replacement_plugin =
        GetContentClient()->renderer()->CreatePluginReplacement(
            init_data_->render_frame, init_data_->module->path());
    if (!replacement_plugin || !replacement_plugin->initialize(container))
      return false;

    container->setPlugin(replacement_plugin);
    return true;
  }

  init_data_.reset();
  container_ = container;
  return true;
}

void PepperWebPluginImpl::destroy() {
  // Tell |container_| to clear references to this plugin's script objects.
  if (container_)
    container_->clearScriptObjects();

  if (instance_.get()) {
    ppapi::PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(instance_object_);
    instance_object_ = PP_MakeUndefined();
    instance_->Delete();
    instance_ = NULL;
  }

  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

NPObject* PepperWebPluginImpl::scriptableObject() {
  // Call through the plugin to get its instance object. The plugin should pass
  // us a reference which we release in destroy().
  if (instance_object_.type == PP_VARTYPE_UNDEFINED)
    instance_object_ = instance_->GetInstanceObject();
  // GetInstanceObject talked to the plugin which may have removed the instance
  // from the DOM, in which case instance_ would be NULL now.
  if (!instance_.get())
    return NULL;

  scoped_refptr<NPObjectVar> object(NPObjectVar::FromPPVar(instance_object_));
  // If there's an InstanceObject, tell the Instance's MessageChannel to pass
  // any non-postMessage calls to it.
  if (object.get()) {
    instance_->message_channel().SetPassthroughObject(object->np_object());
  }
  NPObject* message_channel_np_object(instance_->message_channel().np_object());
  // The object is expected to be retained before it is returned.
  blink::WebBindings::retainObject(message_channel_np_object);
  return message_channel_np_object;
}

NPP PepperWebPluginImpl::pluginNPP() { return instance_->instanceNPP(); }

bool PepperWebPluginImpl::getFormValue(WebString& value) { return false; }

void PepperWebPluginImpl::paint(WebCanvas* canvas, const WebRect& rect) {
  if (!instance_->FlashIsFullscreenOrPending())
    instance_->Paint(canvas, plugin_rect_, rect);
}

void PepperWebPluginImpl::updateGeometry(
    const WebRect& window_rect,
    const WebRect& clip_rect,
    const WebVector<WebRect>& cut_outs_rects,
    bool is_visible) {
  plugin_rect_ = window_rect;
  if (!instance_->FlashIsFullscreenOrPending()) {
    std::vector<gfx::Rect> cut_outs;
    for (size_t i = 0; i < cut_outs_rects.size(); ++i)
      cut_outs.push_back(cut_outs_rects[i]);
    instance_->ViewChanged(plugin_rect_, clip_rect, cut_outs);
  }
}

void PepperWebPluginImpl::updateFocus(bool focused) {
  instance_->SetWebKitFocus(focused);
}

void PepperWebPluginImpl::updateVisibility(bool visible) {}

bool PepperWebPluginImpl::acceptsInputEvents() { return true; }

bool PepperWebPluginImpl::handleInputEvent(const blink::WebInputEvent& event,
                                           blink::WebCursorInfo& cursor_info) {
  if (instance_->FlashIsFullscreenOrPending())
    return false;
  return instance_->HandleInputEvent(event, &cursor_info);
}

void PepperWebPluginImpl::didReceiveResponse(
    const blink::WebURLResponse& response) {
  DCHECK(!instance_->document_loader());
  instance_->HandleDocumentLoad(response);
}

void PepperWebPluginImpl::didReceiveData(const char* data, int data_length) {
  blink::WebURLLoaderClient* document_loader = instance_->document_loader();
  if (document_loader)
    document_loader->didReceiveData(NULL, data, data_length, 0);
}

void PepperWebPluginImpl::didFinishLoading() {
  blink::WebURLLoaderClient* document_loader = instance_->document_loader();
  if (document_loader)
    document_loader->didFinishLoading(
        NULL, 0.0, blink::WebURLLoaderClient::kUnknownEncodedDataLength);
}

void PepperWebPluginImpl::didFailLoading(const blink::WebURLError& error) {
  blink::WebURLLoaderClient* document_loader = instance_->document_loader();
  if (document_loader)
    document_loader->didFail(NULL, error);
}

void PepperWebPluginImpl::didFinishLoadingFrameRequest(const blink::WebURL& url,
                                                       void* notify_data) {}

void PepperWebPluginImpl::didFailLoadingFrameRequest(
    const blink::WebURL& url,
    void* notify_data,
    const blink::WebURLError& error) {}

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

void PepperWebPluginImpl::setZoomLevel(double level, bool text_only) {
  instance_->Zoom(content::ZoomLevelToZoomFactor(level), text_only);
}

bool PepperWebPluginImpl::startFind(const blink::WebString& search_text,
                                    bool case_sensitive,
                                    int identifier) {
  return instance_->StartFind(search_text, case_sensitive, identifier);
}

void PepperWebPluginImpl::selectFindResult(bool forward) {
  instance_->SelectFindResult(forward);
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

bool PepperWebPluginImpl::printPage(int page_number, blink::WebCanvas* canvas) {
  return instance_->PrintPage(page_number, canvas);
}

void PepperWebPluginImpl::printEnd() { return instance_->PrintEnd(); }

bool PepperWebPluginImpl::canRotateView() { return instance_->CanRotateView(); }

void PepperWebPluginImpl::rotateView(RotationType type) {
  instance_->RotateView(type);
}

bool PepperWebPluginImpl::isPlaceholder() { return false; }

}  // namespace content
