// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NPAPI_WEBPLUGIN_DELEGATE_H_
#define CONTENT_CHILD_NPAPI_WEBPLUGIN_DELEGATE_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/cursors/webcursor.h"
#include "third_party/npapi/bindings/npapi.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
class SkCanvas;
struct NPObject;

namespace blink {
class WebInputEvent;
}

namespace gfx {
class Rect;
}

namespace content {

class WebPluginResourceClient;

// This is the interface that a plugin implementation needs to provide.
class WebPluginDelegate {
 public:
  virtual ~WebPluginDelegate() {}

  // Initializes the plugin implementation with the given (UTF8) arguments.
  // Note that the lifetime of WebPlugin must be longer than this delegate.
  // If this function returns false the plugin isn't started and shouldn't be
  // called again.  If this method succeeds, then the WebPlugin is valid until
  // PluginDestroyed is called.
  // The load_manually parameter if true indicates that the plugin data would
  // be passed from webkit. if false indicates that the plugin should download
  // the data. This also controls whether the plugin is instantiated as a full
  // page plugin (NP_FULL) or embedded (NP_EMBED).
  virtual bool Initialize(const GURL& url,
                          const std::vector<std::string>& arg_names,
                          const std::vector<std::string>& arg_values,
                          bool load_manually) = 0;

  // Called when the WebPlugin is being destroyed.  This is a signal to the
  // delegate that it should tear-down the plugin implementation and not call
  // methods on the WebPlugin again.
  virtual void PluginDestroyed() = 0;

  // Update the geometry of the plugin.  This is a request to move the
  // plugin, relative to its containing window, to the coords given by
  // window_rect.  Its contents should be clipped to the coords given
  // by clip_rect, which are relative to the origin of the plugin
  // window.  The clip_rect is in plugin-relative coordinates.
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect) = 0;

  // Tells the plugin to paint the damaged rect.  |canvas| is only used for
  // windowless plugins.
  virtual void Paint(SkCanvas* canvas, const gfx::Rect& rect) = 0;

  // Informs the plugin that it has gained or lost focus. This is only called in
  // windowless mode.
  virtual void SetFocus(bool focused) = 0;

  // For windowless plugins, gives them a user event like mouse/keyboard.
  // Returns whether the event was handled. This is only called in windowsless
  // mode. See NPAPI NPP_HandleEvent for more information.
  virtual bool HandleInputEvent(const blink::WebInputEvent& event,
                                WebCursor::CursorInfo* cursor) = 0;

  // Gets the NPObject associated with the plugin for scripting.
  virtual NPObject* GetPluginScriptableObject() = 0;

  // Gets the NPP instance uniquely identifying the plugin for its lifetime.
  virtual struct _NPP* GetPluginNPP() = 0;

  // Gets the form value associated with the plugin instance.
  // Returns false if the value is not available.
  virtual bool GetFormValue(base::string16* value) = 0;

  // Receives notification about a resource load that the plugin initiated
  // for a frame.
  virtual void DidFinishLoadWithReason(const GURL& url, NPReason reason,
                                       int notify_id) = 0;

  // Returns the process id of the process that is running the plugin.
  virtual int GetProcessId() = 0;

  // The result, UTF-8 encoded, of the script execution is returned via this
  // function.
  virtual void SendJavaScriptStream(const GURL& url,
                                    const std::string& result,
                                    bool success,
                                    int notify_id) = 0;

  // Receives notification about data being available.
  virtual void DidReceiveManualResponse(const GURL& url,
                                        const std::string& mime_type,
                                        const std::string& headers,
                                        uint32 expected_length,
                                        uint32 last_modified) = 0;

  // Receives the data.
  virtual void DidReceiveManualData(const char* buffer, int length) = 0;

  // Indicates end of data load.
  virtual void DidFinishManualLoading() = 0;

  // Indicates a failure in data receipt.
  virtual void DidManualLoadFail() = 0;

  // Creates a WebPluginResourceClient instance and returns the same.
  virtual WebPluginResourceClient* CreateResourceClient(
      unsigned long resource_id,
      const GURL& url,
      int notify_id) = 0;

  // Creates a WebPluginResourceClient instance for an existing stream that is
  // has become seekable.
  virtual WebPluginResourceClient* CreateSeekableResourceClient(
      unsigned long resource_id, int range_request_id) = 0;

  // Tell the plugin that the given URL should be fetched. This is a result of
  // loading the plugin data or the plugin calling HandleURLRequest which didn't
  // end up being routed to another frame or being a javscript:// URL.
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
                        int render_view_id) = 0;

};

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_WEBPLUGIN_DELEGATE_H_
