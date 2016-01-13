// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/npapi/webplugin_delegate_proxy.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "content/child/child_process.h"
#include "content/child/npapi/npobject_proxy.h"
#include "content/child/npapi/npobject_stub.h"
#include "content/child/npapi/npobject_util.h"
#include "content/child/npapi/webplugin_resource_client.h"
#include "content/child/plugin_messages.h"
#include "content/common/content_constants_internal.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/npapi/plugin_channel_host.h"
#include "content/renderer/npapi/webplugin_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/sad_plugin.h"
#include "ipc/ipc_channel_handle.h"
#include "net/base/mime_util.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

#if defined(OS_WIN)
#include "content/public/common/sandbox_init.h"
#endif

using blink::WebBindings;
using blink::WebCursorInfo;
using blink::WebDragData;
using blink::WebInputEvent;
using blink::WebString;
using blink::WebView;

namespace content {

namespace {

class ScopedLogLevel {
 public:
  explicit ScopedLogLevel(int level);
  ~ScopedLogLevel();

 private:
  int old_level_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLogLevel);
};

ScopedLogLevel::ScopedLogLevel(int level)
    : old_level_(logging::GetMinLogLevel()) {
  logging::SetMinLogLevel(level);
}

ScopedLogLevel::~ScopedLogLevel() {
  logging::SetMinLogLevel(old_level_);
}

// Proxy for WebPluginResourceClient.  The object owns itself after creation,
// deleting itself after its callback has been called.
class ResourceClientProxy : public WebPluginResourceClient {
 public:
  ResourceClientProxy(PluginChannelHost* channel, int instance_id)
    : channel_(channel), instance_id_(instance_id), resource_id_(0),
      multibyte_response_expected_(false) {
  }

  virtual ~ResourceClientProxy() {
  }

  void Initialize(unsigned long resource_id, const GURL& url, int notify_id) {
    resource_id_ = resource_id;
    channel_->Send(new PluginMsg_HandleURLRequestReply(
        instance_id_, resource_id, url, notify_id));
  }

  void InitializeForSeekableStream(unsigned long resource_id,
                                   int range_request_id) {
    resource_id_ = resource_id;
    multibyte_response_expected_ = true;
    channel_->Send(new PluginMsg_HTTPRangeRequestReply(
        instance_id_, resource_id, range_request_id));
  }

  // PluginResourceClient implementation:
  virtual void WillSendRequest(const GURL& url, int http_status_code) OVERRIDE {
    DCHECK(channel_.get() != NULL);
    channel_->Send(new PluginMsg_WillSendRequest(
        instance_id_, resource_id_, url, http_status_code));
  }

  virtual void DidReceiveResponse(const std::string& mime_type,
                                  const std::string& headers,
                                  uint32 expected_length,
                                  uint32 last_modified,
                                  bool request_is_seekable) OVERRIDE {
    DCHECK(channel_.get() != NULL);
    PluginMsg_DidReceiveResponseParams params;
    params.id = resource_id_;
    params.mime_type = mime_type;
    params.headers = headers;
    params.expected_length = expected_length;
    params.last_modified = last_modified;
    params.request_is_seekable = request_is_seekable;
    // Grab a reference on the underlying channel so it does not get
    // deleted from under us.
    scoped_refptr<PluginChannelHost> channel_ref(channel_);
    channel_->Send(new PluginMsg_DidReceiveResponse(instance_id_, params));
  }

  virtual void DidReceiveData(const char* buffer,
                              int length,
                              int data_offset) OVERRIDE {
    DCHECK(channel_.get() != NULL);
    DCHECK_GT(length, 0);
    std::vector<char> data;
    data.resize(static_cast<size_t>(length));
    memcpy(&data.front(), buffer, length);
    // Grab a reference on the underlying channel so it does not get
    // deleted from under us.
    scoped_refptr<PluginChannelHost> channel_ref(channel_);
    channel_->Send(new PluginMsg_DidReceiveData(instance_id_, resource_id_,
                                                data, data_offset));
  }

  virtual void DidFinishLoading(unsigned long resource_id) OVERRIDE {
    DCHECK(channel_.get() != NULL);
    DCHECK_EQ(resource_id, resource_id_);
    channel_->Send(new PluginMsg_DidFinishLoading(instance_id_, resource_id_));
    channel_ = NULL;
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  virtual void DidFail(unsigned long resource_id) OVERRIDE {
    DCHECK(channel_.get() != NULL);
    DCHECK_EQ(resource_id, resource_id_);
    channel_->Send(new PluginMsg_DidFail(instance_id_, resource_id_));
    channel_ = NULL;
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  virtual bool IsMultiByteResponseExpected() OVERRIDE {
    return multibyte_response_expected_;
  }

  virtual int ResourceId() OVERRIDE {
    return resource_id_;
  }

 private:
  scoped_refptr<PluginChannelHost> channel_;
  int instance_id_;
  unsigned long resource_id_;
  // Set to true if the response expected is a multibyte response.
  // For e.g. response for a HTTP byte range request.
  bool multibyte_response_expected_;
};

}  // namespace

WebPluginDelegateProxy::WebPluginDelegateProxy(
    WebPluginImpl* plugin,
    const std::string& mime_type,
    const base::WeakPtr<RenderViewImpl>& render_view,
    RenderFrameImpl* render_frame)
    : render_view_(render_view),
      render_frame_(render_frame),
      plugin_(plugin),
      uses_shared_bitmaps_(false),
#if defined(OS_MACOSX)
      uses_compositor_(false),
#elif defined(OS_WIN)
      dummy_activation_window_(NULL),
#endif
      window_(gfx::kNullPluginWindow),
      mime_type_(mime_type),
      instance_id_(MSG_ROUTING_NONE),
      npobject_(NULL),
      npp_(new NPP_t),
      sad_plugin_(NULL),
      invalidate_pending_(false),
      transparent_(false),
      front_buffer_index_(0),
      page_url_(render_view_->webview()->mainFrame()->document().url()) {
}

WebPluginDelegateProxy::~WebPluginDelegateProxy() {
  if (npobject_)
    WebBindings::releaseObject(npobject_);
}

WebPluginDelegateProxy::SharedBitmap::SharedBitmap() {}

WebPluginDelegateProxy::SharedBitmap::~SharedBitmap() {}

void WebPluginDelegateProxy::PluginDestroyed() {
#if defined(OS_MACOSX) || defined(OS_WIN)
  // Ensure that the renderer doesn't think the plugin still has focus.
  if (render_view_)
    render_view_->PluginFocusChanged(false, instance_id_);
#endif

#if defined(OS_WIN)
  if (dummy_activation_window_ && render_view_) {
    render_view_->Send(new ViewHostMsg_WindowlessPluginDummyWindowDestroyed(
        render_view_->routing_id(), dummy_activation_window_));
  }
  dummy_activation_window_ = NULL;
#endif

  if (window_)
    WillDestroyWindow();

  if (render_view_.get())
    render_view_->UnregisterPluginDelegate(this);

  if (channel_host_.get()) {
    Send(new PluginMsg_DestroyInstance(instance_id_));

    // Must remove the route after sending the destroy message, rather than
    // before, since RemoveRoute can lead to all the outstanding NPObjects
    // being told the channel went away if this was the last instance.
    channel_host_->RemoveRoute(instance_id_);

    // Remove the mapping between our instance-Id and NPP identifiers, used by
    // the channel to track object ownership, before releasing it.
    channel_host_->RemoveMappingForNPObjectOwner(instance_id_);

    // Release the channel host now. If we are is the last reference to the
    // channel, this avoids a race where this renderer asks a new connection to
    // the same plugin between now and the time 'this' is actually deleted.
    // Destroying the channel host is what releases the channel name -> FD
    // association on POSIX, and if we ask for a new connection before it is
    // released, the plugin will give us a new FD, and we'll assert when trying
    // to associate it with the channel name.
    channel_host_ = NULL;
  }

  plugin_ = NULL;

  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

bool WebPluginDelegateProxy::Initialize(
    const GURL& url,
    const std::vector<std::string>& arg_names,
    const std::vector<std::string>& arg_values,
    bool load_manually) {
  // TODO(shess): Attempt to work around http://crbug.com/97285 and
  // http://crbug.com/141055 by retrying the connection.  Reports seem
  // to indicate that the plugin hasn't crashed, and that the problem
  // is not 100% persistent.
  const size_t kAttempts = 2;

  bool result = false;
  scoped_refptr<PluginChannelHost> channel_host;
  int instance_id = 0;

  for (size_t attempt = 0; !result && attempt < kAttempts; attempt++) {
#if defined(OS_MACOSX)
    // TODO(shess): Debugging for http://crbug.com/97285 .  See comment
    // in plugin_channel_host.cc.
    scoped_ptr<base::AutoReset<bool> > track_nested_removes(
        new base::AutoReset<bool>(PluginChannelHost::GetRemoveTrackingFlag(),
                                  true));
#endif

    IPC::ChannelHandle channel_handle;
    if (!RenderThreadImpl::current()->Send(new FrameHostMsg_OpenChannelToPlugin(
            render_frame_->GetRoutingID(), url, page_url_, mime_type_,
            &channel_handle, &info_))) {
      continue;
    }

    if (channel_handle.name.empty()) {
      // We got an invalid handle.  Either the plugin couldn't be found (which
      // shouldn't happen, since if we got here the plugin should exist) or the
      // plugin crashed on initialization.
      if (!info_.path.empty()) {
        render_view_->main_render_frame()->PluginCrashed(
            info_.path, base::kNullProcessId);
        LOG(ERROR) << "Plug-in crashed on start";

        // Return true so that the plugin widget is created and we can paint the
        // crashed plugin there.
        return true;
      }
      LOG(ERROR) << "Plug-in couldn't be found";
      return false;
    }

    channel_host =
        PluginChannelHost::GetPluginChannelHost(
            channel_handle, ChildProcess::current()->io_message_loop_proxy());
    if (!channel_host.get()) {
      LOG(ERROR) << "Couldn't get PluginChannelHost";
      continue;
    }
#if defined(OS_MACOSX)
    track_nested_removes.reset();
#endif

    {
      // TODO(bauerb): Debugging for http://crbug.com/141055.
      ScopedLogLevel log_level(-2);  // Equivalent to --v=2
      result = channel_host->Send(new PluginMsg_CreateInstance(
          mime_type_, &instance_id));
      if (!result) {
        LOG(ERROR) << "Couldn't send PluginMsg_CreateInstance";
        continue;
      }
    }
  }

  // Failed too often, give up.
  if (!result)
    return false;

  channel_host_ = channel_host;
  instance_id_ = instance_id;

  channel_host_->AddRoute(instance_id_, this, NULL);

  // Inform the channel of the mapping between our instance-Id and dummy NPP
  // identifier, for use in object ownership tracking.
  channel_host_->AddMappingForNPObjectOwner(instance_id_, GetPluginNPP());

  // Now tell the PluginInstance in the plugin process to initialize.
  PluginMsg_Init_Params params;
  params.url = url;
  params.page_url = page_url_;
  params.arg_names = arg_names;
  params.arg_values = arg_values;
  params.host_render_view_routing_id = render_view_->routing_id();
  params.load_manually = load_manually;

  result = false;
  Send(new PluginMsg_Init(instance_id_, params, &transparent_, &result));

  if (!result)
    LOG(WARNING) << "PluginMsg_Init returned false";

  render_view_->RegisterPluginDelegate(this);

  return result;
}

bool WebPluginDelegateProxy::Send(IPC::Message* msg) {
  if (!channel_host_.get()) {
    DLOG(WARNING) << "dropping message because channel host is null";
    delete msg;
    return false;
  }

  return channel_host_->Send(msg);
}

void WebPluginDelegateProxy::SendJavaScriptStream(const GURL& url,
                                                  const std::string& result,
                                                  bool success,
                                                  int notify_id) {
  Send(new PluginMsg_SendJavaScriptStream(
      instance_id_, url, result, success, notify_id));
}

void WebPluginDelegateProxy::DidReceiveManualResponse(
    const GURL& url, const std::string& mime_type,
    const std::string& headers, uint32 expected_length,
    uint32 last_modified) {
  PluginMsg_DidReceiveResponseParams params;
  params.id = 0;
  params.mime_type = mime_type;
  params.headers = headers;
  params.expected_length = expected_length;
  params.last_modified = last_modified;
  Send(new PluginMsg_DidReceiveManualResponse(instance_id_, url, params));
}

void WebPluginDelegateProxy::DidReceiveManualData(const char* buffer,
                                                  int length) {
  DCHECK_GT(length, 0);
  std::vector<char> data;
  data.resize(static_cast<size_t>(length));
  memcpy(&data.front(), buffer, length);
  Send(new PluginMsg_DidReceiveManualData(instance_id_, data));
}

void WebPluginDelegateProxy::DidFinishManualLoading() {
  Send(new PluginMsg_DidFinishManualLoading(instance_id_));
}

void WebPluginDelegateProxy::DidManualLoadFail() {
  Send(new PluginMsg_DidManualLoadFail(instance_id_));
}

bool WebPluginDelegateProxy::OnMessageReceived(const IPC::Message& msg) {
  GetContentClient()->SetActiveURL(page_url_);

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebPluginDelegateProxy, msg)
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetWindow, OnSetWindow)
    IPC_MESSAGE_HANDLER(PluginHostMsg_CancelResource, OnCancelResource)
    IPC_MESSAGE_HANDLER(PluginHostMsg_InvalidateRect, OnInvalidateRect)
    IPC_MESSAGE_HANDLER(PluginHostMsg_GetWindowScriptNPObject,
                        OnGetWindowScriptNPObject)
    IPC_MESSAGE_HANDLER(PluginHostMsg_GetPluginElement, OnGetPluginElement)
    IPC_MESSAGE_HANDLER(PluginHostMsg_ResolveProxy, OnResolveProxy)
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetCookie, OnSetCookie)
    IPC_MESSAGE_HANDLER(PluginHostMsg_GetCookies, OnGetCookies)
    IPC_MESSAGE_HANDLER(PluginHostMsg_URLRequest, OnHandleURLRequest)
    IPC_MESSAGE_HANDLER(PluginHostMsg_CancelDocumentLoad, OnCancelDocumentLoad)
    IPC_MESSAGE_HANDLER(PluginHostMsg_InitiateHTTPRangeRequest,
                        OnInitiateHTTPRangeRequest)
    IPC_MESSAGE_HANDLER(PluginHostMsg_DidStartLoading, OnDidStartLoading)
    IPC_MESSAGE_HANDLER(PluginHostMsg_DidStopLoading, OnDidStopLoading)
    IPC_MESSAGE_HANDLER(PluginHostMsg_DeferResourceLoading,
                        OnDeferResourceLoading)
    IPC_MESSAGE_HANDLER(PluginHostMsg_URLRedirectResponse,
                        OnURLRedirectResponse)
    IPC_MESSAGE_HANDLER(PluginHostMsg_CheckIfRunInsecureContent,
                        OnCheckIfRunInsecureContent)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetWindowlessData, OnSetWindowlessData)
    IPC_MESSAGE_HANDLER(PluginHostMsg_NotifyIMEStatus, OnNotifyIMEStatus)
#endif
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(PluginHostMsg_FocusChanged,
                        OnFocusChanged);
    IPC_MESSAGE_HANDLER(PluginHostMsg_StartIme,
                        OnStartIme);
    IPC_MESSAGE_HANDLER(PluginHostMsg_AcceleratedPluginEnabledRendering,
                        OnAcceleratedPluginEnabledRendering)
    IPC_MESSAGE_HANDLER(PluginHostMsg_AcceleratedPluginAllocatedIOSurface,
                        OnAcceleratedPluginAllocatedIOSurface)
    IPC_MESSAGE_HANDLER(PluginHostMsg_AcceleratedPluginSwappedIOSurface,
                        OnAcceleratedPluginSwappedIOSurface)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void WebPluginDelegateProxy::OnChannelError() {
  if (plugin_) {
    if (window_) {
      // The actual WebPluginDelegate never got a chance to tell the WebPlugin
      // its window was going away. Do it on its behalf.
      WillDestroyWindow();
    }
    plugin_->Invalidate();
  }
  if (!channel_host_->expecting_shutdown()) {
    render_view_->main_render_frame()->PluginCrashed(
        info_.path, channel_host_->peer_pid());
  }

#if defined(OS_MACOSX) || defined(OS_WIN)
  // Ensure that the renderer doesn't think the plugin still has focus.
  if (render_view_)
    render_view_->PluginFocusChanged(false, instance_id_);
#endif
}

static void CopyTransportDIBHandleForMessage(
    const TransportDIB::Handle& handle_in,
    TransportDIB::Handle* handle_out,
    base::ProcessId peer_pid) {
#if defined(OS_MACOSX)
  // On Mac, TransportDIB::Handle is typedef'ed to FileDescriptor, and
  // FileDescriptor message fields needs to remain valid until the message is
  // sent or else the sendmsg() call will fail.
  if ((handle_out->fd = HANDLE_EINTR(dup(handle_in.fd))) < 0) {
    PLOG(ERROR) << "dup()";
    return;
  }
  handle_out->auto_close = true;
#elif defined(OS_WIN)
  // On Windows we need to duplicate the handle for the plugin process.
  *handle_out = NULL;
  BrokerDuplicateHandle(handle_in, peer_pid, handle_out,
                        FILE_MAP_READ | FILE_MAP_WRITE, 0);
  DCHECK(*handle_out != NULL);
#else
  // Don't need to do anything special for other platforms.
  *handle_out = handle_in;
#endif
}

void WebPluginDelegateProxy::SendUpdateGeometry(
    bool bitmaps_changed) {
  PluginMsg_UpdateGeometry_Param param;
  param.window_rect = plugin_rect_;
  param.clip_rect = clip_rect_;
  param.windowless_buffer0 = TransportDIB::DefaultHandleValue();
  param.windowless_buffer1 = TransportDIB::DefaultHandleValue();
  param.windowless_buffer_index = back_buffer_index();

#if defined(OS_POSIX)
  // If we're using POSIX mmap'd TransportDIBs, sending the handle across
  // IPC establishes a new mapping rather than just sending a window ID,
  // so only do so if we've actually changed the shared memory bitmaps.
  if (bitmaps_changed)
#endif
  {
    if (transport_stores_[0].dib)
      CopyTransportDIBHandleForMessage(transport_stores_[0].dib->handle(),
                                       &param.windowless_buffer0,
                                       channel_host_->peer_pid());

    if (transport_stores_[1].dib)
      CopyTransportDIBHandleForMessage(transport_stores_[1].dib->handle(),
                                       &param.windowless_buffer1,
                                       channel_host_->peer_pid());
  }

  IPC::Message* msg;
#if defined(OS_WIN)
  if (UseSynchronousGeometryUpdates()) {
    msg = new PluginMsg_UpdateGeometrySync(instance_id_, param);
  } else  // NOLINT
#endif
  {
    msg = new PluginMsg_UpdateGeometry(instance_id_, param);
    msg->set_unblock(true);
  }

  Send(msg);
}

void WebPluginDelegateProxy::UpdateGeometry(const gfx::Rect& window_rect,
                                            const gfx::Rect& clip_rect) {
  // window_rect becomes either a window in native windowing system
  // coords, or a backing buffer.  In either case things will go bad
  // if the rectangle is very large.
  if (window_rect.width() < 0  || window_rect.width() > kMaxPluginSideLength ||
      window_rect.height() < 0 || window_rect.height() > kMaxPluginSideLength ||
      // We know this won't overflow due to above checks.
      static_cast<uint32>(window_rect.width()) *
          static_cast<uint32>(window_rect.height()) > kMaxPluginSize) {
    return;
  }

  plugin_rect_ = window_rect;
  clip_rect_ = clip_rect;

  bool bitmaps_changed = false;

  if (uses_shared_bitmaps_) {
    if (!front_buffer_canvas() ||
        (window_rect.width() != front_buffer_canvas()->getDevice()->width() ||
         window_rect.height() != front_buffer_canvas()->getDevice()->height()))
    {
      bitmaps_changed = true;

      // Create a shared memory section that the plugin paints into
      // asynchronously.
      ResetWindowlessBitmaps();
      if (!window_rect.IsEmpty()) {
        if (!CreateSharedBitmap(&transport_stores_[0].dib,
                                &transport_stores_[0].canvas) ||
            !CreateSharedBitmap(&transport_stores_[1].dib,
                                &transport_stores_[1].canvas)) {
          DCHECK(false);
          ResetWindowlessBitmaps();
          return;
        }
      }
    }
  }

  SendUpdateGeometry(bitmaps_changed);
}

void WebPluginDelegateProxy::ResetWindowlessBitmaps() {
  transport_stores_[0].dib.reset();
  transport_stores_[1].dib.reset();

  transport_stores_[0].canvas.reset();
  transport_stores_[1].canvas.reset();
  transport_store_painted_ = gfx::Rect();
  front_buffer_diff_ = gfx::Rect();
}

static size_t BitmapSizeForPluginRect(const gfx::Rect& plugin_rect) {
  const size_t stride =
      skia::PlatformCanvasStrideForWidth(plugin_rect.width());
  return stride * plugin_rect.height();
}

#if !defined(OS_WIN)
bool WebPluginDelegateProxy::CreateLocalBitmap(
    std::vector<uint8>* memory,
    scoped_ptr<skia::PlatformCanvas>* canvas) {
  const size_t size = BitmapSizeForPluginRect(plugin_rect_);
  memory->resize(size);
  if (memory->size() != size)
    return false;
  canvas->reset(skia::CreatePlatformCanvas(
      plugin_rect_.width(), plugin_rect_.height(), true, &((*memory)[0]),
      skia::CRASH_ON_FAILURE));
  return true;
}
#endif

bool WebPluginDelegateProxy::CreateSharedBitmap(
    scoped_ptr<TransportDIB>* memory,
    scoped_ptr<skia::PlatformCanvas>* canvas) {
  const size_t size = BitmapSizeForPluginRect(plugin_rect_);
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  memory->reset(TransportDIB::Create(size, 0));
  if (!memory->get())
    return false;
#endif
#if defined(OS_POSIX) && !defined(OS_ANDROID)
  TransportDIB::Handle handle;
  IPC::Message* msg = new ViewHostMsg_AllocTransportDIB(size, false, &handle);
  if (!RenderThreadImpl::current()->Send(msg))
    return false;
  if (handle.fd < 0)
    return false;
  memory->reset(TransportDIB::Map(handle));
#else
  static uint32 sequence_number = 0;
  memory->reset(TransportDIB::Create(size, sequence_number++));
#endif
  canvas->reset((*memory)->GetPlatformCanvas(plugin_rect_.width(),
                                             plugin_rect_.height()));
  return !!canvas->get();
}

void WebPluginDelegateProxy::Paint(SkCanvas* canvas,
                                   const gfx::Rect& damaged_rect) {
  // Limit the damaged rectangle to whatever is contained inside the plugin
  // rectangle, as that's the rectangle that we'll actually draw.
  gfx::Rect rect = gfx::IntersectRects(damaged_rect, plugin_rect_);

  // If the plugin is no longer connected (channel crashed) draw a crashed
  // plugin bitmap
  if (!channel_host_.get() || !channel_host_->channel_valid()) {
    // Lazily load the sad plugin image.
    if (!sad_plugin_)
      sad_plugin_ = GetContentClient()->renderer()->GetSadPluginBitmap();
    if (sad_plugin_)
      PaintSadPlugin(canvas, plugin_rect_, *sad_plugin_);
    return;
  }

  if (!uses_shared_bitmaps_)
    return;

  // We got a paint before the plugin's coordinates, so there's no buffer to
  // copy from.
  if (!front_buffer_canvas())
    return;

  gfx::Rect offset_rect = rect;
  offset_rect.Offset(-plugin_rect_.x(), -plugin_rect_.y());

  // transport_store_painted_ is really a bounding box, so in principle this
  // check could falsely indicate that we don't need to paint offset_rect, but
  // in practice it works fine.
  if (!transport_store_painted_.Contains(offset_rect)) {
    Send(new PluginMsg_Paint(instance_id_, offset_rect));
    // Since the plugin is not blocked on the renderer in this context, there is
    // a chance that it will begin repainting the back-buffer before we complete
    // capturing the data. Buffer flipping would increase that risk because
    // geometry update is asynchronous, so we don't want to use buffer flipping
    // here.
    UpdateFrontBuffer(offset_rect, false);
  }

  const SkBitmap& bitmap =
      front_buffer_canvas()->getDevice()->accessBitmap(false);
  SkPaint paint;
  paint.setXfermodeMode(
      transparent_ ? SkXfermode::kSrcATop_Mode : SkXfermode::kSrc_Mode);
  SkIRect src_rect = gfx::RectToSkIRect(offset_rect);
  canvas->drawBitmapRect(bitmap,
                         &src_rect,
                         gfx::RectToSkRect(rect),
                         &paint);

  if (invalidate_pending_) {
    // Only send the PaintAck message if this paint is in response to an
    // invalidate from the plugin, since this message acts as an access token
    // to ensure only one process is using the transport dib at a time.
    invalidate_pending_ = false;
    Send(new PluginMsg_DidPaint(instance_id_));
  }
}

NPObject* WebPluginDelegateProxy::GetPluginScriptableObject() {
  if (npobject_)
    return WebBindings::retainObject(npobject_);

  int route_id = MSG_ROUTING_NONE;
  Send(new PluginMsg_GetPluginScriptableObject(instance_id_, &route_id));
  if (route_id == MSG_ROUTING_NONE)
    return NULL;

  npobject_ = NPObjectProxy::Create(
      channel_host_.get(), route_id, 0, page_url_, GetPluginNPP());

  return WebBindings::retainObject(npobject_);
}

NPP WebPluginDelegateProxy::GetPluginNPP() {
  // Return a dummy NPP for WebKit to use to identify this plugin.
  return npp_.get();
}

bool WebPluginDelegateProxy::GetFormValue(base::string16* value) {
  bool success = false;
  Send(new PluginMsg_GetFormValue(instance_id_, value, &success));
  return success;
}

void WebPluginDelegateProxy::DidFinishLoadWithReason(
    const GURL& url, NPReason reason, int notify_id) {
  Send(new PluginMsg_DidFinishLoadWithReason(
      instance_id_, url, reason, notify_id));
}

void WebPluginDelegateProxy::SetFocus(bool focused) {
  Send(new PluginMsg_SetFocus(instance_id_, focused));
#if defined(OS_WIN)
  if (render_view_)
    render_view_->PluginFocusChanged(focused, instance_id_);
#endif
}

bool WebPluginDelegateProxy::HandleInputEvent(
    const WebInputEvent& event,
    WebCursor::CursorInfo* cursor_info) {
  bool handled;
  WebCursor cursor;
  // A windowless plugin can enter a modal loop in the context of a
  // NPP_HandleEvent call, in which case we need to pump messages to
  // the plugin. We pass of the corresponding event handle to the
  // plugin process, which is set if the plugin does enter a modal loop.
  IPC::SyncMessage* message = new PluginMsg_HandleInputEvent(
      instance_id_, &event, &handled, &cursor);
  message->set_pump_messages_event(modal_loop_pump_messages_event_.get());
  Send(message);
  return handled;
}

int WebPluginDelegateProxy::GetProcessId() {
  return channel_host_->peer_pid();
}

void WebPluginDelegateProxy::SetContentAreaFocus(bool has_focus) {
  IPC::Message* msg = new PluginMsg_SetContentAreaFocus(instance_id_,
                                                        has_focus);
  // Make sure focus events are delivered in the right order relative to
  // sync messages they might interact with (Paint, HandleEvent, etc.).
  msg->set_unblock(true);
  Send(msg);
}

#if defined(OS_WIN)
void WebPluginDelegateProxy::ImeCompositionUpdated(
    const base::string16& text,
    const std::vector<int>& clauses,
    const std::vector<int>& target,
    int cursor_position,
    int plugin_id) {
  // Dispatch the raw IME data if this plug-in is the focused one.
  if (instance_id_ != plugin_id)
    return;

  IPC::Message* msg = new PluginMsg_ImeCompositionUpdated(instance_id_,
      text, clauses, target, cursor_position);
  msg->set_unblock(true);
  Send(msg);
}

void WebPluginDelegateProxy::ImeCompositionCompleted(const base::string16& text,
                                                     int plugin_id) {
  // Dispatch the IME text if this plug-in is the focused one.
  if (instance_id_ != plugin_id)
    return;

  IPC::Message* msg = new PluginMsg_ImeCompositionCompleted(instance_id_, text);
  msg->set_unblock(true);
  Send(msg);
}
#endif

#if defined(OS_MACOSX)
void WebPluginDelegateProxy::SetWindowFocus(bool window_has_focus) {
  IPC::Message* msg = new PluginMsg_SetWindowFocus(instance_id_,
                                                   window_has_focus);
  // Make sure focus events are delivered in the right order relative to
  // sync messages they might interact with (Paint, HandleEvent, etc.).
  msg->set_unblock(true);
  Send(msg);
}

void WebPluginDelegateProxy::SetContainerVisibility(bool is_visible) {
  IPC::Message* msg;
  if (is_visible) {
    gfx::Rect window_frame = render_view_->rootWindowRect();
    gfx::Rect view_frame = render_view_->windowRect();
    blink::WebView* webview = render_view_->webview();
    msg = new PluginMsg_ContainerShown(instance_id_, window_frame, view_frame,
                                       webview && webview->isActive());
  } else {
    msg = new PluginMsg_ContainerHidden(instance_id_);
  }
  // Make sure visibility events are delivered in the right order relative to
  // sync messages they might interact with (Paint, HandleEvent, etc.).
  msg->set_unblock(true);
  Send(msg);
}

void WebPluginDelegateProxy::WindowFrameChanged(gfx::Rect window_frame,
                                                gfx::Rect view_frame) {
  IPC::Message* msg = new PluginMsg_WindowFrameChanged(instance_id_,
                                                       window_frame,
                                                       view_frame);
  // Make sure frame events are delivered in the right order relative to
  // sync messages they might interact with (e.g., HandleEvent).
  msg->set_unblock(true);
  Send(msg);
}
void WebPluginDelegateProxy::ImeCompositionCompleted(const base::string16& text,
                                                     int plugin_id) {
  // If the message isn't intended for this plugin, there's nothing to do.
  if (instance_id_ != plugin_id)
    return;

  IPC::Message* msg = new PluginMsg_ImeCompositionCompleted(instance_id_,
                                                            text);
  // Order relative to other key events is important.
  msg->set_unblock(true);
  Send(msg);
}
#endif  // OS_MACOSX

void WebPluginDelegateProxy::OnSetWindow(gfx::PluginWindowHandle window) {
#if defined(OS_MACOSX)
  uses_shared_bitmaps_ = !window && !uses_compositor_;
#else
  uses_shared_bitmaps_ = !window;
#endif
  window_ = window;
  if (plugin_)
    plugin_->SetWindow(window);
}

void WebPluginDelegateProxy::WillDestroyWindow() {
  DCHECK(window_);
  plugin_->WillDestroyWindow(window_);
  window_ = gfx::kNullPluginWindow;
}

#if defined(OS_WIN)
void WebPluginDelegateProxy::OnSetWindowlessData(
      HANDLE modal_loop_pump_messages_event,
      gfx::NativeViewId dummy_activation_window) {
  DCHECK(modal_loop_pump_messages_event_ == NULL);
  DCHECK(dummy_activation_window_ == NULL);

  dummy_activation_window_ = dummy_activation_window;
  render_view_->Send(new ViewHostMsg_WindowlessPluginDummyWindowCreated(
      render_view_->routing_id(), dummy_activation_window_));

  // Bug 25583: this can be null because some "virus scanners" block the
  // DuplicateHandle call in the plugin process.
  if (!modal_loop_pump_messages_event)
    return;

  modal_loop_pump_messages_event_.reset(
      new base::WaitableEvent(modal_loop_pump_messages_event));
}

void WebPluginDelegateProxy::OnNotifyIMEStatus(int input_type,
                                               const gfx::Rect& caret_rect) {
  if (!render_view_)
    return;

  ViewHostMsg_TextInputState_Params p;
  p.type = static_cast<ui::TextInputType>(input_type);
  p.mode = ui::TEXT_INPUT_MODE_DEFAULT;
  p.can_compose_inline = true;

  render_view_->Send(new ViewHostMsg_TextInputStateChanged(
      render_view_->routing_id(), p));

  ViewHostMsg_SelectionBounds_Params bounds_params;
  bounds_params.anchor_rect = bounds_params.focus_rect = caret_rect;
  bounds_params.anchor_dir = bounds_params.focus_dir =
      blink::WebTextDirectionLeftToRight;
  bounds_params.is_anchor_first = true;
  render_view_->Send(new ViewHostMsg_SelectionBoundsChanged(
      render_view_->routing_id(),
      bounds_params));
}
#endif

void WebPluginDelegateProxy::OnCancelResource(int id) {
  if (plugin_)
    plugin_->CancelResource(id);
}

void WebPluginDelegateProxy::OnInvalidateRect(const gfx::Rect& rect) {
  if (!plugin_)
    return;

  // Clip the invalidation rect to the plugin bounds; the plugin may have been
  // resized since the invalidate message was sent.
  gfx::Rect clipped_rect =
      gfx::IntersectRects(rect, gfx::Rect(plugin_rect_.size()));

  invalidate_pending_ = true;
  // The plugin is blocked on the renderer because the invalidate message it has
  // sent us is synchronous, so we can use buffer flipping here if the caller
  // allows it.
  UpdateFrontBuffer(clipped_rect, true);
  plugin_->InvalidateRect(clipped_rect);
}

void WebPluginDelegateProxy::OnGetWindowScriptNPObject(
    int route_id, bool* success) {
  *success = false;
  NPObject* npobject = NULL;
  if (plugin_)
    npobject = plugin_->GetWindowScriptNPObject();

  if (!npobject)
    return;

  // The stub will delete itself when the proxy tells it that it's released, or
  // otherwise when the channel is closed.
  new NPObjectStub(npobject, channel_host_.get(), route_id, 0, page_url_);
  *success = true;
}

void WebPluginDelegateProxy::OnResolveProxy(const GURL& url,
                                            bool* result,
                                            std::string* proxy_list) {
  *result = RenderThreadImpl::current()->ResolveProxy(url, proxy_list);
}

void WebPluginDelegateProxy::OnGetPluginElement(int route_id, bool* success) {
  *success = false;
  NPObject* npobject = NULL;
  if (plugin_)
    npobject = plugin_->GetPluginElement();
  if (!npobject)
    return;

  // The stub will delete itself when the proxy tells it that it's released, or
  // otherwise when the channel is closed.
  new NPObjectStub(
      npobject, channel_host_.get(), route_id, 0, page_url_);
  *success = true;
}

void WebPluginDelegateProxy::OnSetCookie(const GURL& url,
                                         const GURL& first_party_for_cookies,
                                         const std::string& cookie) {
  if (plugin_)
    plugin_->SetCookie(url, first_party_for_cookies, cookie);
}

void WebPluginDelegateProxy::OnGetCookies(const GURL& url,
                                          const GURL& first_party_for_cookies,
                                          std::string* cookies) {
  DCHECK(cookies);
  if (plugin_)
    *cookies = plugin_->GetCookies(url, first_party_for_cookies);
}

void WebPluginDelegateProxy::CopyFromBackBufferToFrontBuffer(
    const gfx::Rect& rect) {
#if defined(OS_MACOSX)
  // Blitting the bits directly is much faster than going through CG, and since
  // the goal is just to move the raw pixels between two bitmaps with the same
  // pixel format (no compositing, color correction, etc.), it's safe.
  const size_t stride =
      skia::PlatformCanvasStrideForWidth(plugin_rect_.width());
  const size_t chunk_size = 4 * rect.width();
  DCHECK(back_buffer_dib() != NULL);
  uint8* source_data = static_cast<uint8*>(back_buffer_dib()->memory()) +
                       rect.y() * stride + 4 * rect.x();
  DCHECK(front_buffer_dib() != NULL);
  uint8* target_data = static_cast<uint8*>(front_buffer_dib()->memory()) +
                       rect.y() * stride + 4 * rect.x();
  for (int row = 0; row < rect.height(); ++row) {
    memcpy(target_data, source_data, chunk_size);
    source_data += stride;
    target_data += stride;
  }
#else
  BlitCanvasToCanvas(front_buffer_canvas(),
                     rect,
                     back_buffer_canvas(),
                     rect.origin());
#endif
}

void WebPluginDelegateProxy::UpdateFrontBuffer(
    const gfx::Rect& rect,
    bool allow_buffer_flipping) {
  if (!front_buffer_canvas()) {
    return;
  }

#if defined(OS_WIN)
  // If SendUpdateGeometry() would block on the plugin process then we don't
  // want to use buffer flipping at all since it would add extra locking.
  // (Alternatively we could probably safely use async updates for buffer
  // flipping all the time since the size is not changing.)
  if (UseSynchronousGeometryUpdates()) {
    allow_buffer_flipping = false;
  }
#endif

  // Plugin has just painted "rect" into the back-buffer, so the front-buffer
  // no longer holds the latest content for that rectangle.
  front_buffer_diff_.Subtract(rect);
  if (allow_buffer_flipping && front_buffer_diff_.IsEmpty()) {
    // Back-buffer contains the latest content for all areas; simply flip
    // the buffers.
    front_buffer_index_ = back_buffer_index();
    SendUpdateGeometry(false);
    // The front-buffer now holds newer content for this region than the
    // back-buffer.
    front_buffer_diff_ = rect;
  } else {
    // Back-buffer contains the latest content for "rect" but the front-buffer
    // contains the latest content for some other areas (or buffer flipping not
    // allowed); fall back to copying the data.
    CopyFromBackBufferToFrontBuffer(rect);
  }
  transport_store_painted_.Union(rect);
}

void WebPluginDelegateProxy::OnHandleURLRequest(
    const PluginHostMsg_URLRequest_Params& params) {
  const char* data = NULL;
  if (params.buffer.size())
    data = &params.buffer[0];

  const char* target = NULL;
  if (params.target.length())
    target = params.target.c_str();

  plugin_->HandleURLRequest(
      params.url.c_str(), params.method.c_str(), target, data,
      static_cast<unsigned int>(params.buffer.size()), params.notify_id,
      params.popups_allowed, params.notify_redirects);
}

WebPluginResourceClient* WebPluginDelegateProxy::CreateResourceClient(
    unsigned long resource_id, const GURL& url, int notify_id) {
  if (!channel_host_.get())
    return NULL;

  ResourceClientProxy* proxy =
      new ResourceClientProxy(channel_host_.get(), instance_id_);
  proxy->Initialize(resource_id, url, notify_id);
  return proxy;
}

WebPluginResourceClient* WebPluginDelegateProxy::CreateSeekableResourceClient(
    unsigned long resource_id, int range_request_id) {
  if (!channel_host_.get())
    return NULL;

  ResourceClientProxy* proxy =
      new ResourceClientProxy(channel_host_.get(), instance_id_);
  proxy->InitializeForSeekableStream(resource_id, range_request_id);
  return proxy;
}

void WebPluginDelegateProxy::FetchURL(unsigned long resource_id,
                                      int notify_id,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      const std::string& method,
                                      const char* buf,
                                      unsigned int len,
                                      const GURL& referrer,
                                      bool notify_redirects,
                                      bool is_plugin_src_load,
                                      int origin_pid,
                                      int render_frame_id,
                                      int render_view_id) {
  PluginMsg_FetchURL_Params params;
  params.resource_id = resource_id;
  params.notify_id = notify_id;
  params.url = url;
  params.first_party_for_cookies = first_party_for_cookies;
  params.method = method;
  if (len) {
    params.post_data.resize(len);
    memcpy(&params.post_data.front(), buf, len);
  }
  params.referrer = referrer;
  params.notify_redirect = notify_redirects;
  params.is_plugin_src_load = is_plugin_src_load;
  params.render_frame_id = render_frame_id;
  Send(new PluginMsg_FetchURL(instance_id_, params));
}

#if defined(OS_MACOSX)
void WebPluginDelegateProxy::OnFocusChanged(bool focused) {
  if (render_view_)
    render_view_->PluginFocusChanged(focused, instance_id_);
}

void WebPluginDelegateProxy::OnStartIme() {
  if (render_view_)
    render_view_->StartPluginIme();
}
#endif

gfx::PluginWindowHandle WebPluginDelegateProxy::GetPluginWindowHandle() {
  return window_;
}

void WebPluginDelegateProxy::OnCancelDocumentLoad() {
  plugin_->CancelDocumentLoad();
}

void WebPluginDelegateProxy::OnInitiateHTTPRangeRequest(
    const std::string& url,
    const std::string& range_info,
    int range_request_id) {
  plugin_->InitiateHTTPRangeRequest(
      url.c_str(), range_info.c_str(), range_request_id);
}

void WebPluginDelegateProxy::OnDidStartLoading() {
  plugin_->DidStartLoading();
}

void WebPluginDelegateProxy::OnDidStopLoading() {
  plugin_->DidStopLoading();
}

void WebPluginDelegateProxy::OnDeferResourceLoading(unsigned long resource_id,
                                                    bool defer) {
  plugin_->SetDeferResourceLoading(resource_id, defer);
}

#if defined(OS_MACOSX)
void WebPluginDelegateProxy::OnAcceleratedPluginEnabledRendering() {
  uses_compositor_ = true;
  OnSetWindow(gfx::kNullPluginWindow);
}

void WebPluginDelegateProxy::OnAcceleratedPluginAllocatedIOSurface(
    int32 width,
    int32 height,
    uint32 surface_id) {
  if (plugin_)
    plugin_->AcceleratedPluginAllocatedIOSurface(width, height, surface_id);
}

void WebPluginDelegateProxy::OnAcceleratedPluginSwappedIOSurface() {
  if (plugin_)
    plugin_->AcceleratedPluginSwappedIOSurface();
}
#endif

#if defined(OS_WIN)
bool WebPluginDelegateProxy::UseSynchronousGeometryUpdates() {
  // Need to update geometry synchronously with WMP, otherwise if a site
  // scripts the plugin to start playing while it's in the middle of handling
  // an update geometry message, videos don't play.  See urls in bug 20260.
  if (info_.name.find(base::ASCIIToUTF16("Windows Media Player")) !=
      base::string16::npos)
    return true;

  // The move networks plugin needs to be informed of geometry updates
  // synchronously.
  std::vector<WebPluginMimeType>::iterator index;
  for (index = info_.mime_types.begin(); index != info_.mime_types.end();
       index++) {
    if (index->mime_type == "application/x-vnd.moveplayer.qm" ||
        index->mime_type == "application/x-vnd.moveplay2.qm" ||
        index->mime_type == "application/x-vnd.movenetworks.qm" ||
        index->mime_type == "application/x-vnd.mnplayer.qm") {
      return true;
    }
  }
  return false;
}
#endif

void WebPluginDelegateProxy::OnURLRedirectResponse(bool allow,
                                                   int resource_id) {
  if (!plugin_)
    return;

  plugin_->URLRedirectResponse(allow, resource_id);
}

void WebPluginDelegateProxy::OnCheckIfRunInsecureContent(const GURL& url,
                                                         bool* result) {
  *result = plugin_->CheckIfRunInsecureContent(url);
}

}  // namespace content
