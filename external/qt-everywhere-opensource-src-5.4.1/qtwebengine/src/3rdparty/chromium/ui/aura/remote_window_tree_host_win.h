// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_REMOTE_WINDOW_TREE_HOST_WIN_H_
#define UI_AURA_REMOTE_WINDOW_TREE_HOST_WIN_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/remote_input_method_delegate_win.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/metro_viewer/ime_types.h"

struct MetroViewerHostMsg_MouseButtonParams;

namespace base {
class FilePath;
}

namespace ui {
class RemoteInputMethodPrivateWin;
class ViewProp;
}

namespace IPC {
class Message;
class Sender;
}

namespace aura {

// WindowTreeHost implementaton that receives events from a different
// process. In the case of Windows this is the Windows 8 (aka Metro)
// frontend process, which forwards input events to this class.
class AURA_EXPORT RemoteWindowTreeHostWin
    : public WindowTreeHost,
      public ui::EventSource,
      public ui::internal::RemoteInputMethodDelegateWin {
 public:
  // Returns the current RemoteWindowTreeHostWin. This does *not* create a
  // RemoteWindowTreeHostWin.
  static RemoteWindowTreeHostWin* Instance();

  // Returns true if there is a RemoteWindowTreeHostWin and it has a valid
  // HWND. A return value of false typically indicates we're not in metro mode.
  static bool IsValid();

  // Sets the handle to the remote window. The |remote_window| is the actual
  // window owned by the viewer process. Call this before Connected() for some
  // customers like input method initialization which needs the handle.
  void SetRemoteWindowHandle(HWND remote_window);
  HWND remote_window() { return remote_window_; }

  // The |host| can be used when we need to send a message to it.
  void Connected(IPC::Sender* host);
  // Called when the remote process has closed its IPC connection.
  void Disconnected();

  // Called when we have a message from the remote process.
  bool OnMessageReceived(const IPC::Message& message);

  void HandleOpenURLOnDesktop(const base::FilePath& shortcut,
                              const base::string16& url);

  void HandleWindowSizeChanged(uint32 width, uint32 height);

  // Returns the active ASH root window.
  Window* GetAshWindow();

  // Returns true if the remote window is the foreground window according to the
  // OS.
  bool IsForegroundWindow();

 protected:
  RemoteWindowTreeHostWin();
  virtual ~RemoteWindowTreeHostWin();

 private:
  // IPC message handing methods:
  void OnMouseMoved(int32 x, int32 y, int32 flags);
  void OnMouseButton(const MetroViewerHostMsg_MouseButtonParams& params);
  void OnKeyDown(uint32 vkey,
                 uint32 repeat_count,
                 uint32 scan_code,
                 uint32 flags);
  void OnKeyUp(uint32 vkey,
               uint32 repeat_count,
               uint32 scan_code,
               uint32 flags);
  void OnChar(uint32 key_code,
              uint32 repeat_count,
              uint32 scan_code,
              uint32 flags);
  void OnWindowActivated();
  void OnEdgeGesture();
  void OnTouchDown(int32 x, int32 y, uint64 timestamp, uint32 pointer_id);
  void OnTouchUp(int32 x, int32 y, uint64 timestamp, uint32 pointer_id);
  void OnTouchMoved(int32 x, int32 y, uint64 timestamp, uint32 pointer_id);
  void OnSetCursorPosAck();

  // For Input Method support:
  ui::RemoteInputMethodPrivateWin* GetRemoteInputMethodPrivate();
  void OnImeCandidatePopupChanged(bool visible);
  void OnImeCompositionChanged(
      const base::string16& text,
      int32 selection_start,
      int32 selection_end,
      const std::vector<metro_viewer::UnderlineInfo>& underlines);
  void OnImeTextCommitted(const base::string16& text);
  void OnImeInputSourceChanged(uint16 language_id, bool is_ime);

  // WindowTreeHost overrides:
  virtual ui::EventSource* GetEventSource() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void SetCursorNative(gfx::NativeCursor cursor) OVERRIDE;
  virtual void MoveCursorToNative(const gfx::Point& location) OVERRIDE;
  virtual void OnCursorVisibilityChangedNative(bool show) OVERRIDE;

  // ui::EventSource:
  virtual ui::EventProcessor* GetEventProcessor() OVERRIDE;

  // ui::internal::RemoteInputMethodDelegateWin overrides:
  virtual void CancelComposition() OVERRIDE;
  virtual void OnTextInputClientUpdated(
      const std::vector<int32>& input_scopes,
      const std::vector<gfx::Rect>& composition_character_bounds) OVERRIDE;

  // Helper function to dispatch a keyboard message to the desired target.
  // The default target is the WindowEventDispatcher. For nested message loop
  // invocations we post a synthetic keyboard message directly into the message
  // loop. The dispatcher for the nested loop would then decide how this
  // message is routed.
  void DispatchKeyboardMessage(ui::EventType type,
                               uint32 vkey,
                               uint32 repeat_count,
                               uint32 scan_code,
                               uint32 flags,
                               bool is_character);

  // Sets the event flags. |flags| is a bitmask of EventFlags. If there is a
  // change the system virtual key state is updated as well. This way if chrome
  // queries for key state it matches that of event being dispatched.
  void SetEventFlags(uint32 flags);

  uint32 mouse_event_flags() const {
    return event_flags_ & (ui::EF_LEFT_MOUSE_BUTTON |
                           ui::EF_MIDDLE_MOUSE_BUTTON |
                           ui::EF_RIGHT_MOUSE_BUTTON);
  }

  uint32 key_event_flags() const {
    return event_flags_ & (ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                           ui::EF_ALT_DOWN | ui::EF_CAPS_LOCK_DOWN);
  }

  HWND remote_window_;
  IPC::Sender* host_;
  scoped_ptr<ui::ViewProp> prop_;

  // Set to true if we need to ignore mouse messages until the SetCursorPos
  // operation is acked by the viewer.
  bool ignore_mouse_moves_until_set_cursor_ack_;

  // Tracking last click event for synthetically generated mouse events.
  scoped_ptr<ui::MouseEvent> last_mouse_click_event_;

  // State of the keyboard/mouse at the time of the last input event. See
  // description of SetEventFlags().
  uint32 event_flags_;

  // Current size of this root window.
  gfx::Size window_size_;

  DISALLOW_COPY_AND_ASSIGN(RemoteWindowTreeHostWin);
};

}  // namespace aura

#endif  // UI_AURA_REMOTE_WINDOW_TREE_HOST_WIN_H_
