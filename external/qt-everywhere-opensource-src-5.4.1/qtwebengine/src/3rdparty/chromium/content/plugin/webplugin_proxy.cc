// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/plugin/webplugin_proxy.h"

#include "build/build_config.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/shared_memory.h"
#include "build/build_config.h"
#include "content/child/npapi/npobject_proxy.h"
#include "content/child/npapi/npobject_util.h"
#include "content/child/npapi/webplugin_delegate_impl.h"
#include "content/child/npapi/webplugin_resource_client.h"
#include "content/child/plugin_messages.h"
#include "content/plugin/plugin_channel.h"
#include "content/plugin/plugin_thread.h"
#include "content/public/common/content_client.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/platform_device.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/canvas.h"
#include "url/url_constants.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "content/plugin/webplugin_accelerated_surface_proxy_mac.h"
#endif

#if defined(OS_WIN)
#include "content/common/plugin_process_messages.h"
#include "content/public/common/sandbox_init.h"
#endif

using blink::WebBindings;

namespace content {

WebPluginProxy::SharedTransportDIB::SharedTransportDIB(TransportDIB* dib)
    : dib_(dib) {
}

WebPluginProxy::SharedTransportDIB::~SharedTransportDIB() {
}

WebPluginProxy::WebPluginProxy(
    PluginChannel* channel,
    int route_id,
    const GURL& page_url,
    int host_render_view_routing_id)
    : channel_(channel),
      route_id_(route_id),
      window_npobject_(NULL),
      plugin_element_(NULL),
      delegate_(NULL),
      waiting_for_paint_(false),
      page_url_(page_url),
      windowless_buffer_index_(0),
      host_render_view_routing_id_(host_render_view_routing_id),
      weak_factory_(this) {
}

WebPluginProxy::~WebPluginProxy() {
#if defined(OS_MACOSX)
  // Destroy the surface early, since it may send messages during cleanup.
  if (accelerated_surface_)
    accelerated_surface_.reset();
#endif

  if (plugin_element_)
    WebBindings::releaseObject(plugin_element_);
  if (window_npobject_)
    WebBindings::releaseObject(window_npobject_);
}

bool WebPluginProxy::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

void WebPluginProxy::SetWindow(gfx::PluginWindowHandle window) {
  Send(new PluginHostMsg_SetWindow(route_id_, window));
}

void WebPluginProxy::SetAcceptsInputEvents(bool accepts) {
  NOTREACHED();
}

void WebPluginProxy::WillDestroyWindow(gfx::PluginWindowHandle window) {
#if defined(OS_WIN)
  PluginThread::current()->Send(
      new PluginProcessHostMsg_PluginWindowDestroyed(
          window, ::GetParent(window)));
#else
  NOTIMPLEMENTED();
#endif
}

#if defined(OS_WIN)
void WebPluginProxy::SetWindowlessData(
    HANDLE pump_messages_event, gfx::NativeViewId dummy_activation_window) {
  HANDLE pump_messages_event_for_renderer = NULL;
  BrokerDuplicateHandle(pump_messages_event, channel_->peer_pid(),
                                 &pump_messages_event_for_renderer,
                                 SYNCHRONIZE | EVENT_MODIFY_STATE, 0);
  DCHECK(pump_messages_event_for_renderer);
  Send(new PluginHostMsg_SetWindowlessData(
      route_id_, pump_messages_event_for_renderer, dummy_activation_window));
}
#endif

void WebPluginProxy::CancelResource(unsigned long id) {
  Send(new PluginHostMsg_CancelResource(route_id_, id));
  resource_clients_.erase(id);
}

void WebPluginProxy::Invalidate() {
  gfx::Rect rect(0, 0,
                 delegate_->GetRect().width(),
                 delegate_->GetRect().height());
  InvalidateRect(rect);
}

void WebPluginProxy::InvalidateRect(const gfx::Rect& rect) {
#if defined(OS_MACOSX)
  // If this is a Core Animation plugin, all we need to do is inform the
  // delegate.
  if (!windowless_context()) {
    delegate_->PluginDidInvalidate();
    return;
  }

  // Some plugins will send invalidates larger than their own rect when
  // offscreen, so constrain invalidates to the plugin rect.
  gfx::Rect plugin_rect = delegate_->GetRect();
  plugin_rect.set_origin(gfx::Point(0, 0));
  plugin_rect.Intersect(rect);
  const gfx::Rect invalidate_rect(plugin_rect);
#else
  const gfx::Rect invalidate_rect(rect);
#endif
  damaged_rect_.Union(invalidate_rect);
  // Ignore NPN_InvalidateRect calls with empty rects.  Also don't send an
  // invalidate if it's outside the clipping region, since if we did it won't
  // lead to a paint and we'll be stuck waiting forever for a DidPaint response.
  //
  // TODO(piman): There is a race condition here, because this test assumes
  // that when the paint actually occurs, the clip rect will not have changed.
  // This is not true because scrolling (or window resize) could occur and be
  // handled by the renderer before it receives the InvalidateRect message,
  // changing the clip rect and then not painting.
  if (damaged_rect_.IsEmpty() ||
      !delegate_->GetClipRect().Intersects(damaged_rect_))
    return;

  // Only send a single InvalidateRect message at a time.  From DidPaint we
  // will dispatch an additional InvalidateRect message if necessary.
  if (!waiting_for_paint_) {
    waiting_for_paint_ = true;
    // Invalidates caused by calls to NPN_InvalidateRect/NPN_InvalidateRgn
    // need to be painted asynchronously as per the NPAPI spec.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&WebPluginProxy::OnPaint,
                   weak_factory_.GetWeakPtr(),
                   damaged_rect_));
    damaged_rect_ = gfx::Rect();
  }
}

NPObject* WebPluginProxy::GetWindowScriptNPObject() {
  if (window_npobject_)
    return window_npobject_;

  int npobject_route_id = channel_->GenerateRouteID();
  bool success = false;
  Send(new PluginHostMsg_GetWindowScriptNPObject(
      route_id_, npobject_route_id, &success));
  if (!success)
    return NULL;

  // PluginChannel creates a dummy owner identifier for unknown owners, so
  // use that.
  NPP owner = channel_->GetExistingNPObjectOwner(MSG_ROUTING_NONE);

  window_npobject_ = NPObjectProxy::Create(channel_.get(),
                                           npobject_route_id,
                                           host_render_view_routing_id_,
                                           page_url_,
                                           owner);

  return window_npobject_;
}

NPObject* WebPluginProxy::GetPluginElement() {
  if (plugin_element_)
    return plugin_element_;

  int npobject_route_id = channel_->GenerateRouteID();
  bool success = false;
  Send(new PluginHostMsg_GetPluginElement(route_id_, npobject_route_id,
                                          &success));
  if (!success)
    return NULL;

  // PluginChannel creates a dummy owner identifier for unknown owners, so
  // use that.
  NPP owner = channel_->GetExistingNPObjectOwner(MSG_ROUTING_NONE);

  plugin_element_ = NPObjectProxy::Create(channel_.get(),
                                          npobject_route_id,
                                          host_render_view_routing_id_,
                                          page_url_,
                                          owner);

  return plugin_element_;
}

bool WebPluginProxy::FindProxyForUrl(const GURL& url, std::string* proxy_list) {
  bool result = false;
  Send(new PluginHostMsg_ResolveProxy(route_id_, url, &result, proxy_list));
  return result;
}

void WebPluginProxy::SetCookie(const GURL& url,
                               const GURL& first_party_for_cookies,
                               const std::string& cookie) {
  Send(new PluginHostMsg_SetCookie(route_id_, url,
                                   first_party_for_cookies, cookie));
}

std::string WebPluginProxy::GetCookies(const GURL& url,
                                       const GURL& first_party_for_cookies) {
  std::string cookies;
  Send(new PluginHostMsg_GetCookies(route_id_, url,
                                    first_party_for_cookies, &cookies));

  return cookies;
}

WebPluginResourceClient* WebPluginProxy::GetResourceClient(int id) {
  ResourceClientMap::iterator iterator = resource_clients_.find(id);
  // The IPC messages which deal with streams are now asynchronous. It is
  // now possible to receive stream messages from the renderer for streams
  // which may have been cancelled by the plugin.
  if (iterator == resource_clients_.end()) {
    return NULL;
  }

  return iterator->second;
}

int WebPluginProxy::GetRendererId() {
  if (channel_.get())
    return channel_->renderer_id();
  return -1;
}

void WebPluginProxy::DidPaint() {
  // If we have an accumulated damaged rect, then check to see if we need to
  // send out another InvalidateRect message.
  waiting_for_paint_ = false;
  if (!damaged_rect_.IsEmpty())
    InvalidateRect(damaged_rect_);
}

void WebPluginProxy::OnResourceCreated(int resource_id,
                                       WebPluginResourceClient* client) {
  DCHECK(resource_clients_.find(resource_id) == resource_clients_.end());
  resource_clients_[resource_id] = client;
}

void WebPluginProxy::HandleURLRequest(const char* url,
                                      const char* method,
                                      const char* target,
                                      const char* buf,
                                      unsigned int len,
                                      int notify_id,
                                      bool popups_allowed,
                                      bool notify_redirects) {
 if (!target && (0 == base::strcasecmp(method, "GET"))) {
    // Please refer to https://bugzilla.mozilla.org/show_bug.cgi?id=366082
    // for more details on this.
    if (delegate_->GetQuirks() &
        WebPluginDelegateImpl::PLUGIN_QUIRK_BLOCK_NONSTANDARD_GETURL_REQUESTS) {
      GURL request_url(url);
      if (!request_url.SchemeIs(url::kHttpScheme) &&
          !request_url.SchemeIs(url::kHttpsScheme) &&
          !request_url.SchemeIs(url::kFtpScheme)) {
        return;
      }
    }
  }

  PluginHostMsg_URLRequest_Params params;
  params.url = url;
  params.method = method;
  if (target)
    params.target = std::string(target);

  if (len) {
    params.buffer.resize(len);
    memcpy(&params.buffer.front(), buf, len);
  }

  params.notify_id = notify_id;
  params.popups_allowed = popups_allowed;
  params.notify_redirects = notify_redirects;

  Send(new PluginHostMsg_URLRequest(route_id_, params));
}

void WebPluginProxy::Paint(const gfx::Rect& rect) {
#if defined(OS_MACOSX)
  if (!windowless_context())
    return;
#else
  if (!windowless_canvas() || !windowless_canvas()->getDevice())
    return;
#endif

  // Clear the damaged area so that if the plugin doesn't paint there we won't
  // end up with the old values.
  gfx::Rect offset_rect = rect;
  offset_rect.Offset(delegate_->GetRect().OffsetFromOrigin());
#if defined(OS_MACOSX)
  CGContextSaveGState(windowless_context());
  // It is possible for windowless_contexts_ to change during plugin painting
  // (since the plugin can make a synchronous call during paint event handling),
  // in which case we don't want to try to restore later. Not an owning ref
  // since owning the ref without owning the shared backing memory doesn't make
  // sense, so this should only be used for pointer comparisons.
  CGContextRef saved_context_weak = windowless_context();
  // We also save the buffer index for the comparison because if we flip buffers
  // but haven't reallocated them then we do need to restore the context because
  // it is going to continue to be used.
  int saved_index = windowless_buffer_index_;

  CGContextClipToRect(windowless_context(), rect.ToCGRect());
  // TODO(caryclark): This is a temporary workaround to allow the Darwin / Skia
  // port to share code with the Darwin / CG port. All ports will eventually use
  // the common code below.
  delegate_->CGPaint(windowless_context(), rect);
  if (windowless_contexts_[saved_index].get() == saved_context_weak)
    CGContextRestoreGState(windowless_contexts_[saved_index]);
#else
  // See above comment about windowless_context_ changing.
  // http::/crbug.com/139462
  skia::RefPtr<skia::PlatformCanvas> saved_canvas = windowless_canvas();

  saved_canvas->save();

  // The given clip rect is relative to the plugin coordinate system.
  SkRect sk_rect = { SkIntToScalar(rect.x()),
                     SkIntToScalar(rect.y()),
                     SkIntToScalar(rect.right()),
                     SkIntToScalar(rect.bottom()) };
  saved_canvas->clipRect(sk_rect);

  // Fill a transparent value so that if the plugin supports transparency that
  // will work.
  saved_canvas->drawColor(SkColorSetARGB(0, 0, 0, 0), SkXfermode::kSrc_Mode);

  // Bring the windowless canvas into the window coordinate system, which is
  // how the plugin expects to draw (since the windowless API was originally
  // designed just for scribbling over the web page).
  saved_canvas->translate(SkIntToScalar(-delegate_->GetRect().x()),
                          SkIntToScalar(-delegate_->GetRect().y()));

  // Before we send the invalidate, paint so that renderer uses the updated
  // bitmap.
  delegate_->Paint(saved_canvas.get(), offset_rect);

  saved_canvas->restore();
#endif
}

void WebPluginProxy::UpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect,
    const TransportDIB::Handle& windowless_buffer0,
    const TransportDIB::Handle& windowless_buffer1,
    int windowless_buffer_index) {
  gfx::Rect old = delegate_->GetRect();
  gfx::Rect old_clip_rect = delegate_->GetClipRect();

  // Update the buffers before doing anything that could call into plugin code,
  // so that we don't process buffer changes out of order if plugins make
  // synchronous calls that lead to nested UpdateGeometry calls.
  if (TransportDIB::is_valid_handle(windowless_buffer0)) {
    // The plugin's rect changed, so now we have new buffers to draw into.
    SetWindowlessBuffers(windowless_buffer0,
                         windowless_buffer1,
                         window_rect);
  }

  DCHECK(0 <= windowless_buffer_index && windowless_buffer_index <= 1);
  windowless_buffer_index_ = windowless_buffer_index;

#if defined(OS_MACOSX)
  delegate_->UpdateGeometryAndContext(
      window_rect, clip_rect, windowless_context());
#else
  delegate_->UpdateGeometry(window_rect, clip_rect);
#endif

  // Send over any pending invalidates which occured when the plugin was
  // off screen.
  if (delegate_->IsWindowless() && !clip_rect.IsEmpty() &&
      !damaged_rect_.IsEmpty()) {
    InvalidateRect(damaged_rect_);
  }
}

#if defined(OS_WIN)

void WebPluginProxy::CreateCanvasFromHandle(
    const TransportDIB::Handle& dib_handle,
    const gfx::Rect& window_rect,
    skia::RefPtr<skia::PlatformCanvas>* canvas) {
  *canvas = skia::AdoptRef(
      skia::CreatePlatformCanvas(window_rect.width(),
                                 window_rect.height(),
                                 true,
                                 dib_handle,
                                 skia::RETURN_NULL_ON_FAILURE));
  // The canvas does not own the section so we need to close it now.
  CloseHandle(dib_handle);
}

void WebPluginProxy::SetWindowlessBuffers(
    const TransportDIB::Handle& windowless_buffer0,
    const TransportDIB::Handle& windowless_buffer1,
    const gfx::Rect& window_rect) {
  CreateCanvasFromHandle(windowless_buffer0,
                         window_rect,
                         &windowless_canvases_[0]);
  if (!windowless_canvases_[0]) {
    windowless_canvases_[1].clear();
    return;
  }
  CreateCanvasFromHandle(windowless_buffer1,
                         window_rect,
                         &windowless_canvases_[1]);
  if (!windowless_canvases_[1]) {
    windowless_canvases_[0].clear();
    return;
  }
}

#elif defined(OS_MACOSX)

void WebPluginProxy::CreateDIBAndCGContextFromHandle(
    const TransportDIB::Handle& dib_handle,
    const gfx::Rect& window_rect,
    scoped_ptr<TransportDIB>* dib_out,
    base::ScopedCFTypeRef<CGContextRef>* cg_context_out) {
  // Convert the shared memory handle to a handle that works in our process,
  // and then use that to create a CGContextRef.
  TransportDIB* dib = TransportDIB::Map(dib_handle);
  CGContextRef cg_context = NULL;
  if (dib) {
    cg_context = CGBitmapContextCreate(
        dib->memory(),
        window_rect.width(),
        window_rect.height(),
        8,
        4 * window_rect.width(),
        base::mac::GetSystemColorSpace(),
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    CGContextTranslateCTM(cg_context, 0, window_rect.height());
    CGContextScaleCTM(cg_context, 1, -1);
  }
  dib_out->reset(dib);
  cg_context_out->reset(cg_context);
}

void WebPluginProxy::SetWindowlessBuffers(
    const TransportDIB::Handle& windowless_buffer0,
    const TransportDIB::Handle& windowless_buffer1,
    const gfx::Rect& window_rect) {
  CreateDIBAndCGContextFromHandle(windowless_buffer0,
                                  window_rect,
                                  &windowless_dibs_[0],
                                  &windowless_contexts_[0]);
  CreateDIBAndCGContextFromHandle(windowless_buffer1,
                                  window_rect,
                                  &windowless_dibs_[1],
                                  &windowless_contexts_[1]);
}

#else

void WebPluginProxy::SetWindowlessBuffers(
    const TransportDIB::Handle& windowless_buffer0,
    const TransportDIB::Handle& windowless_buffer1,
    const gfx::Rect& window_rect) {
  NOTIMPLEMENTED();
}

#endif

void WebPluginProxy::CancelDocumentLoad() {
  Send(new PluginHostMsg_CancelDocumentLoad(route_id_));
}

void WebPluginProxy::InitiateHTTPRangeRequest(
    const char* url, const char* range_info, int range_request_id) {
  Send(new PluginHostMsg_InitiateHTTPRangeRequest(
      route_id_, url, range_info, range_request_id));
}

void WebPluginProxy::DidStartLoading() {
  Send(new PluginHostMsg_DidStartLoading(route_id_));
}

void WebPluginProxy::DidStopLoading() {
  Send(new PluginHostMsg_DidStopLoading(route_id_));
}

void WebPluginProxy::SetDeferResourceLoading(unsigned long resource_id,
                                             bool defer) {
  Send(new PluginHostMsg_DeferResourceLoading(route_id_, resource_id, defer));
}

#if defined(OS_MACOSX)
void WebPluginProxy::FocusChanged(bool focused) {
  IPC::Message* msg = new PluginHostMsg_FocusChanged(route_id_, focused);
  Send(msg);
}

void WebPluginProxy::StartIme() {
  IPC::Message* msg = new PluginHostMsg_StartIme(route_id_);
  // This message can be sent during event-handling, and needs to be delivered
  // within that context.
  msg->set_unblock(true);
  Send(msg);
}

WebPluginAcceleratedSurface* WebPluginProxy::GetAcceleratedSurface(
    gfx::GpuPreference gpu_preference) {
  if (!accelerated_surface_)
    accelerated_surface_.reset(
        WebPluginAcceleratedSurfaceProxy::Create(this, gpu_preference));
  return accelerated_surface_.get();
}

void WebPluginProxy::AcceleratedPluginEnabledRendering() {
  Send(new PluginHostMsg_AcceleratedPluginEnabledRendering(route_id_));
}

void WebPluginProxy::AcceleratedPluginAllocatedIOSurface(int32 width,
                                                         int32 height,
                                                         uint32 surface_id) {
  Send(new PluginHostMsg_AcceleratedPluginAllocatedIOSurface(
      route_id_, width, height, surface_id));
}

void WebPluginProxy::AcceleratedPluginSwappedIOSurface() {
  Send(new PluginHostMsg_AcceleratedPluginSwappedIOSurface(
      route_id_));
}
#endif

void WebPluginProxy::OnPaint(const gfx::Rect& damaged_rect) {
  GetContentClient()->SetActiveURL(page_url_);

  Paint(damaged_rect);
  Send(new PluginHostMsg_InvalidateRect(route_id_, damaged_rect));
}

bool WebPluginProxy::IsOffTheRecord() {
  return channel_->incognito();
}

void WebPluginProxy::ResourceClientDeleted(
    WebPluginResourceClient* resource_client) {
  // resource_client->ResourceId() is 0 at this point, so can't use it as an
  // index into the map.
  ResourceClientMap::iterator index = resource_clients_.begin();
  while (index != resource_clients_.end()) {
    WebPluginResourceClient* client = (*index).second;
    if (client == resource_client) {
      resource_clients_.erase(index);
      return;
    } else {
      index++;
    }
  }
}

void WebPluginProxy::URLRedirectResponse(bool allow, int resource_id) {
  Send(new PluginHostMsg_URLRedirectResponse(route_id_, allow, resource_id));
}

bool WebPluginProxy::CheckIfRunInsecureContent(const GURL& url) {
  bool result = true;
  Send(new PluginHostMsg_CheckIfRunInsecureContent(
      route_id_, url, &result));
  return result;
}

#if defined(OS_WIN) && !defined(USE_AURA)
void WebPluginProxy::UpdateIMEStatus() {
  // Retrieve the IME status from a plug-in and send it to a renderer process
  // when the plug-in has updated it.
  int input_type;
  gfx::Rect caret_rect;
  if (!delegate_->GetIMEStatus(&input_type, &caret_rect))
    return;

  Send(new PluginHostMsg_NotifyIMEStatus(route_id_, input_type, caret_rect));
}
#endif

}  // namespace content
