// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PLUGIN_WEBPLUGIN_DELEGATE_STUB_H_
#define CONTENT_PLUGIN_WEBPLUGIN_DELEGATE_STUB_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "content/child/npapi/npobject_stub.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "third_party/npapi/bindings/npapi.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

struct PluginMsg_Init_Params;
struct PluginMsg_DidReceiveResponseParams;
struct PluginMsg_FetchURL_Params;
struct PluginMsg_UpdateGeometry_Param;

namespace blink {
class WebInputEvent;
}

namespace content {
class PluginChannel;
class WebCursor;
class WebPluginDelegateImpl;
class WebPluginProxy;

// Converts the IPC messages from WebPluginDelegateProxy into calls to the
// actual WebPluginDelegateImpl object.
class WebPluginDelegateStub : public IPC::Listener,
                              public IPC::Sender,
                              public base::RefCounted<WebPluginDelegateStub> {
 public:
  WebPluginDelegateStub(const std::string& mime_type, int instance_id,
                        PluginChannel* channel);

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  int instance_id() { return instance_id_; }
  WebPluginDelegateImpl* delegate() { return delegate_; }
  WebPluginProxy* webplugin() { return webplugin_; }

 private:
  friend class base::RefCounted<WebPluginDelegateStub>;

  virtual ~WebPluginDelegateStub();

  // Message handlers for the WebPluginDelegate calls that are proxied from the
  // renderer over the IPC channel.
  void OnInit(const PluginMsg_Init_Params& params,
              bool* transparent,
              bool* result);
  void OnWillSendRequest(int id, const GURL& url, int http_status_code);
  void OnDidReceiveResponse(const PluginMsg_DidReceiveResponseParams& params);
  void OnDidReceiveData(int id, const std::vector<char>& buffer,
                        int data_offset);
  void OnDidFinishLoading(int id);
  void OnDidFail(int id);
  void OnDidFinishLoadWithReason(const GURL& url, int reason, int notify_id);
  void OnSetFocus(bool focused);
  void OnHandleInputEvent(const blink::WebInputEvent* event,
                          bool* handled, WebCursor* cursor);
  void OnPaint(const gfx::Rect& damaged_rect);
  void OnDidPaint();
  void OnUpdateGeometry(const PluginMsg_UpdateGeometry_Param& param);
  void OnGetPluginScriptableObject(int* route_id);
  void OnSendJavaScriptStream(const GURL& url,
                              const std::string& result,
                              bool success,
                              int notify_id);
  void OnGetFormValue(base::string16* value, bool* success);

  void OnSetContentAreaFocus(bool has_focus);
#if defined(OS_WIN) && !defined(USE_AURA)
  void OnImeCompositionUpdated(const base::string16& text,
                               const std::vector<int>& clauses,
                               const std::vector<int>& target,
                               int cursor_position);
  void OnImeCompositionCompleted(const base::string16& text);
#endif
#if defined(OS_MACOSX)
  void OnSetWindowFocus(bool has_focus);
  void OnContainerHidden();
  void OnContainerShown(gfx::Rect window_frame, gfx::Rect view_frame,
                        bool has_focus);
  void OnWindowFrameChanged(const gfx::Rect& window_frame,
                            const gfx::Rect& view_frame);
  void OnImeCompositionCompleted(const base::string16& text);
#endif

  void OnDidReceiveManualResponse(
      const GURL& url,
      const PluginMsg_DidReceiveResponseParams& params);
  void OnDidReceiveManualData(const std::vector<char>& buffer);
  void OnDidFinishManualLoading();
  void OnDidManualLoadFail();
  void OnHandleURLRequestReply(unsigned long resource_id,
                               const GURL& url,
                               int notify_id);
  void OnHTTPRangeRequestReply(unsigned long resource_id, int range_request_id);
  void OnFetchURL(const PluginMsg_FetchURL_Params& params);

  std::string mime_type_;
  int instance_id_;

  scoped_refptr<PluginChannel> channel_;

  base::WeakPtr<NPObjectStub> plugin_scriptable_object_;
  WebPluginDelegateImpl* delegate_;
  WebPluginProxy* webplugin_;
  bool in_destructor_;

  // The url of the main frame hosting the plugin.
  GURL page_url_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebPluginDelegateStub);
};

}  // namespace content

#endif  // CONTENT_PLUGIN_WEBPLUGIN_DELEGATE_STUB_H_
