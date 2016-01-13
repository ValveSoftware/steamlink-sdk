// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/plugin_instance.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/child/npapi/plugin_host.h"
#include "content/child/npapi/plugin_lib.h"
#include "content/child/npapi/plugin_stream_url.h"
#include "content/child/npapi/plugin_string_stream.h"
#include "content/child/npapi/webplugin.h"
#include "content/child/npapi/webplugin_delegate.h"
#include "content/child/npapi/webplugin_resource_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "net/base/escape.h"

#if defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace content {

PluginInstance::PluginInstance(PluginLib* plugin, const std::string& mime_type)
    : plugin_(plugin),
      npp_(0),
      host_(PluginHost::Singleton()),
      npp_functions_(plugin->functions()),
      window_handle_(0),
      windowless_(false),
      transparent_(true),
      webplugin_(0),
      mime_type_(mime_type),
      get_notify_data_(0),
      use_mozilla_user_agent_(false),
#if defined (OS_MACOSX)
#ifdef NP_NO_QUICKDRAW
      drawing_model_(NPDrawingModelCoreGraphics),
#else
      drawing_model_(NPDrawingModelQuickDraw),
#endif
#ifdef NP_NO_CARBON
      event_model_(NPEventModelCocoa),
#else
      event_model_(NPEventModelCarbon),
#endif
      currently_handled_event_(NULL),
#endif
      message_loop_(base::MessageLoop::current()),
      load_manually_(false),
      in_close_streams_(false),
      next_timer_id_(1),
      next_notify_id_(0),
      next_range_request_id_(0),
      handles_url_redirects_(false) {
  npp_ = new NPP_t();
  npp_->ndata = 0;
  npp_->pdata = 0;

  if (mime_type_ == kFlashPluginSwfMimeType)
    transparent_ = false;

  memset(&zero_padding_, 0, sizeof(zero_padding_));
  DCHECK(message_loop_);
}

PluginInstance::~PluginInstance() {
  CloseStreams();

  if (npp_ != 0) {
    delete npp_;
    npp_ = 0;
  }

  if (plugin_.get())
    plugin_->CloseInstance();
}

PluginStreamUrl* PluginInstance::CreateStream(unsigned long resource_id,
                                              const GURL& url,
                                              const std::string& mime_type,
                                              int notify_id) {

  bool notify;
  void* notify_data;
  GetNotifyData(notify_id, &notify, &notify_data);
  PluginStreamUrl* stream = new PluginStreamUrl(
      resource_id, url, this, notify, notify_data);

  AddStream(stream);
  return stream;
}

void PluginInstance::AddStream(PluginStream* stream) {
  open_streams_.push_back(make_scoped_refptr(stream));
}

void PluginInstance::RemoveStream(PluginStream* stream) {
  if (in_close_streams_)
    return;

  std::vector<scoped_refptr<PluginStream> >::iterator stream_index;
  for (stream_index = open_streams_.begin();
       stream_index != open_streams_.end(); ++stream_index) {
    if (stream_index->get() == stream) {
      open_streams_.erase(stream_index);
      break;
    }
  }
}

bool PluginInstance::IsValidStream(const NPStream* stream) {
  std::vector<scoped_refptr<PluginStream> >::iterator stream_index;
  for (stream_index = open_streams_.begin();
          stream_index != open_streams_.end(); ++stream_index) {
    if ((*stream_index)->stream() == stream)
      return true;
  }

  return false;
}

void PluginInstance::CloseStreams() {
  in_close_streams_ = true;
  for (unsigned int index = 0; index < open_streams_.size(); ++index) {
    // Close all streams on the way down.
    open_streams_[index]->Close(NPRES_USER_BREAK);
  }
  open_streams_.clear();
  in_close_streams_ = false;
}

WebPluginResourceClient* PluginInstance::GetRangeRequest(
    int id) {
  PendingRangeRequestMap::iterator iter = pending_range_requests_.find(id);
  if (iter == pending_range_requests_.end()) {
    NOTREACHED();
    return NULL;
  }

  WebPluginResourceClient* rv = iter->second->AsResourceClient();
  pending_range_requests_.erase(iter);
  return rv;
}

bool PluginInstance::Start(const GURL& url,
                           char** const param_names,
                           char** const param_values,
                           int param_count,
                           bool load_manually) {
  load_manually_ = load_manually;
  unsigned short mode = load_manually_ ? NP_FULL : NP_EMBED;
  npp_->ndata = this;

  NPError err = NPP_New(mode, param_count,
      const_cast<char **>(param_names), const_cast<char **>(param_values));

  if (err == NPERR_NO_ERROR) {
    handles_url_redirects_ =
        ((npp_functions_->version >= NPVERS_HAS_URL_REDIRECT_HANDLING) &&
         (npp_functions_->urlredirectnotify));
  }
  return err == NPERR_NO_ERROR;
}

NPObject *PluginInstance::GetPluginScriptableObject() {
  NPObject *value = NULL;
  NPError error = NPP_GetValue(NPPVpluginScriptableNPObject, &value);
  if (error != NPERR_NO_ERROR || value == NULL)
    return NULL;
  return value;
}

bool PluginInstance::GetFormValue(base::string16* value) {
  // Plugins will allocate memory for the return value by using NPN_MemAlloc().
  char *plugin_value = NULL;
  NPError error = NPP_GetValue(NPPVformValue, &plugin_value);
  if (error != NPERR_NO_ERROR || !plugin_value) {
    return false;
  }
  // Assumes the result is UTF8 text, as Firefox does.
  *value = base::UTF8ToUTF16(plugin_value);
  host_->host_functions()->memfree(plugin_value);
  return true;
}

// WebPluginLoadDelegate methods
void PluginInstance::DidFinishLoadWithReason(const GURL& url,
                                             NPReason reason,
                                             int notify_id) {
  bool notify;
  void* notify_data;
  GetNotifyData(notify_id, &notify, &notify_data);
  if (!notify) {
    NOTREACHED();
    return;
  }

  NPP_URLNotify(url.spec().c_str(), reason, notify_data);
}

unsigned PluginInstance::GetBackingTextureId() {
  // By default the plugin instance is not backed by an OpenGL texture.
  return 0;
}

// NPAPI methods
NPError PluginInstance::NPP_New(unsigned short mode,
                                short argc,
                                char* argn[],
                                char* argv[]) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->newp != 0);
  DCHECK(argc >= 0);

  if (npp_functions_->newp != 0) {
    return npp_functions_->newp(
        (NPMIMEType)mime_type_.c_str(), npp_, mode,  argc, argn, argv, NULL);
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

void PluginInstance::NPP_Destroy() {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->destroy != 0);

  if (npp_functions_->destroy != 0) {
    NPSavedData *savedData = 0;
    npp_functions_->destroy(npp_, &savedData);

    // TODO: Support savedData.  Technically, these need to be
    //       saved on a per-URL basis, and then only passed
    //       to new instances of the plugin at the same URL.
    //       Sounds like a huge security risk.  When we do support
    //       these, we should pass them back to the PluginLib
    //       to be stored there.
    DCHECK(savedData == 0);
  }

  for (unsigned int file_index = 0; file_index < files_created_.size();
       file_index++) {
    base::DeleteFile(files_created_[file_index], false);
  }

  // Ensure that no timer callbacks are invoked after NPP_Destroy.
  timers_.clear();
}

NPError PluginInstance::NPP_SetWindow(NPWindow* window) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->setwindow != 0);

  if (npp_functions_->setwindow != 0) {
    return npp_functions_->setwindow(npp_, window);
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

NPError PluginInstance::NPP_NewStream(NPMIMEType type,
                                      NPStream* stream,
                                      NPBool seekable,
                                      unsigned short* stype) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->newstream != 0);
  if (npp_functions_->newstream != 0) {
      return npp_functions_->newstream(npp_, type, stream, seekable, stype);
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

NPError PluginInstance::NPP_DestroyStream(NPStream* stream, NPReason reason) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->destroystream != 0);

  if (stream == NULL || !IsValidStream(stream) || (stream->ndata == NULL))
    return NPERR_INVALID_INSTANCE_ERROR;

  if (npp_functions_->destroystream != 0) {
    NPError result = npp_functions_->destroystream(npp_, stream, reason);
    stream->ndata = NULL;
    return result;
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

int PluginInstance::NPP_WriteReady(NPStream* stream) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->writeready != 0);
  if (npp_functions_->writeready != 0) {
    return npp_functions_->writeready(npp_, stream);
  }
  return 0;
}

int PluginInstance::NPP_Write(NPStream* stream,
                              int offset,
                              int len,
                              void* buffer) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->write != 0);
  if (npp_functions_->write != 0) {
    return npp_functions_->write(npp_, stream, offset, len, buffer);
  }
  return 0;
}

void PluginInstance::NPP_StreamAsFile(NPStream* stream, const char* fname) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->asfile != 0);
  if (npp_functions_->asfile != 0) {
    npp_functions_->asfile(npp_, stream, fname);
  }

  // Creating a temporary FilePath instance on the stack as the explicit
  // FilePath constructor with StringType as an argument causes a compiler
  // error when invoked via vector push back.
  base::FilePath file_name = base::FilePath::FromUTF8Unsafe(fname);
  files_created_.push_back(file_name);
}

void PluginInstance::NPP_URLNotify(const char* url,
                                   NPReason reason,
                                   void* notifyData) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->urlnotify != 0);
  if (npp_functions_->urlnotify != 0) {
    npp_functions_->urlnotify(npp_, url, reason, notifyData);
  }
}

NPError PluginInstance::NPP_GetValue(NPPVariable variable, void* value) {
  DCHECK(npp_functions_ != 0);
  // getvalue is NULL for Shockwave
  if (npp_functions_->getvalue != 0) {
    return npp_functions_->getvalue(npp_, variable, value);
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

NPError PluginInstance::NPP_SetValue(NPNVariable variable, void* value) {
  DCHECK(npp_functions_ != 0);
  if (npp_functions_->setvalue != 0) {
    return npp_functions_->setvalue(npp_, variable, value);
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

short PluginInstance::NPP_HandleEvent(void* event) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->event != 0);
  if (npp_functions_->event != 0) {
    return npp_functions_->event(npp_, (void*)event);
  }
  return false;
}

bool PluginInstance::NPP_Print(NPPrint* platform_print) {
  DCHECK(npp_functions_ != 0);
  if (npp_functions_->print != 0) {
    npp_functions_->print(npp_, platform_print);
    return true;
  }
  return false;
}

void PluginInstance::NPP_URLRedirectNotify(const char* url, int32_t status,
                                           void* notify_data) {
  DCHECK(npp_functions_ != 0);
  if (npp_functions_->urlredirectnotify != 0) {
    npp_functions_->urlredirectnotify(npp_, url, status, notify_data);
  }
}

void PluginInstance::SendJavaScriptStream(const GURL& url,
                                          const std::string& result,
                                          bool success,
                                          int notify_id) {
  bool notify;
  void* notify_data;
  GetNotifyData(notify_id, &notify, &notify_data);

  if (success) {
    PluginStringStream *stream =
        new PluginStringStream(this, url, notify, notify_data);
    AddStream(stream);
    stream->SendToPlugin(result, "text/html");
  } else {
    // NOTE: Sending an empty stream here will crash MacroMedia
    // Flash 9.  Just send the URL Notify.
    if (notify)
      NPP_URLNotify(url.spec().c_str(), NPRES_DONE, notify_data);
  }
}

void PluginInstance::DidReceiveManualResponse(const GURL& url,
                                              const std::string& mime_type,
                                              const std::string& headers,
                                              uint32 expected_length,
                                              uint32 last_modified) {
  DCHECK(load_manually_);

  plugin_data_stream_ = CreateStream(-1, url, mime_type, 0);
  plugin_data_stream_->DidReceiveResponse(mime_type, headers, expected_length,
                                          last_modified, true);
}

void PluginInstance::DidReceiveManualData(const char* buffer, int length) {
  DCHECK(load_manually_);
  if (plugin_data_stream_.get() != NULL) {
    plugin_data_stream_->DidReceiveData(buffer, length, 0);
  }
}

void PluginInstance::DidFinishManualLoading() {
  DCHECK(load_manually_);
  if (plugin_data_stream_.get() != NULL) {
    plugin_data_stream_->DidFinishLoading(plugin_data_stream_->ResourceId());
    plugin_data_stream_->Close(NPRES_DONE);
    plugin_data_stream_ = NULL;
  }
}

void PluginInstance::DidManualLoadFail() {
  DCHECK(load_manually_);
  if (plugin_data_stream_.get() != NULL) {
    plugin_data_stream_->DidFail(plugin_data_stream_->ResourceId());
    plugin_data_stream_ = NULL;
  }
}

void PluginInstance::PluginThreadAsyncCall(void (*func)(void*),
                                           void* user_data) {
  message_loop_->PostTask(
      FROM_HERE, base::Bind(&PluginInstance::OnPluginThreadAsyncCall, this,
                            func, user_data));
}

void PluginInstance::OnPluginThreadAsyncCall(void (*func)(void*),
                                             void* user_data) {
  // Do not invoke the callback if NPP_Destroy has already been invoked.
  if (webplugin_)
    func(user_data);
}

uint32 PluginInstance::ScheduleTimer(uint32 interval,
                                     NPBool repeat,
                                     void (*func)(NPP id, uint32 timer_id)) {
  // Use next timer id.
  uint32 timer_id;
  timer_id = next_timer_id_;
  ++next_timer_id_;
  DCHECK(next_timer_id_ != 0);

  // Record timer interval and repeat.
  TimerInfo info;
  info.interval = interval;
  info.repeat = repeat ? true : false;
  timers_[timer_id] = info;

  // Schedule the callback.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PluginInstance::OnTimerCall, this, func, npp_, timer_id),
      base::TimeDelta::FromMilliseconds(interval));
  return timer_id;
}

void PluginInstance::UnscheduleTimer(uint32 timer_id) {
  // Remove info about the timer.
  TimerMap::iterator it = timers_.find(timer_id);
  if (it != timers_.end())
    timers_.erase(it);
}

#if !defined(OS_MACOSX)
NPError PluginInstance::PopUpContextMenu(NPMenu* menu) {
  NOTIMPLEMENTED();
  return NPERR_GENERIC_ERROR;
}
#endif

void PluginInstance::OnTimerCall(void (*func)(NPP id, uint32 timer_id),
                                 NPP id,
                                 uint32 timer_id) {
  // Do not invoke callback if the timer has been unscheduled.
  TimerMap::iterator it = timers_.find(timer_id);
  if (it == timers_.end())
    return;

  // Get all information about the timer before invoking the callback. The
  // callback might unschedule the timer.
  TimerInfo info = it->second;

  func(id, timer_id);

  // If the timer was unscheduled by the callback, just free up the timer id.
  if (timers_.find(timer_id) == timers_.end())
    return;

  // Reschedule repeating timers after invoking the callback so callback is not
  // re-entered if it pumps the message loop.
  if (info.repeat) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PluginInstance::OnTimerCall, this, func, npp_, timer_id),
        base::TimeDelta::FromMilliseconds(info.interval));
  } else {
    timers_.erase(it);
  }
}

void PluginInstance::PushPopupsEnabledState(bool enabled) {
  popups_enabled_stack_.push(enabled);
}

void PluginInstance::PopPopupsEnabledState() {
  popups_enabled_stack_.pop();
}

void PluginInstance::RequestRead(NPStream* stream, NPByteRange* range_list) {
  std::string range_info = "bytes=";

  while (range_list) {
    range_info += base::IntToString(range_list->offset);
    range_info.push_back('-');
    range_info +=
        base::IntToString(range_list->offset + range_list->length - 1);
    range_list = range_list->next;
    if (range_list)
      range_info.push_back(',');
  }

  if (plugin_data_stream_.get()) {
    if (plugin_data_stream_->stream() == stream) {
      webplugin_->CancelDocumentLoad();
      plugin_data_stream_ = NULL;
    }
  }

  // The lifetime of a NPStream instance depends on the PluginStream instance
  // which owns it. When a plugin invokes NPN_RequestRead on a seekable stream,
  // we don't want to create a new stream when the corresponding response is
  // received. We send over a cookie which represents the PluginStream
  // instance which is sent back from the renderer when the response is
  // received.
  std::vector<scoped_refptr<PluginStream> >::iterator stream_index;
  for (stream_index = open_streams_.begin();
          stream_index != open_streams_.end(); ++stream_index) {
    PluginStream* plugin_stream = stream_index->get();
    if (plugin_stream->stream() == stream) {
      // A stream becomes seekable the first time NPN_RequestRead
      // is called on it.
      plugin_stream->set_seekable(true);

      if (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableDirectNPAPIRequests)) {
        pending_range_requests_[++next_range_request_id_] = plugin_stream;
        webplugin_->InitiateHTTPRangeRequest(
            stream->url, range_info.c_str(), next_range_request_id_);
        return;
      } else {
        PluginStreamUrl* plugin_stream_url =
            static_cast<PluginStreamUrl*>(plugin_stream);
        plugin_stream_url->FetchRange(range_info);
        return;
      }
    }
  }
  NOTREACHED();
}

void PluginInstance::RequestURL(const char* url,
                                const char* method,
                                const char* target,
                                const char* buf,
                                unsigned int len,
                                bool notify,
                                void* notify_data) {
  int notify_id = 0;
  if (notify) {
    notify_id = ++next_notify_id_;
    pending_requests_[notify_id] = notify_data;
  }

  webplugin_->HandleURLRequest(
      url, method, target, buf, len, notify_id, popups_allowed(),
      notify ? handles_url_redirects_ : false);
}

bool PluginInstance::ConvertPoint(double source_x, double source_y,
                                  NPCoordinateSpace source_space,
                                  double* dest_x, double* dest_y,
                                  NPCoordinateSpace dest_space) {
#if defined(OS_MACOSX)
  CGRect main_display_bounds = CGDisplayBounds(CGMainDisplayID());

  double flipped_screen_x = source_x;
  double flipped_screen_y = source_y;
  switch(source_space) {
    case NPCoordinateSpacePlugin:
      flipped_screen_x += plugin_origin_.x();
      flipped_screen_y += plugin_origin_.y();
      break;
    case NPCoordinateSpaceWindow:
      flipped_screen_x += containing_window_frame_.x();
      flipped_screen_y = containing_window_frame_.height() - source_y +
          containing_window_frame_.y();
      break;
    case NPCoordinateSpaceFlippedWindow:
      flipped_screen_x += containing_window_frame_.x();
      flipped_screen_y += containing_window_frame_.y();
      break;
    case NPCoordinateSpaceScreen:
      flipped_screen_y = main_display_bounds.size.height - flipped_screen_y;
      break;
    case NPCoordinateSpaceFlippedScreen:
      break;
    default:
      NOTREACHED();
      return false;
  }

  double target_x = flipped_screen_x;
  double target_y = flipped_screen_y;
  switch(dest_space) {
    case NPCoordinateSpacePlugin:
      target_x -= plugin_origin_.x();
      target_y -= plugin_origin_.y();
      break;
    case NPCoordinateSpaceWindow:
      target_x -= containing_window_frame_.x();
      target_y -= containing_window_frame_.y();
      target_y = containing_window_frame_.height() - target_y;
      break;
    case NPCoordinateSpaceFlippedWindow:
      target_x -= containing_window_frame_.x();
      target_y -= containing_window_frame_.y();
      break;
    case NPCoordinateSpaceScreen:
      target_y = main_display_bounds.size.height - flipped_screen_y;
      break;
    case NPCoordinateSpaceFlippedScreen:
      break;
    default:
      NOTREACHED();
      return false;
  }

  if (dest_x)
    *dest_x = target_x;
  if (dest_y)
    *dest_y = target_y;
  return true;
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

void PluginInstance::GetNotifyData(int notify_id,
                                   bool* notify,
                                   void** notify_data) {
  PendingRequestMap::iterator iter = pending_requests_.find(notify_id);
  if (iter != pending_requests_.end()) {
    *notify = true;
    *notify_data = iter->second;
    pending_requests_.erase(iter);
  } else {
    *notify = false;
    *notify_data = NULL;
  }
}

void PluginInstance::URLRedirectResponse(bool allow, void* notify_data) {
  // The notify_data passed in allows us to identify the matching stream.
  std::vector<scoped_refptr<PluginStream> >::iterator stream_index;
  for (stream_index = open_streams_.begin();
          stream_index != open_streams_.end(); ++stream_index) {
    PluginStream* plugin_stream = stream_index->get();
    if (plugin_stream->notify_data() == notify_data) {
      PluginStreamUrl* plugin_stream_url =
          static_cast<PluginStreamUrl*>(plugin_stream);
      plugin_stream_url->URLRedirectResponse(allow);
      break;
    }
  }
}

}  // namespace content
