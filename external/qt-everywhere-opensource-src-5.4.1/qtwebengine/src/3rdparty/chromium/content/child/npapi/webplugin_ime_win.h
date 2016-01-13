// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NPAPI_WEBPLUGIN_IME_WIN_H_
#define CONTENT_CHILD_NPAPI_WEBPLUGIN_IME_WIN_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "third_party/npapi/bindings/npapi.h"
#include "ui/gfx/rect.h"

namespace content {

class PluginInstance;

// A class that emulates an IME for windowless plug-ins. A windowless plug-in
// does not have a window. Therefore, we cannot attach an IME to a windowless
// plug-in. To allow such windowless plug-ins to use IMEs without any changes to
// them, this class receives the IME data from a browser and patches IMM32
// functions to return the IME data when a windowless plug-in calls IMM32
// functions. I would not Flash retrieves pointers to IMM32 functions with
// GetProcAddress(), this class also needs a hook to GetProcAddress() to
// dispatch IMM32 function calls from a plug-in to this class as listed in the
// following snippet.
//
//   FARPROC WINAPI GetProcAddressPatch(HMODULE module, LPCSTR name) {
//     FARPROC* proc = WebPluginIMEWin::GetProcAddress(name);
//     if (proc)
//       return proc;
//     return ::GetProcAddress(module, name);
//   }
//   ...
//   app::win::IATPatchFunction get_proc_address;
//   get_proc_address.Patch(
//       GetPluginPath().value().c_str(), "kernel32.dll", "GetProcAddress",
//       GetProcAddressPatch);
//
// After we successfuly dispatch IMM32 calls from a plug-in to this class, we
// need to update its IME data so the class can return it to the plug-in through
// its IMM32 calls. To update the IME data, we call CompositionUpdated() or
// CompositionCompleted() BEFORE sending an IMM32 Window message to the plugin
// with a SendEvents() call as listed in the following snippet. (Plug-ins call
// IMM32 functions when it receives IMM32 window messages. We need to update the
// IME data of this class before sending IMM32 messages so the plug-ins can get
// the latest data.)
//
//   WebPluginIMEWin ime;
//   ...
//   base::string16 text = "composing";
//   std::vector<int> clauses;
//   clauses.push_back(0);
//   clauses.push_back(text.length());
//   std::vector<int> target;
//   ime.CompositionUpdated(text, clauses, target, text.length());
//   ime.SendEvents(instance());
//
//   base::string16 result = "result";
//   ime.CompositionCompleted(result);
//   ime.SendEvents(instance());
//
// This class also provides GetStatus() so we can retrieve the IME status
// changed by a plug-in with IMM32 functions. This function is mainly used for
// retrieving the position of a caret.
//
class WebPluginIMEWin {
 public:
  // A simple class that allows a plug-in to access a WebPluginIMEWin instance
  // only in a scope.
  class ScopedLock {
   public:
    explicit ScopedLock(WebPluginIMEWin* instance) : instance_(instance) {
      if (instance_)
        instance_->Lock();
    }
    ~ScopedLock() {
      if (instance_)
        instance_->Unlock();
    }

   private:
    WebPluginIMEWin* instance_;
  };

  WebPluginIMEWin();
  ~WebPluginIMEWin();

  // Sends raw IME events sent from a browser to this IME emulator and updates
  // the list of Windows events to be sent to a plug-in. A raw IME event is
  // mapped to two or more Windows events and it is not so trivial to send these
  // Windows events to a plug-in. This function inserts Windows events in the
  // order expected by a plug-in.
  void CompositionUpdated(const base::string16& text,
                          std::vector<int> clauses,
                          std::vector<int> target,
                          int cursor_position);
  void CompositionCompleted(const base::string16& text);

  // Send all the events added in Update() to a plug-in.
  bool SendEvents(PluginInstance* instance);

  // Retrieves the status of this IME emulator.
  bool GetStatus(int* input_type, gfx::Rect* caret_rect);

  // Returns the pointers to IMM32-emulation functions implemented by this
  // class. This function is used for over-writing the ones returned from
  // GetProcAddress() calls of Win32 API.
  static FARPROC GetProcAddress(const char* name);

 private:
  // Allow (or disallow) the patch functions to use this WebPluginIMEWin
  // instance through our patch functions. Our patch functions need a static
  // member variable |instance_| to access a WebPluginIMEWIn instance. We lock
  // this static variable to prevent two or more plug-ins from accessing a
  // WebPluginIMEWin instance.
  void Lock();
  void Unlock();

  // Retrieve the instance of this class.
  static WebPluginIMEWin* GetInstance(HIMC context);

  // IMM32 patch functions implemented by this class.
  static BOOL WINAPI ImmAssociateContextEx(HWND window,
                                           HIMC context,
                                           DWORD flags);
  static LONG WINAPI ImmGetCompositionStringW(HIMC context,
                                              DWORD index,
                                              LPVOID dst_data,
                                              DWORD dst_size);
  static HIMC WINAPI ImmGetContext(HWND window);
  static BOOL WINAPI ImmReleaseContext(HWND window, HIMC context);
  static BOOL WINAPI ImmSetCandidateWindow(HIMC context,
                                           CANDIDATEFORM* candidate);
  static BOOL WINAPI ImmSetOpenStatus(HIMC context, BOOL open);

  // a list of NPEvents to be sent to a plug-in.
  std::vector<NPEvent> events_;

  // The return value for GCS_COMPSTR.
  base::string16 composition_text_;

  // The return value for GCS_RESULTSTR.
  base::string16 result_text_;

  // The return value for GCS_COMPATTR.
  std::string composition_attributes_;

  // The return value for GCS_COMPCLAUSE.
  std::vector<uint32> composition_clauses_;

  // The return value for GCS_RESULTCLAUSE.
  uint32 result_clauses_[2];

  // The return value for GCS_CURSORPOS.
  int cursor_position_;

  // The return value for GCS_DELTASTART.
  int delta_start_;

  // Whether we are composing text. This variable is used for sending a
  // WM_IME_STARTCOMPOSITION message when we start composing IME text.
  bool composing_text_;

  // Whether a plug-in supports IME messages. When a plug-in cannot handle
  // IME messages, we need to send the IME text with WM_CHAR messages as Windows
  // does.
  bool support_ime_messages_;

  // The IME status received from a plug-in.
  bool status_updated_;
  int input_type_;
  gfx::Rect caret_rect_;

  // The pointer to the WebPluginIMEWin instance used by patch functions.
  static WebPluginIMEWin* instance_;
};

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_WEBPLUGIN_IME_WIN_H_
