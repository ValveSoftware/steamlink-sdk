// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/plugin_host.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/child/npapi/plugin_instance.h"
#include "content/child/npapi/plugin_lib.h"
#include "content/child/npapi/plugin_stream_url.h"
#include "content/child/npapi/webplugin_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "content/public/common/webplugininfo.h"
#include "net/base/filename_util.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using blink::WebBindings;

// Declarations for stub implementations of deprecated functions, which are no
// longer listed in npapi.h.
extern "C" {
void* NPN_GetJavaEnv();
void* NPN_GetJavaPeer(NPP);
}

namespace content {

// Finds a PluginInstance from an NPP.
// The caller must take a reference if needed.
static PluginInstance* FindInstance(NPP id) {
  if (id == NULL) {
    return NULL;
  }
  return reinterpret_cast<PluginInstance*>(id->ndata);
}

#if defined(OS_MACOSX)
// Returns true if Core Animation plugins are supported. This requires that the
// OS supports shared accelerated surfaces via IOSurface. This is true on Snow
// Leopard and higher.
static bool SupportsCoreAnimationPlugins() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableCoreAnimationPlugins))
    return false;
  // We also need to be running with desktop GL and not the software
  // OSMesa renderer in order to share accelerated surfaces between
  // processes. Because on MacOS we lazy-initialize GLSurface in the
  // renderer process here, ensure we're not also initializing GL somewhere
  // else, and that we only do this once.
  static gfx::GLImplementation implementation = gfx::kGLImplementationNone;
  if (implementation == gfx::kGLImplementationNone) {
    // Not initialized yet.
    DCHECK_EQ(implementation, gfx::GetGLImplementation())
        << "GL already initialized by someone else to: "
        << gfx::GetGLImplementation();
    if (!gfx::GLSurface::InitializeOneOff()) {
      return false;
    }
    implementation = gfx::GetGLImplementation();
  }
  return (implementation == gfx::kGLImplementationDesktopGL);
}
#endif

PluginHost::PluginHost() {
  InitializeHostFuncs();
}

PluginHost::~PluginHost() {
}

PluginHost *PluginHost::Singleton() {
  CR_DEFINE_STATIC_LOCAL(scoped_refptr<PluginHost>, singleton, ());
  if (singleton.get() == NULL) {
    singleton = new PluginHost();
  }

  DCHECK(singleton.get() != NULL);
  return singleton.get();
}

void PluginHost::InitializeHostFuncs() {
  memset(&host_funcs_, 0, sizeof(host_funcs_));
  host_funcs_.size = sizeof(host_funcs_);
  host_funcs_.version = (NP_VERSION_MAJOR << 8) | (NP_VERSION_MINOR);

  // The "basic" functions
  host_funcs_.geturl = &NPN_GetURL;
  host_funcs_.posturl = &NPN_PostURL;
  host_funcs_.requestread = &NPN_RequestRead;
  host_funcs_.newstream = &NPN_NewStream;
  host_funcs_.write = &NPN_Write;
  host_funcs_.destroystream = &NPN_DestroyStream;
  host_funcs_.status = &NPN_Status;
  host_funcs_.uagent = &NPN_UserAgent;
  host_funcs_.memalloc = &NPN_MemAlloc;
  host_funcs_.memfree = &NPN_MemFree;
  host_funcs_.memflush = &NPN_MemFlush;
  host_funcs_.reloadplugins = &NPN_ReloadPlugins;

  // Stubs for deprecated Java functions
  host_funcs_.getJavaEnv = &NPN_GetJavaEnv;
  host_funcs_.getJavaPeer = &NPN_GetJavaPeer;

  // Advanced functions we implement
  host_funcs_.geturlnotify = &NPN_GetURLNotify;
  host_funcs_.posturlnotify = &NPN_PostURLNotify;
  host_funcs_.getvalue = &NPN_GetValue;
  host_funcs_.setvalue = &NPN_SetValue;
  host_funcs_.invalidaterect = &NPN_InvalidateRect;
  host_funcs_.invalidateregion = &NPN_InvalidateRegion;
  host_funcs_.forceredraw = &NPN_ForceRedraw;

  // These come from the Javascript Engine
  host_funcs_.getstringidentifier = WebBindings::getStringIdentifier;
  host_funcs_.getstringidentifiers = WebBindings::getStringIdentifiers;
  host_funcs_.getintidentifier = WebBindings::getIntIdentifier;
  host_funcs_.identifierisstring = WebBindings::identifierIsString;
  host_funcs_.utf8fromidentifier = WebBindings::utf8FromIdentifier;
  host_funcs_.intfromidentifier = WebBindings::intFromIdentifier;
  host_funcs_.createobject = WebBindings::createObject;
  host_funcs_.retainobject = WebBindings::retainObject;
  host_funcs_.releaseobject = WebBindings::releaseObject;
  host_funcs_.invoke = WebBindings::invoke;
  host_funcs_.invokeDefault = WebBindings::invokeDefault;
  host_funcs_.evaluate = WebBindings::evaluate;
  host_funcs_.getproperty = WebBindings::getProperty;
  host_funcs_.setproperty = WebBindings::setProperty;
  host_funcs_.removeproperty = WebBindings::removeProperty;
  host_funcs_.hasproperty = WebBindings::hasProperty;
  host_funcs_.hasmethod = WebBindings::hasMethod;
  host_funcs_.releasevariantvalue = WebBindings::releaseVariantValue;
  host_funcs_.setexception = WebBindings::setException;
  host_funcs_.pushpopupsenabledstate = NPN_PushPopupsEnabledState;
  host_funcs_.poppopupsenabledstate = NPN_PopPopupsEnabledState;
  host_funcs_.enumerate = WebBindings::enumerate;
  host_funcs_.pluginthreadasynccall = NPN_PluginThreadAsyncCall;
  host_funcs_.construct = WebBindings::construct;
  host_funcs_.getvalueforurl = NPN_GetValueForURL;
  host_funcs_.setvalueforurl = NPN_SetValueForURL;
  host_funcs_.getauthenticationinfo = NPN_GetAuthenticationInfo;
  host_funcs_.scheduletimer = NPN_ScheduleTimer;
  host_funcs_.unscheduletimer = NPN_UnscheduleTimer;
  host_funcs_.popupcontextmenu = NPN_PopUpContextMenu;
  host_funcs_.convertpoint = NPN_ConvertPoint;
  host_funcs_.handleevent = NPN_HandleEvent;
  host_funcs_.unfocusinstance = NPN_UnfocusInstance;
  host_funcs_.urlredirectresponse = NPN_URLRedirectResponse;
}

void PluginHost::PatchNPNetscapeFuncs(NPNetscapeFuncs* overrides) {
  // When running in the plugin process, we need to patch the NPN functions
  // that the plugin calls to interact with NPObjects that we give.  Otherwise
  // the plugin will call the v8 NPN functions, which won't work since we have
  // an NPObjectProxy and not a real v8 implementation.
  if (overrides->invoke)
    host_funcs_.invoke = overrides->invoke;

  if (overrides->invokeDefault)
    host_funcs_.invokeDefault = overrides->invokeDefault;

  if (overrides->evaluate)
    host_funcs_.evaluate = overrides->evaluate;

  if (overrides->getproperty)
    host_funcs_.getproperty = overrides->getproperty;

  if (overrides->setproperty)
    host_funcs_.setproperty = overrides->setproperty;

  if (overrides->removeproperty)
    host_funcs_.removeproperty = overrides->removeproperty;

  if (overrides->hasproperty)
    host_funcs_.hasproperty = overrides->hasproperty;

  if (overrides->hasmethod)
    host_funcs_.hasmethod = overrides->hasmethod;

  if (overrides->setexception)
    host_funcs_.setexception = overrides->setexception;

  if (overrides->enumerate)
    host_funcs_.enumerate = overrides->enumerate;
}

bool PluginHost::SetPostData(const char* buf,
                             uint32 length,
                             std::vector<std::string>* names,
                             std::vector<std::string>* values,
                             std::vector<char>* body) {
  // Use a state table to do the parsing.  Whitespace must be
  // trimmed after the fact if desired.  In our case, we actually
  // don't care about the whitespace, because we're just going to
  // pass this back into another POST.  This function strips out the
  // "Content-length" header and does not append it to the request.

  //
  // This parser takes action only on state changes.
  //
  // Transition table:
  //                  :       \n  NULL    Other
  // 0 GetHeader      1       2   4       0
  // 1 GetValue       1       0   3       1
  // 2 GetData        2       2   3       2
  // 3 DONE
  // 4 ERR
  //
  enum { INPUT_COLON=0, INPUT_NEWLINE, INPUT_NULL, INPUT_OTHER };
  enum { GETNAME, GETVALUE, GETDATA, DONE, ERR };
  int statemachine[3][4] = { { GETVALUE, GETDATA, GETDATA, GETNAME },
                             { GETVALUE, GETNAME, DONE, GETVALUE },
                             { GETDATA,  GETDATA, DONE, GETDATA } };
  std::string name, value;
  const char* ptr = static_cast<const char*>(buf);
  const char* start = ptr;
  int state = GETNAME;  // initial state
  bool done = false;
  bool err = false;
  do {
    int input;

    // Translate the current character into an input
    // for the state table.
    switch (*ptr) {
      case ':' :
        input = INPUT_COLON;
        break;
      case '\n':
        input = INPUT_NEWLINE;
        break;
      case 0   :
        input = INPUT_NULL;
        break;
      default  :
        input = INPUT_OTHER;
        break;
    }

    int newstate = statemachine[state][input];

    // Take action based on the new state.
    if (state != newstate) {
      switch (newstate) {
        case GETNAME:
          // Got a value.
          value = std::string(start, ptr - start);
          base::TrimWhitespace(value, base::TRIM_ALL, &value);
          // If the name field is empty, we'll skip this header
          // but we won't error out.
          if (!name.empty() && name != "content-length") {
            names->push_back(name);
            values->push_back(value);
          }
          start = ptr + 1;
          break;
        case GETVALUE:
          // Got a header.
          name = StringToLowerASCII(std::string(start, ptr - start));
          base::TrimWhitespace(name, base::TRIM_ALL, &name);
          start = ptr + 1;
          break;
        case GETDATA: {
          // Finished headers, now get body
          if (*ptr)
            start = ptr + 1;
          size_t previous_size = body->size();
          size_t new_body_size = length - static_cast<int>(start - buf);
          body->resize(previous_size + new_body_size);
          if (!body->empty())
            memcpy(&body->front() + previous_size, start, new_body_size);
          done = true;
          break;
        }
        case ERR:
          // error
          err = true;
          done = true;
          break;
      }
    }
    state = newstate;
    ptr++;
  } while (!done);

  return !err;
}

}  // namespace content

extern "C" {

using content::FindInstance;
using content::PluginHost;
using content::PluginInstance;
using content::WebPlugin;

// Allocates memory from the host's memory space.
void* NPN_MemAlloc(uint32_t size) {
  // Note: We must use the same allocator/deallocator
  // that is used by the javascript library, as some of the
  // JS APIs will pass memory to the plugin which the plugin
  // will attempt to free.
  return malloc(size);
}

// Deallocates memory from the host's memory space
void NPN_MemFree(void* ptr) {
  if (ptr != NULL && ptr != reinterpret_cast<void*>(-1))
    free(ptr);
}

// Requests that the host free a specified amount of memory.
uint32_t NPN_MemFlush(uint32_t size) {
  // This is not relevant on Windows; MAC specific
  return size;
}

// This is for dynamic discovery of new plugins.
// Should force a re-scan of the plugins directory to load new ones.
void NPN_ReloadPlugins(NPBool reload_pages) {
  blink::resetPluginCache(reload_pages ? true : false);
}

// Requests a range of bytes for a seekable stream.
NPError NPN_RequestRead(NPStream* stream, NPByteRange* range_list) {
  if (!stream || !range_list)
    return NPERR_GENERIC_ERROR;

  scoped_refptr<PluginInstance> plugin(
      reinterpret_cast<PluginInstance*>(stream->ndata));
  if (!plugin.get())
    return NPERR_GENERIC_ERROR;

  plugin->RequestRead(stream, range_list);
  return NPERR_NO_ERROR;
}

// Generic form of GetURL for common code between GetURL and GetURLNotify.
static NPError GetURLNotify(NPP id,
                            const char* url,
                            const char* target,
                            bool notify,
                            void* notify_data) {
  if (!url)
    return NPERR_INVALID_URL;

  scoped_refptr<PluginInstance> plugin(FindInstance(id));
  if (!plugin.get()) {
    return NPERR_GENERIC_ERROR;
  }

  plugin->RequestURL(url, "GET", target, NULL, 0, notify, notify_data);
  return NPERR_NO_ERROR;
}

// Requests creation of a new stream with the contents of the
// specified URL; gets notification of the result.
NPError NPN_GetURLNotify(NPP id,
                         const char* url,
                         const char* target,
                         void* notify_data) {
  // This is identical to NPN_GetURL, but after finishing, the
  // browser will call NPP_URLNotify to inform the plugin that
  // it has completed.

  // According to the NPAPI documentation, if target == _self
  // or a parent to _self, the browser should return NPERR_INVALID_PARAM,
  // because it can't notify the plugin once deleted.  This is
  // absolutely false; firefox doesn't do this, and Flash relies on
  // being able to use this.

  // Also according to the NPAPI documentation, we should return
  // NPERR_INVALID_URL if the url requested is not valid.  However,
  // this would require that we synchronously start fetching the
  // URL.  That just isn't practical.  As such, there really is
  // no way to return this error.  From looking at the Firefox
  // implementation, it doesn't look like Firefox does this either.

  return GetURLNotify(id, url, target, true, notify_data);
}

NPError NPN_GetURL(NPP id, const char* url, const char* target) {
  // Notes:
  //    Request from the Plugin to fetch content either for the plugin
  //    or to be placed into a browser window.
  //
  // If target == null, the browser fetches content and streams to plugin.
  //    otherwise, the browser loads content into an existing browser frame.
  // If the target is the window/frame containing the plugin, the plugin
  //    may be destroyed.
  // If the target is _blank, a mailto: or news: url open content in a new
  //    browser window
  // If the target is _self, no other instance of the plugin is created.  The
  //    plugin continues to operate in its own window

  return GetURLNotify(id, url, target, false, 0);
}

// Generic form of PostURL for common code between PostURL and PostURLNotify.
static NPError PostURLNotify(NPP id,
                             const char* url,
                             const char* target,
                             uint32_t len,
                             const char* buf,
                             NPBool file,
                             bool notify,
                             void* notify_data) {
  if (!url)
    return NPERR_INVALID_URL;

  scoped_refptr<PluginInstance> plugin(FindInstance(id));
  if (!plugin.get()) {
    NOTREACHED();
    return NPERR_GENERIC_ERROR;
  }

  std::string post_file_contents;

  if (file) {
    // Post data to be uploaded from a file. This can be handled in two
    // ways.
    // 1. Read entire file and send the contents as if it was a post data
    //    specified in the argument
    // 2. Send just the file details and read them in the browser at the
    //    time of sending the request.
    // Approach 2 is more efficient but complicated. Approach 1 has a major
    // drawback of sending potentially large data over two IPC hops.  In a way
    // 'large data over IPC' problem exists as it is in case of plugin giving
    // the data directly instead of in a file.
    // Currently we are going with the approach 1 to get the feature working.
    // We can optimize this later with approach 2.

    // TODO(joshia): Design a scheme to send a file descriptor instead of
    // entire file contents across.

    // Security alert:
    // ---------------
    // Here we are blindly uploading whatever file requested by a plugin.
    // This is risky as someone could exploit a plugin to send private
    // data in arbitrary locations.
    // A malicious (non-sandboxed) plugin has unfeterred access to OS
    // resources and can do this anyway without using browser's HTTP stack.
    // FWIW, Firefox and Safari don't perform any security checks.

    if (!buf)
      return NPERR_FILE_NOT_FOUND;

    std::string file_path_ascii(buf);
    base::FilePath file_path;
    static const char kFileUrlPrefix[] = "file:";
    if (StartsWithASCII(file_path_ascii, kFileUrlPrefix, false)) {
      GURL file_url(file_path_ascii);
      DCHECK(file_url.SchemeIsFile());
      net::FileURLToFilePath(file_url, &file_path);
    } else {
      file_path = base::FilePath::FromUTF8Unsafe(file_path_ascii);
    }

    base::File::Info post_file_info;
    if (!base::GetFileInfo(file_path, &post_file_info) ||
        post_file_info.is_directory)
      return NPERR_FILE_NOT_FOUND;

    if (!base::ReadFileToString(file_path, &post_file_contents))
      return NPERR_FILE_NOT_FOUND;

    buf = post_file_contents.c_str();
    len = post_file_contents.size();
  }

  // The post data sent by a plugin contains both headers
  // and post data.  Example:
  //      Content-type: text/html
  //      Content-length: 200
  //
  //      <200 bytes of content here>
  //
  // Unfortunately, our stream needs these broken apart,
  // so we need to parse the data and set headers and data
  // separately.
  plugin->RequestURL(url, "POST", target, buf, len, notify, notify_data);
  return NPERR_NO_ERROR;
}

NPError NPN_PostURLNotify(NPP id,
                          const char* url,
                          const char* target,
                          uint32_t len,
                          const char* buf,
                          NPBool file,
                          void* notify_data) {
  return PostURLNotify(id, url, target, len, buf, file, true, notify_data);
}

NPError NPN_PostURL(NPP id,
                    const char* url,
                    const char* target,
                    uint32_t len,
                    const char* buf,
                    NPBool file) {
  // POSTs data to an URL, either from a temp file or a buffer.
  // If file is true, buf contains a temp file (which host will delete after
  //   completing), and len contains the length of the filename.
  // If file is false, buf contains the data to send, and len contains the
  //   length of the buffer
  //
  // If target is null,
  //   server response is returned to the plugin
  // If target is _current, _self, or _top,
  //   server response is written to the plugin window and plugin is unloaded.
  // If target is _new or _blank,
  //   server response is written to a new browser window
  // If target is an existing frame,
  //   server response goes to that frame.
  //
  // For protocols other than FTP
  //   file uploads must be line-end converted from \r\n to \n
  //
  // Note:  you cannot specify headers (even a blank line) in a memory buffer,
  //        use NPN_PostURLNotify

  return PostURLNotify(id, url, target, len, buf, file, false, 0);
}

NPError NPN_NewStream(NPP id,
                      NPMIMEType type,
                      const char* target,
                      NPStream** stream) {
  // Requests creation of a new data stream produced by the plugin,
  // consumed by the browser.
  //
  // Browser should put this stream into a window target.
  //
  // TODO: implement me
  DVLOG(1) << "NPN_NewStream is not implemented yet.";
  return NPERR_GENERIC_ERROR;
}

int32_t NPN_Write(NPP id, NPStream* stream, int32_t len, void* buffer) {
  // Writes data to an existing Plugin-created stream.

  // TODO: implement me
  DVLOG(1) << "NPN_Write is not implemented yet.";
  return NPERR_GENERIC_ERROR;
}

NPError NPN_DestroyStream(NPP id, NPStream* stream, NPReason reason) {
  // Destroys a stream (could be created by plugin or browser).
  //
  // Reasons:
  //    NPRES_DONE          - normal completion
  //    NPRES_USER_BREAK    - user terminated
  //    NPRES_NETWORK_ERROR - network error (all errors fit here?)
  //
  //

  scoped_refptr<PluginInstance> plugin(FindInstance(id));
  if (plugin.get() == NULL) {
    NOTREACHED();
    return NPERR_GENERIC_ERROR;
  }

  return plugin->NPP_DestroyStream(stream, reason);
}

const char* NPN_UserAgent(NPP id) {
#if defined(OS_WIN)
  // Flash passes in a null id during the NP_initialize call.  We need to
  // default to the Mozilla user agent if we don't have an NPP instance or
  // else Flash won't request windowless mode.
  bool use_mozilla_user_agent = true;
  if (id) {
    scoped_refptr<PluginInstance> plugin = FindInstance(id);
    if (plugin.get() && !plugin->use_mozilla_user_agent())
      use_mozilla_user_agent = false;
  }

  if (use_mozilla_user_agent)
    return "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.9a1) "
        "Gecko/20061103 Firefox/2.0a1";
#endif

  // Provide a consistent user-agent string with memory that lasts
  // long enough for the caller to read it.
  static base::LazyInstance<std::string>::Leaky leaky_user_agent =
    LAZY_INSTANCE_INITIALIZER;
  if (leaky_user_agent == NULL)
    leaky_user_agent.Get() = content::GetContentClient()->GetUserAgent();
  return leaky_user_agent.Get().c_str();
}

void NPN_Status(NPP id, const char* message) {
  // Displays a message on the status line of the browser window.

  // TODO: implement me
  DVLOG(1) << "NPN_Status is not implemented yet.";
}

void NPN_InvalidateRect(NPP id, NPRect *invalidRect) {
  // Invalidates specified drawing area prior to repainting or refreshing a
  // windowless plugin

  // Before a windowless plugin can refresh part of its drawing area, it must
  // first invalidate it.  This function causes the NPP_HandleEvent method to
  // pass an update event or a paint message to the plug-in.  After calling
  // this method, the plug-in receives a paint message asynchronously.

  // The browser redraws invalid areas of the document and any windowless
  // plug-ins at regularly timed intervals. To force a paint message, the
  // plug-in can call NPN_ForceRedraw after calling this method.

  scoped_refptr<PluginInstance> plugin(FindInstance(id));
  if (plugin.get() && plugin->webplugin()) {
    if (invalidRect) {
#if defined(OS_WIN)
      if (!plugin->windowless()) {
        RECT rect = {0};
        rect.left = invalidRect->left;
        rect.right = invalidRect->right;
        rect.top = invalidRect->top;
        rect.bottom = invalidRect->bottom;
        ::InvalidateRect(plugin->window_handle(), &rect, false);
        return;
      }
#endif
      gfx::Rect rect(invalidRect->left,
                     invalidRect->top,
                     invalidRect->right - invalidRect->left,
                     invalidRect->bottom - invalidRect->top);
      plugin->webplugin()->InvalidateRect(rect);
    } else {
      plugin->webplugin()->Invalidate();
    }
  }
}

void NPN_InvalidateRegion(NPP id, NPRegion invalidRegion) {
  // Invalidates a specified drawing region prior to repainting
  // or refreshing a window-less plugin.
  //
  // Similar to NPN_InvalidateRect.

  // TODO: this is overkill--add platform-specific region handling (at the
  // very least, fetch the region's bounding box and pass it to InvalidateRect).
  scoped_refptr<PluginInstance> plugin(FindInstance(id));
  DCHECK(plugin.get() != NULL);
  if (plugin.get() && plugin->webplugin())
    plugin->webplugin()->Invalidate();
}

void NPN_ForceRedraw(NPP id) {
  // Forces repaint for a windowless plug-in.
  //
  // We deliberately do not implement this; we don't want plugins forcing
  // synchronous paints.
}

NPError NPN_GetValue(NPP id, NPNVariable variable, void* value) {
  // Allows the plugin to query the browser for information
  //
  // Variables:
  //    NPNVxDisplay (unix only)
  //    NPNVxtAppContext (unix only)
  //    NPNVnetscapeWindow (win only) - Gets the native window on which the
  //              plug-in drawing occurs, returns HWND
  //    NPNVjavascriptEnabledBool:  tells whether Javascript is enabled
  //    NPNVasdEnabledBool:  tells whether SmartUpdate is enabled
  //    NPNVOfflineBool: tells whether offline-mode is enabled

  NPError rv = NPERR_GENERIC_ERROR;

  switch (static_cast<int>(variable)) {
    case NPNVWindowNPObject: {
      scoped_refptr<PluginInstance> plugin(FindInstance(id));
      if (!plugin.get()) {
        NOTREACHED();
        return NPERR_INVALID_INSTANCE_ERROR;
      }
      NPObject *np_object = plugin->webplugin()->GetWindowScriptNPObject();
      // Return value is expected to be retained, as
      // described here:
      // <http://www.mozilla.org/projects/plugins/npruntime.html#browseraccess>
      if (np_object) {
        WebBindings::retainObject(np_object);
        void **v = (void **)value;
        *v = np_object;
        rv = NPERR_NO_ERROR;
      } else {
        NOTREACHED();
      }
      break;
    }
    case NPNVPluginElementNPObject: {
      scoped_refptr<PluginInstance> plugin(FindInstance(id));
      if (!plugin.get()) {
        NOTREACHED();
        return NPERR_INVALID_INSTANCE_ERROR;
      }
      NPObject *np_object = plugin->webplugin()->GetPluginElement();
      // Return value is expected to be retained, as
      // described here:
      // <http://www.mozilla.org/projects/plugins/npruntime.html#browseraccess>
      if (np_object) {
        WebBindings::retainObject(np_object);
        void** v = static_cast<void**>(value);
        *v = np_object;
        rv = NPERR_NO_ERROR;
      } else {
        NOTREACHED();
      }
      break;
    }
  #if !defined(OS_MACOSX)  // OS X doesn't have windowed plugins.
    case NPNVnetscapeWindow: {
      scoped_refptr<PluginInstance> plugin = FindInstance(id);
      if (!plugin.get()) {
        NOTREACHED();
        return NPERR_INVALID_INSTANCE_ERROR;
      }
      gfx::PluginWindowHandle handle = plugin->window_handle();
      *((void**)value) = (void*)handle;
      rv = NPERR_NO_ERROR;
      break;
    }
  #endif
    case NPNVjavascriptEnabledBool: {
      // yes, JS is enabled.
      *((void**)value) = (void*)1;
      rv = NPERR_NO_ERROR;
      break;
    }
    case NPNVSupportsWindowless: {
      NPBool* supports_windowless = reinterpret_cast<NPBool*>(value);
      *supports_windowless = true;
      rv = NPERR_NO_ERROR;
      break;
    }
    case NPNVprivateModeBool: {
      NPBool* private_mode = reinterpret_cast<NPBool*>(value);
      scoped_refptr<PluginInstance> plugin(FindInstance(id));
      if (!plugin.get()) {
        NOTREACHED();
        return NPERR_INVALID_INSTANCE_ERROR;
      }
      *private_mode = plugin->webplugin()->IsOffTheRecord();
      rv = NPERR_NO_ERROR;
      break;
    }
  #if defined(OS_MACOSX)
    case NPNVpluginDrawingModel: {
      // return the drawing model that was negotiated when we initialized.
      scoped_refptr<PluginInstance> plugin(FindInstance(id));
      if (!plugin.get()) {
        NOTREACHED();
        return NPERR_INVALID_INSTANCE_ERROR;
      }
      *reinterpret_cast<int*>(value) = plugin->drawing_model();
      rv = NPERR_NO_ERROR;
      break;
    }
    case NPNVsupportsCoreGraphicsBool:
    case NPNVsupportsCocoaBool: {
      // These drawing and event models are always supported.
      NPBool* supports_model = reinterpret_cast<NPBool*>(value);
      *supports_model = true;
      rv = NPERR_NO_ERROR;
      break;
    }
    case NPNVsupportsInvalidatingCoreAnimationBool:
    case NPNVsupportsCoreAnimationBool: {
      NPBool* supports_model = reinterpret_cast<NPBool*>(value);
      *supports_model = content::SupportsCoreAnimationPlugins();
      rv = NPERR_NO_ERROR;
      break;
    }
#ifndef NP_NO_CARBON
    case NPNVsupportsCarbonBool:
#endif
#ifndef NP_NO_QUICKDRAW
    case NPNVsupportsQuickDrawBool:
#endif
    case NPNVsupportsOpenGLBool: {
      // These models are never supported. OpenGL was never widely supported,
      // and QuickDraw and Carbon have been deprecated for quite some time.
      NPBool* supports_model = reinterpret_cast<NPBool*>(value);
      *supports_model = false;
      rv = NPERR_NO_ERROR;
      break;
    }
    case NPNVsupportsCompositingCoreAnimationPluginsBool: {
      NPBool* supports_compositing = reinterpret_cast<NPBool*>(value);
      *supports_compositing = content::SupportsCoreAnimationPlugins();
      rv = NPERR_NO_ERROR;
      break;
    }
    case NPNVsupportsUpdatedCocoaTextInputBool: {
      // We support the clarifications to the Cocoa IME event spec.
      NPBool* supports_update = reinterpret_cast<NPBool*>(value);
      *supports_update = true;
      rv = NPERR_NO_ERROR;
      break;
    }
  #endif  // OS_MACOSX
    default:
      DVLOG(1) << "NPN_GetValue(" << variable << ") is not implemented yet.";
      break;
  }
  return rv;
}

NPError NPN_SetValue(NPP id, NPPVariable variable, void* value) {
  // Allows the plugin to set various modes

  scoped_refptr<PluginInstance> plugin(FindInstance(id));
  if (!plugin.get()) {
    NOTREACHED();
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  switch(variable) {
    case NPPVpluginWindowBool: {
      // Sets windowless mode for display of the plugin
      // Note: the documentation at
      // http://developer.mozilla.org/en/docs/NPN_SetValue is wrong.  When
      // value is NULL, the mode is set to true.  This is the same way Mozilla
      // works.
      plugin->set_windowless(value == 0);
      return NPERR_NO_ERROR;
    }
    case NPPVpluginTransparentBool: {
      // Sets transparent mode for display of the plugin
      //
      // Transparent plugins require the browser to paint the background
      // before having the plugin paint.  By default, windowless plugins
      // are transparent.  Making a windowless plugin opaque means that
      // the plugin does not require the browser to paint the background.
      bool mode = (value != 0);
      plugin->set_transparent(mode);
      return NPERR_NO_ERROR;
    }
    case NPPVjavascriptPushCallerBool:
      // Specifies whether you are pushing or popping the JSContext off.
      // the stack
      // TODO: implement me
      DVLOG(1) << "NPN_SetValue(NPPVJavascriptPushCallerBool) is not "
                  "implemented.";
      return NPERR_GENERIC_ERROR;
    case NPPVpluginKeepLibraryInMemory:
      // Tells browser that plugin library should live longer than usual.
      // TODO: implement me
      DVLOG(1) << "NPN_SetValue(NPPVpluginKeepLibraryInMemory) is not "
                  "implemented.";
      return NPERR_GENERIC_ERROR;
  #if defined(OS_MACOSX)
    case NPPVpluginDrawingModel: {
      intptr_t model = reinterpret_cast<intptr_t>(value);
      if (model == NPDrawingModelCoreGraphics ||
          ((model == NPDrawingModelInvalidatingCoreAnimation ||
            model == NPDrawingModelCoreAnimation) &&
           content::SupportsCoreAnimationPlugins())) {
        plugin->set_drawing_model(static_cast<NPDrawingModel>(model));
        return NPERR_NO_ERROR;
      }
      return NPERR_GENERIC_ERROR;
    }
    case NPPVpluginEventModel: {
      // Only the Cocoa event model is supported.
      intptr_t model = reinterpret_cast<intptr_t>(value);
      if (model == NPEventModelCocoa) {
        plugin->set_event_model(static_cast<NPEventModel>(model));
        return NPERR_NO_ERROR;
      }
      return NPERR_GENERIC_ERROR;
    }
  #endif
    default:
      // TODO: implement me
      DVLOG(1) << "NPN_SetValue(" << variable << ") is not implemented.";
      break;
  }

  NOTREACHED();
  return NPERR_GENERIC_ERROR;
}

void* NPN_GetJavaEnv() {
  // TODO: implement me
  DVLOG(1) << "NPN_GetJavaEnv is not implemented.";
  return NULL;
}

void* NPN_GetJavaPeer(NPP) {
  // TODO: implement me
  DVLOG(1) << "NPN_GetJavaPeer is not implemented.";
  return NULL;
}

void NPN_PushPopupsEnabledState(NPP id, NPBool enabled) {
  scoped_refptr<PluginInstance> plugin(FindInstance(id));
  if (plugin.get())
    plugin->PushPopupsEnabledState(enabled ? true : false);
}

void NPN_PopPopupsEnabledState(NPP id) {
  scoped_refptr<PluginInstance> plugin(FindInstance(id));
  if (plugin.get())
    plugin->PopPopupsEnabledState();
}

void NPN_PluginThreadAsyncCall(NPP id,
                               void (*func)(void*),
                               void* user_data) {
  scoped_refptr<PluginInstance> plugin(FindInstance(id));
  if (plugin.get())
    plugin->PluginThreadAsyncCall(func, user_data);
}

NPError NPN_GetValueForURL(NPP id,
                           NPNURLVariable variable,
                           const char* url,
                           char** value,
                           uint32_t* len) {
  if (!id)
    return NPERR_INVALID_PARAM;

  if (!url || !*url || !len)
    return NPERR_INVALID_URL;

  *len = 0;
  std::string result;

  switch (variable) {
    case NPNURLVProxy: {
      result = "DIRECT";
      scoped_refptr<PluginInstance> plugin(FindInstance(id));
      if (!plugin.get())
        return NPERR_GENERIC_ERROR;

      WebPlugin* webplugin = plugin->webplugin();
      if (!webplugin)
        return NPERR_GENERIC_ERROR;

      if (!webplugin->FindProxyForUrl(GURL(std::string(url)), &result))
        return NPERR_GENERIC_ERROR;
      break;
    }
    case NPNURLVCookie: {
      scoped_refptr<PluginInstance> plugin(FindInstance(id));
      if (!plugin.get())
        return NPERR_GENERIC_ERROR;

      WebPlugin* webplugin = plugin->webplugin();
      if (!webplugin)
        return NPERR_GENERIC_ERROR;

      // Bypass third-party cookie blocking by using the url as the
      // first_party_for_cookies.
      GURL cookies_url((std::string(url)));
      result = webplugin->GetCookies(cookies_url, cookies_url);
      break;
    }
    default:
      return NPERR_GENERIC_ERROR;
  }

  // Allocate this using the NPAPI allocator. The plugin will call
  // NPN_Free to free this.
  *value = static_cast<char*>(NPN_MemAlloc(result.length() + 1));
  base::strlcpy(*value, result.c_str(), result.length() + 1);
  *len = result.length();

  return NPERR_NO_ERROR;
}

NPError NPN_SetValueForURL(NPP id,
                           NPNURLVariable variable,
                           const char* url,
                           const char* value,
                           uint32_t len) {
  if (!id)
    return NPERR_INVALID_PARAM;

  if (!url || !*url)
    return NPERR_INVALID_URL;

  switch (variable) {
    case NPNURLVCookie: {
      scoped_refptr<PluginInstance> plugin(FindInstance(id));
      if (!plugin.get())
        return NPERR_GENERIC_ERROR;

      WebPlugin* webplugin = plugin->webplugin();
      if (!webplugin)
        return NPERR_GENERIC_ERROR;

      std::string cookie(value, len);
      GURL cookies_url((std::string(url)));
      webplugin->SetCookie(cookies_url, cookies_url, cookie);
      return NPERR_NO_ERROR;
    }
    case NPNURLVProxy:
      // We don't support setting proxy values, fall through...
      break;
    default:
      // Fall through and return an error...
      break;
  }

  return NPERR_GENERIC_ERROR;
}

NPError NPN_GetAuthenticationInfo(NPP id,
                                  const char* protocol,
                                  const char* host,
                                  int32_t port,
                                  const char* scheme,
                                  const char* realm,
                                  char** username,
                                  uint32_t* ulen,
                                  char** password,
                                  uint32_t* plen) {
  if (!id || !protocol || !host || !scheme || !realm || !username ||
      !ulen || !password || !plen)
    return NPERR_INVALID_PARAM;

  // TODO: implement me (bug 23928)
  return NPERR_GENERIC_ERROR;
}

uint32_t NPN_ScheduleTimer(NPP id,
                           uint32_t interval,
                           NPBool repeat,
                           void (*func)(NPP id, uint32_t timer_id)) {
  scoped_refptr<PluginInstance> plugin(FindInstance(id));
  if (!plugin.get())
    return 0;

  return plugin->ScheduleTimer(interval, repeat, func);
}

void NPN_UnscheduleTimer(NPP id, uint32_t timer_id) {
  scoped_refptr<PluginInstance> plugin(FindInstance(id));
  if (plugin.get())
    plugin->UnscheduleTimer(timer_id);
}

NPError NPN_PopUpContextMenu(NPP id, NPMenu* menu) {
  if (!menu)
    return NPERR_INVALID_PARAM;

  scoped_refptr<PluginInstance> plugin(FindInstance(id));
  if (plugin.get()) {
    return plugin->PopUpContextMenu(menu);
  }
  NOTREACHED();
  return NPERR_GENERIC_ERROR;
}

NPBool NPN_ConvertPoint(NPP id, double sourceX, double sourceY,
                        NPCoordinateSpace sourceSpace,
                        double *destX, double *destY,
                        NPCoordinateSpace destSpace) {
  scoped_refptr<PluginInstance> plugin(FindInstance(id));
  if (plugin.get()) {
    return plugin->ConvertPoint(
        sourceX, sourceY, sourceSpace, destX, destY, destSpace);
  }
  NOTREACHED();
  return false;
}

NPBool NPN_HandleEvent(NPP id, void *event, NPBool handled) {
  // TODO: Implement advanced key handling: http://crbug.com/46578
  NOTIMPLEMENTED();
  return false;
}

NPBool NPN_UnfocusInstance(NPP id, NPFocusDirection direction) {
  // TODO: Implement advanced key handling: http://crbug.com/46578
  NOTIMPLEMENTED();
  return false;
}

void NPN_URLRedirectResponse(NPP instance, void* notify_data, NPBool allow) {
  scoped_refptr<PluginInstance> plugin(FindInstance(instance));
  if (plugin.get()) {
    plugin->URLRedirectResponse(!!allow, notify_data);
  }
}

}  // extern "C"
