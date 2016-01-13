// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NPAPI_WEBPLUGIN_DELEGATE_PROXY_H_
#define CONTENT_RENDERER_NPAPI_WEBPLUGIN_DELEGATE_PROXY_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/child/npapi/webplugin_delegate.h"
#include "content/public/common/webplugininfo.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/surface/transport_dib.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include "base/containers/hash_tables.h"
#include "base/memory/linked_ptr.h"
#endif

struct NPObject;
struct PluginHostMsg_URLRequest_Params;
class SkBitmap;

namespace base {
class WaitableEvent;
}

namespace content {
class NPObjectStub;
class PluginChannelHost;
class RenderFrameImpl;
class RenderViewImpl;
class WebPluginImpl;

// An implementation of WebPluginDelegate that proxies all calls to
// the plugin process.
class WebPluginDelegateProxy
    : public WebPluginDelegate,
      public IPC::Listener,
      public IPC::Sender,
      public base::SupportsWeakPtr<WebPluginDelegateProxy> {
 public:
  WebPluginDelegateProxy(WebPluginImpl* plugin,
                         const std::string& mime_type,
                         const base::WeakPtr<RenderViewImpl>& render_view,
                         RenderFrameImpl* render_frame);

  // WebPluginDelegate implementation:
  virtual void PluginDestroyed() OVERRIDE;
  virtual bool Initialize(const GURL& url,
                          const std::vector<std::string>& arg_names,
                          const std::vector<std::string>& arg_values,
                          bool load_manually) OVERRIDE;
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect) OVERRIDE;
  virtual void Paint(SkCanvas* canvas, const gfx::Rect& rect) OVERRIDE;
  virtual NPObject* GetPluginScriptableObject() OVERRIDE;
  virtual struct _NPP* GetPluginNPP() OVERRIDE;
  virtual bool GetFormValue(base::string16* value) OVERRIDE;
  virtual void DidFinishLoadWithReason(const GURL& url, NPReason reason,
                                       int notify_id) OVERRIDE;
  virtual void SetFocus(bool focused) OVERRIDE;
  virtual bool HandleInputEvent(const blink::WebInputEvent& event,
                                WebCursor::CursorInfo* cursor) OVERRIDE;
  virtual int GetProcessId() OVERRIDE;

  // Informs the plugin that its containing content view has gained or lost
  // first responder status.
  virtual void SetContentAreaFocus(bool has_focus);
#if defined(OS_WIN)
  // Informs the plugin that plugin IME has updated its status.
  virtual void ImeCompositionUpdated(
      const base::string16& text,
      const std::vector<int>& clauses,
      const std::vector<int>& target,
      int cursor_position,
      int plugin_id);
  // Informs the plugin that plugin IME has completed.
  // If |text| is empty, composition was cancelled.
  virtual void ImeCompositionCompleted(const base::string16& text,
                                       int plugin_id);
#endif
#if defined(OS_MACOSX)
  // Informs the plugin that its enclosing window has gained or lost focus.
  virtual void SetWindowFocus(bool window_has_focus);
  // Informs the plugin that its container (window/tab) has changed visibility.
  virtual void SetContainerVisibility(bool is_visible);
  // Informs the plugin that its enclosing window's frame has changed.
  virtual void WindowFrameChanged(gfx::Rect window_frame, gfx::Rect view_frame);
  // Informs the plugin that plugin IME has completed.
  // If |text| is empty, composition was cancelled.
  virtual void ImeCompositionCompleted(const base::string16& text,
                                       int plugin_id);
#endif

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  virtual void SendJavaScriptStream(const GURL& url,
                                    const std::string& result,
                                    bool success,
                                    int notify_id) OVERRIDE;

  virtual void DidReceiveManualResponse(const GURL& url,
                                        const std::string& mime_type,
                                        const std::string& headers,
                                        uint32 expected_length,
                                        uint32 last_modified) OVERRIDE;
  virtual void DidReceiveManualData(const char* buffer, int length) OVERRIDE;
  virtual void DidFinishManualLoading() OVERRIDE;
  virtual void DidManualLoadFail() OVERRIDE;
  virtual WebPluginResourceClient* CreateResourceClient(
      unsigned long resource_id, const GURL& url, int notify_id) OVERRIDE;
  virtual WebPluginResourceClient* CreateSeekableResourceClient(
      unsigned long resource_id, int range_request_id) OVERRIDE;
  virtual void FetchURL(unsigned long resource_id,
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
                        int render_view_id) OVERRIDE;

  gfx::PluginWindowHandle GetPluginWindowHandle();

 protected:
  friend class base::DeleteHelper<WebPluginDelegateProxy>;
  virtual ~WebPluginDelegateProxy();

 private:
  struct SharedBitmap {
    SharedBitmap();
    ~SharedBitmap();

    scoped_ptr<TransportDIB> dib;
    scoped_ptr<SkCanvas> canvas;
  };

  // Message handlers for messages that proxy WebPlugin methods, which
  // we translate into calls to the real WebPlugin.
  void OnSetWindow(gfx::PluginWindowHandle window);
  void OnCompleteURL(const std::string& url_in, std::string* url_out,
                     bool* result);
  void OnHandleURLRequest(const PluginHostMsg_URLRequest_Params& params);
  void OnCancelResource(int id);
  void OnInvalidateRect(const gfx::Rect& rect);
  void OnGetWindowScriptNPObject(int route_id, bool* success);
  void OnResolveProxy(const GURL& url, bool* result, std::string* proxy_list);
  void OnGetPluginElement(int route_id, bool* success);
  void OnSetCookie(const GURL& url,
                   const GURL& first_party_for_cookies,
                   const std::string& cookie);
  void OnGetCookies(const GURL& url, const GURL& first_party_for_cookies,
                    std::string* cookies);
  void OnCancelDocumentLoad();
  void OnInitiateHTTPRangeRequest(const std::string& url,
                                  const std::string& range_info,
                                  int range_request_id);
  void OnDidStartLoading();
  void OnDidStopLoading();
  void OnDeferResourceLoading(unsigned long resource_id, bool defer);
  void OnURLRedirectResponse(bool allow, int resource_id);
  void OnCheckIfRunInsecureContent(const GURL& url, bool* result);
#if defined(OS_MACOSX)
  void OnFocusChanged(bool focused);
  void OnStartIme();
  // Accelerated (Core Animation) plugin implementation.
  void OnAcceleratedPluginEnabledRendering();
  void OnAcceleratedPluginAllocatedIOSurface(int32 width,
                                             int32 height,
                                             uint32 surface_id);
  void OnAcceleratedPluginSwappedIOSurface();
#endif
#if defined(OS_WIN)
  void OnSetWindowlessData(HANDLE modal_loop_pump_messages_event,
                           gfx::NativeViewId dummy_activation_window);
  void OnNotifyIMEStatus(const int input_mode, const gfx::Rect& caret_rect);
#endif
  // Helper function that sends the UpdateGeometry message.
  void SendUpdateGeometry(bool bitmaps_changed);

  // Copies the given rectangle from the back-buffer transport_stores_ bitmap to
  // the front-buffer transport_stores_ bitmap.
  void CopyFromBackBufferToFrontBuffer(const gfx::Rect& rect);

  // Updates the front-buffer with the given rectangle from the back-buffer,
  // either by copying the rectangle or flipping the buffers.
  void UpdateFrontBuffer(const gfx::Rect& rect, bool allow_buffer_flipping);

  // Clears the shared memory section and canvases used for windowless plugins.
  void ResetWindowlessBitmaps();

  int front_buffer_index() const {
    return front_buffer_index_;
  }

  int back_buffer_index() const {
    return 1 - front_buffer_index_;
  }

  SkCanvas* front_buffer_canvas() const {
    return transport_stores_[front_buffer_index()].canvas.get();
  }

  SkCanvas* back_buffer_canvas() const {
    return transport_stores_[back_buffer_index()].canvas.get();
  }

  TransportDIB* front_buffer_dib() const {
    return transport_stores_[front_buffer_index()].dib.get();
  }

  TransportDIB* back_buffer_dib() const {
    return transport_stores_[back_buffer_index()].dib.get();
  }

#if !defined(OS_WIN)
  // Creates a process-local memory section and canvas. PlatformCanvas on
  // Windows only works with a DIB, not arbitrary memory.
  bool CreateLocalBitmap(std::vector<uint8>* memory,
                         scoped_ptr<SkCanvas>* canvas);
#endif

  // Creates a shared memory section and canvas.
  bool CreateSharedBitmap(scoped_ptr<TransportDIB>* memory,
                          scoped_ptr<SkCanvas>* canvas);

  // Called for cleanup during plugin destruction. Normally right before the
  // plugin window gets destroyed, or when the plugin has crashed (at which
  // point the window has already been destroyed).
  void WillDestroyWindow();

#if defined(OS_WIN)
  // Returns true if we should update the plugin geometry synchronously.
  bool UseSynchronousGeometryUpdates();
#endif

  base::WeakPtr<RenderViewImpl> render_view_;
  RenderFrameImpl* render_frame_;
  WebPluginImpl* plugin_;
  bool uses_shared_bitmaps_;
#if defined(OS_MACOSX)
  bool uses_compositor_;
#elif defined(OS_WIN)
  // Used for windowless plugins so that keyboard activation works.
  gfx::NativeViewId dummy_activation_window_;
#endif
  gfx::PluginWindowHandle window_;
  scoped_refptr<PluginChannelHost> channel_host_;
  std::string mime_type_;
  int instance_id_;
  WebPluginInfo info_;

  gfx::Rect plugin_rect_;
  gfx::Rect clip_rect_;

  NPObject* npobject_;

  // Dummy NPP used to uniquely identify this plugin.
  scoped_ptr<NPP_t> npp_;

  // Event passed in by the plugin process and is used to decide if messages
  // need to be pumped in the NPP_HandleEvent sync call.
  scoped_ptr<base::WaitableEvent> modal_loop_pump_messages_event_;

  // Bitmap for crashed plugin
  SkBitmap* sad_plugin_;

  // True if we got an invalidate from the plugin and are waiting for a paint.
  bool invalidate_pending_;

  // If the plugin is transparent or not.
  bool transparent_;

  // The index in the transport_stores_ array of the current front buffer
  // (i.e., the buffer to display).
  int front_buffer_index_;
  SharedBitmap transport_stores_[2];
  // This lets us know the total portion of the transport store that has been
  // painted since the buffers were created.
  gfx::Rect transport_store_painted_;
  // This is a bounding box on the portion of the front-buffer that was painted
  // on the last buffer flip and which has not yet been re-painted in the
  // back-buffer.
  gfx::Rect front_buffer_diff_;

  // The url of the main frame hosting the plugin.
  GURL page_url_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginDelegateProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_NPAPI_WEBPLUGIN_DELEGATE_PROXY_H_
