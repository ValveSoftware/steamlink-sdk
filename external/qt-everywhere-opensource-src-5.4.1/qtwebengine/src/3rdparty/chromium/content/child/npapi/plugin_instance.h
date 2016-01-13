// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: Need to deal with NPAPI's NPSavedData.
//       I haven't seen plugins use it yet.

#ifndef CONTENT_CHILD_NPAPI_PLUGIN_INSTANCE_H_
#define CONTENT_CHILD_NPAPI_PLUGIN_INSTANCE_H_

#include <map>
#include <stack>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/nphostapi.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

namespace base {
class MessageLoop;
}

namespace content {

class PluginLib;
class PluginHost;
class PluginStream;
class PluginStreamUrl;
class WebPlugin;
class WebPluginResourceClient;

#if defined(OS_MACOSX)
class ScopedCurrentPluginEvent;
#endif

// A PluginInstance is an active, running instance of a Plugin.
// A single plugin may have many PluginInstances.
class PluginInstance : public base::RefCountedThreadSafe<PluginInstance> {
 public:
  // Create a new instance of a plugin.  The PluginInstance
  // will hold a reference to the plugin.
  PluginInstance(PluginLib* plugin, const std::string &mime_type);

  // Activates the instance by calling NPP_New.
  // This should be called after our instance is all
  // setup from the host side and we are ready to receive
  // requests from the plugin.  We must not call any
  // functions on the plugin instance until start has
  // been called.
  //
  // url: The instance URL.
  // param_names: the list of names of attributes passed via the
  //       element.
  // param_values: the list of values corresponding to param_names
  // param_count: number of attributes
  // load_manually: if true indicates that the plugin data would be passed
  //                from webkit. if false indicates that the plugin should
  //                download the data.
  //                This also controls whether the plugin is instantiated as
  //                a full page plugin (NP_FULL) or embedded (NP_EMBED)
  //
  bool Start(const GURL& url,
             char** const param_names,
             char** const param_values,
             int param_count,
             bool load_manually);

  // NPAPI's instance identifier for this instance
  NPP npp() { return npp_; }

  // Get/Set for the instance's window handle.
  gfx::PluginWindowHandle window_handle() const { return window_handle_; }
  void set_window_handle(gfx::PluginWindowHandle value) {
    window_handle_ = value;
  }

  // Get/Set whether this instance is in Windowless mode.
  // Default is false.
  bool windowless() { return windowless_; }
  void set_windowless(bool value) { windowless_ = value; }

  // Get/Set whether this instance is transparent. This only applies to
  // windowless plugins.  Transparent plugins require that webkit paint the
  // background.
  // Default is true for all plugins other than Flash. For Flash, we default to
  // opaque since it always tells us if it's transparent during NPP_New.
  bool transparent() { return transparent_; }
  void set_transparent(bool value) { transparent_ = value; }

  // Get/Set the WebPlugin associated with this instance
  WebPlugin* webplugin() { return webplugin_; }
  void set_web_plugin(WebPlugin* webplugin) {
    webplugin_ = webplugin;
  }

  // Get the mimeType for this plugin stream
  const std::string &mime_type() { return mime_type_; }

  PluginLib* plugin_lib() { return plugin_.get(); }

#if defined(OS_MACOSX)
  // Get/Set the Mac NPAPI drawing and event models
  NPDrawingModel drawing_model() { return drawing_model_; }
  void set_drawing_model(NPDrawingModel value) { drawing_model_ = value; }
  NPEventModel event_model() { return event_model_; }
  void set_event_model(NPEventModel value) { event_model_ = value; }
  // Updates the instance's tracking of the location of the plugin location
  // relative to the upper left of the screen.
  void set_plugin_origin(const gfx::Point& origin) { plugin_origin_ = origin; }
  // Updates the instance's tracking of the frame of the containing window
  // relative to the upper left of the screen.
  void set_window_frame(const gfx::Rect& frame) {
    containing_window_frame_ = frame;
  }
#endif

  // Creates a stream for sending an URL.  If notify_id is non-zero, it will
  // send a notification to the plugin when the stream is complete; otherwise it
  // will not.  Set object_url to true if the load is for the object tag's url,
  // or false if it's for a url that the plugin fetched through
  // NPN_GetUrl[Notify].
  PluginStreamUrl* CreateStream(unsigned long resource_id,
                                const GURL& url,
                                const std::string& mime_type,
                                int notify_id);

  // For each instance, we track all streams.  When the
  // instance closes, all remaining streams are also
  // closed.  All streams associated with this instance
  // should call AddStream so that they can be cleaned
  // up when the instance shuts down.
  void AddStream(PluginStream* stream);

  // This is called when a stream is closed. We remove the stream from the
  // list, which releases the reference maintained to the stream.
  void RemoveStream(PluginStream* stream);

  // Closes all open streams on this instance.
  void CloseStreams();

  // Returns the WebPluginResourceClient object for a stream that has become
  // seekable.
  WebPluginResourceClient* GetRangeRequest(int id);

  // Have the plugin create its script object.
  NPObject* GetPluginScriptableObject();

  // Returns the form value of this instance.
  bool GetFormValue(base::string16* value);

  // WebViewDelegate methods that we implement. This is for handling
  // callbacks during getURLNotify.
  void DidFinishLoadWithReason(const GURL& url, NPReason reason, int notify_id);

  // If true, send the Mozilla user agent instead of Chrome's to the plugin.
  bool use_mozilla_user_agent() { return use_mozilla_user_agent_; }
  void set_use_mozilla_user_agent() { use_mozilla_user_agent_ = true; }

  // If the plugin instance is backed by a texture, return its ID in the
  // compositor's namespace. Otherwise return 0. Returns 0 by default.
  unsigned GetBackingTextureId();

  // Helper that implements NPN_PluginThreadAsyncCall semantics
  void PluginThreadAsyncCall(void (*func)(void *),
                             void* userData);

  uint32 ScheduleTimer(uint32 interval,
                       NPBool repeat,
                       void (*func)(NPP id, uint32 timer_id));

  void UnscheduleTimer(uint32 timer_id);

  bool ConvertPoint(double source_x, double source_y,
                    NPCoordinateSpace source_space,
                    double* dest_x, double* dest_y,
                    NPCoordinateSpace dest_space);

  NPError PopUpContextMenu(NPMenu* menu);

  //
  // NPAPI methods for calling the Plugin Instance
  //
  NPError NPP_New(unsigned short, short, char *[], char *[]);
  NPError NPP_SetWindow(NPWindow*);
  NPError NPP_NewStream(NPMIMEType, NPStream*, NPBool, unsigned short*);
  NPError NPP_DestroyStream(NPStream*, NPReason);
  int NPP_WriteReady(NPStream*);
  int NPP_Write(NPStream*, int, int, void*);
  void NPP_StreamAsFile(NPStream*, const char*);
  void NPP_URLNotify(const char*, NPReason, void*);
  NPError NPP_GetValue(NPPVariable, void*);
  NPError NPP_SetValue(NPNVariable, void*);
  short NPP_HandleEvent(void*);
  void NPP_Destroy();
  bool NPP_Print(NPPrint* platform_print);
  void NPP_URLRedirectNotify(const char* url, int32_t status,
                             void* notify_data);

  void SendJavaScriptStream(const GURL& url,
                            const std::string& result,
                            bool success,
                            int notify_id);

  void DidReceiveManualResponse(const GURL& url,
                                const std::string& mime_type,
                                const std::string& headers,
                                uint32 expected_length,
                                uint32 last_modified);
  void DidReceiveManualData(const char* buffer, int length);
  void DidFinishManualLoading();
  void DidManualLoadFail();

  void PushPopupsEnabledState(bool enabled);
  void PopPopupsEnabledState();

  bool popups_allowed() const {
    return popups_enabled_stack_.empty() ? false : popups_enabled_stack_.top();
  }

  // Initiates byte range reads for plugins.
  void RequestRead(NPStream* stream, NPByteRange* range_list);

  // Handles GetURL/GetURLNotify/PostURL/PostURLNotify requests initiated
  // by plugins.
  void RequestURL(const char* url,
                  const char* method,
                  const char* target,
                  const char* buf,
                  unsigned int len,
                  bool notify,
                  void* notify_data);

  // Handles NPN_URLRedirectResponse calls issued by plugins in response to
  // HTTP URL redirect notifications.
  void URLRedirectResponse(bool allow, void* notify_data);

  bool handles_url_redirects() const { return handles_url_redirects_; }

 private:
  friend class base::RefCountedThreadSafe<PluginInstance>;

#if defined(OS_MACOSX)
  friend class ScopedCurrentPluginEvent;
  // Sets the event that the plugin is currently handling. The object is not
  // owned or copied, so the caller must call this again with NULL before the
  // event pointer becomes invalid. Clients use ScopedCurrentPluginEvent rather
  // than calling this directly.
  void set_currently_handled_event(NPCocoaEvent* event) {
    currently_handled_event_ = event;
  }
#endif

  ~PluginInstance();
  void OnPluginThreadAsyncCall(void (*func)(void *), void* userData);
  void OnTimerCall(void (*func)(NPP id, uint32 timer_id),
                   NPP id, uint32 timer_id);
  bool IsValidStream(const NPStream* stream);
  void GetNotifyData(int notify_id, bool* notify, void** notify_data);

  // This is a hack to get the real player plugin to work with chrome
  // The real player plugin dll(nppl3260) when loaded by firefox is loaded via
  // the NS COM API which is analogous to win32 COM. So the NPAPI functions in
  // the plugin are invoked via an interface by firefox. The plugin instance
  // handle which is passed to every NPAPI method is owned by the real player
  // plugin, i.e. it expects the ndata member to point to a structure which
  // it knows about. Eventually it dereferences this structure and compares
  // a member variable at offset 0x24(Version 6.0.11.2888) /2D (Version
  // 6.0.11.3088) with 0 and on failing this check, takes  a different code
  // path which causes a crash. Safari and Opera work with version 6.0.11.2888
  // by chance as their ndata structure contains a 0 at the location which real
  // player checks:(. They crash with version 6.0.11.3088 as well. The
  // following member just adds a 96 byte padding to our PluginInstance class
  // which is passed in the ndata member. This magic number works correctly on
  // Vista with UAC on or off :(.
  // NOTE: Please dont change the ordering of the member variables
  // New members should be added after this padding array.
  // TODO(iyengar) : Disassemble the Realplayer ndata structure and look into
  // the possiblity of conforming to it (http://b/issue?id=936667). We
  // could also log a bug with Real, which would save the effort.
  uint8                                    zero_padding_[96];
  scoped_refptr<PluginLib>                 plugin_;
  NPP                                      npp_;
  scoped_refptr<PluginHost>                host_;
  NPPluginFuncs*                           npp_functions_;
  std::vector<scoped_refptr<PluginStream> > open_streams_;
  gfx::PluginWindowHandle                  window_handle_;
  bool                                     windowless_;
  bool                                     transparent_;
  WebPlugin*                               webplugin_;
  std::string                              mime_type_;
  GURL                                     get_url_;
  intptr_t                                 get_notify_data_;
  bool                                     use_mozilla_user_agent_;
#if defined(OS_MACOSX)
  NPDrawingModel                           drawing_model_;
  NPEventModel                             event_model_;
  gfx::Point                               plugin_origin_;
  gfx::Rect                                containing_window_frame_;
  NPCocoaEvent*                            currently_handled_event_;  // weak
#endif
  base::MessageLoop*                       message_loop_;
  scoped_refptr<PluginStreamUrl>           plugin_data_stream_;

  // This flag if true indicates that the plugin data would be passed from
  // webkit. if false indicates that the plugin should download the data.
  bool                                     load_manually_;

  // Stack indicating if popups are to be enabled for the outgoing
  // NPN_GetURL/NPN_GetURLNotify calls.
  std::stack<bool>                         popups_enabled_stack_;

  // True if in CloseStreams().
  bool in_close_streams_;

  // List of files created for the current plugin instance. File names are
  // added to the list every time the NPP_StreamAsFile function is called.
  std::vector<base::FilePath> files_created_;

  // Next unusued timer id.
  uint32 next_timer_id_;

  // Map of timer id to settings for timer.
  struct TimerInfo {
    uint32 interval;
    bool repeat;
  };
  typedef std::map<uint32, TimerInfo> TimerMap;
  TimerMap timers_;

  // Tracks pending GET/POST requests so that the plugin-given data doesn't
  // cross process boundaries to an untrusted process.
  typedef std::map<int, void*> PendingRequestMap;
  PendingRequestMap pending_requests_;
  int next_notify_id_;

  // Used to track pending range requests so that when WebPlugin replies to us
  // we can match the reply to the stream.
  typedef std::map<int, scoped_refptr<PluginStream> > PendingRangeRequestMap;
  PendingRangeRequestMap pending_range_requests_;
  int next_range_request_id_;
  // The plugin handles the NPAPI URL redirect notification API.
  // See here https://wiki.mozilla.org/NPAPI:HTTPRedirectHandling
  bool handles_url_redirects_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstance);
};

#if defined(OS_MACOSX)
// Helper to simplify correct usage of set_currently_handled_event.
// Instantiating will set |instance|'s currently handled to |event| for the
// lifetime of the object, then NULL when it goes out of scope.
class ScopedCurrentPluginEvent {
 public:
  ScopedCurrentPluginEvent(PluginInstance* instance, NPCocoaEvent* event);
  ~ScopedCurrentPluginEvent();

 private:
  scoped_refptr<PluginInstance> instance_;
  DISALLOW_COPY_AND_ASSIGN(ScopedCurrentPluginEvent);
};
#endif

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_PLUGIN_INSTANCE_H_
