// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_H_
#define UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_H_

#include <windows.h>

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/win/window_event_target.h"
#include "ui/events/event.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/sequential_id_generator.h"
#include "ui/gfx/win/window_impl.h"
#include "ui/views/ime/input_method_delegate.h"
#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
class ImageSkia;
class Insets;
}

namespace ui  {
class ViewProp;
}

namespace views {

class FullscreenHandler;
class HWNDMessageHandlerDelegate;
class InputMethod;

// These two messages aren't defined in winuser.h, but they are sent to windows
// with captions. They appear to paint the window caption and frame.
// Unfortunately if you override the standard non-client rendering as we do
// with CustomFrameWindow, sometimes Windows (not deterministically
// reproducibly but definitely frequently) will send these messages to the
// window and paint the standard caption/title over the top of the custom one.
// So we need to handle these messages in CustomFrameWindow to prevent this
// from happening.
const int WM_NCUAHDRAWCAPTION = 0xAE;
const int WM_NCUAHDRAWFRAME = 0xAF;

// IsMsgHandled() and BEGIN_SAFE_MSG_MAP_EX are a modified version of
// BEGIN_MSG_MAP_EX. The main difference is it adds a WeakPtrFactory member
// (|weak_factory_|) that is used in _ProcessWindowMessage() and changing
// IsMsgHandled() from a member function to a define that checks if the weak
// factory is still valid in addition to the member. Together these allow for
// |this| to be deleted during dispatch.
#define IsMsgHandled() !ref.get() || msg_handled_

#define BEGIN_SAFE_MSG_MAP_EX(the_class) \
 private: \
  base::WeakPtrFactory<the_class> weak_factory_; \
  BOOL msg_handled_; \
\
 public: \
  /* "handled" management for cracked handlers */ \
  void SetMsgHandled(BOOL handled) { \
    msg_handled_ = handled; \
  } \
  BOOL ProcessWindowMessage(HWND hwnd, \
                            UINT msg, \
                            WPARAM w_param, \
                            LPARAM l_param, \
                            LRESULT& l_result, \
                            DWORD msg_map_id = 0) { \
    base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr()); \
    BOOL old_msg_handled = msg_handled_; \
    BOOL ret = _ProcessWindowMessage(hwnd, msg, w_param, l_param, l_result, \
                                     msg_map_id); \
    if (ref.get()) \
      msg_handled_ = old_msg_handled; \
    return ret; \
  } \
  BOOL _ProcessWindowMessage(HWND hWnd, \
                             UINT uMsg, \
                             WPARAM wParam, \
                             LPARAM lParam, \
                             LRESULT& lResult, \
                             DWORD dwMsgMapID) { \
    base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr()); \
    BOOL bHandled = TRUE; \
    hWnd; \
    uMsg; \
    wParam; \
    lParam; \
    lResult; \
    bHandled; \
    switch(dwMsgMapID) { \
      case 0:

// An object that handles messages for a HWND that implements the views
// "Custom Frame" look. The purpose of this class is to isolate the windows-
// specific message handling from the code that wraps it. It is intended to be
// used by both a views::NativeWidget and an aura::WindowTreeHost
// implementation.
// TODO(beng): This object should eventually *become* the WindowImpl.
class VIEWS_EXPORT HWNDMessageHandler :
    public gfx::WindowImpl,
    public internal::InputMethodDelegate,
    public ui::WindowEventTarget {
 public:
  explicit HWNDMessageHandler(HWNDMessageHandlerDelegate* delegate);
  ~HWNDMessageHandler();

  void Init(HWND parent, const gfx::Rect& bounds);
  void InitModalType(ui::ModalType modal_type);

  void Close();
  void CloseNow();

  gfx::Rect GetWindowBoundsInScreen() const;
  gfx::Rect GetClientAreaBoundsInScreen() const;
  gfx::Rect GetRestoredBounds() const;
  // This accounts for the case where the widget size is the client size.
  gfx::Rect GetClientAreaBounds() const;

  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const;

  // Sets the bounds of the HWND to |bounds_in_pixels|. If the HWND size is not
  // changed, |force_size_changed| determines if we should pretend it is.
  void SetBounds(const gfx::Rect& bounds_in_pixels, bool force_size_changed);

  void SetSize(const gfx::Size& size);
  void CenterWindow(const gfx::Size& size);

  void SetRegion(HRGN rgn);

  void StackAbove(HWND other_hwnd);
  void StackAtTop();

  void Show();
  void ShowWindowWithState(ui::WindowShowState show_state);
  void ShowMaximizedWithBounds(const gfx::Rect& bounds);
  void Hide();

  void Maximize();
  void Minimize();
  void Restore();

  void Activate();
  void Deactivate();

  void SetAlwaysOnTop(bool on_top);

  bool IsVisible() const;
  bool IsActive() const;
  bool IsMinimized() const;
  bool IsMaximized() const;
  bool IsAlwaysOnTop() const;

  bool RunMoveLoop(const gfx::Vector2d& drag_offset, bool hide_on_escape);
  void EndMoveLoop();

  // Tells the HWND its client area has changed.
  void SendFrameChanged();

  void FlashFrame(bool flash);

  void ClearNativeFocus();

  void SetCapture();
  void ReleaseCapture();
  bool HasCapture() const;

  FullscreenHandler* fullscreen_handler() { return fullscreen_handler_.get(); }

  void SetVisibilityChangedAnimationsEnabled(bool enabled);

  // Returns true if the title changed.
  bool SetTitle(const base::string16& title);

  void SetCursor(HCURSOR cursor);

  void FrameTypeChanged();

  void SchedulePaintInRect(const gfx::Rect& rect);
  void SetOpacity(BYTE opacity);

  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon);

  void set_remove_standard_frame(bool remove_standard_frame) {
    remove_standard_frame_ = remove_standard_frame;
  }

  void set_use_system_default_icon(bool use_system_default_icon) {
    use_system_default_icon_ = use_system_default_icon;
  }

 private:
  typedef std::set<DWORD> TouchIDs;

  // Overridden from internal::InputMethodDelegate:
  virtual void DispatchKeyEventPostIME(const ui::KeyEvent& key) OVERRIDE;

  // Overridden from WindowImpl:
  virtual HICON GetDefaultWindowIcon() const OVERRIDE;
  virtual LRESULT OnWndProc(UINT message,
                            WPARAM w_param,
                            LPARAM l_param) OVERRIDE;

  // Overridden from WindowEventTarget
  virtual LRESULT HandleMouseMessage(unsigned int message,
                                     WPARAM w_param,
                                     LPARAM l_param) OVERRIDE;
  virtual LRESULT HandleKeyboardMessage(unsigned int message,
                                        WPARAM w_param,
                                        LPARAM l_param) OVERRIDE;
  virtual LRESULT HandleTouchMessage(unsigned int message,
                                     WPARAM w_param,
                                     LPARAM l_param) OVERRIDE;

  virtual LRESULT HandleScrollMessage(unsigned int message,
                                      WPARAM w_param,
                                      LPARAM l_param) OVERRIDE;

  virtual LRESULT HandleNcHitTestMessage(unsigned int message,
                                         WPARAM w_param,
                                         LPARAM l_param) OVERRIDE;

  // Returns the auto-hide edges of the appbar. See
  // ViewsDelegate::GetAppbarAutohideEdges() for details. If the edges change,
  // OnAppbarAutohideEdgesChanged() is called.
  int GetAppbarAutohideEdges(HMONITOR monitor);

  // Callback if the autohide edges have changed. See
  // ViewsDelegate::GetAppbarAutohideEdges() for details.
  void OnAppbarAutohideEdgesChanged();

  // Can be called after the delegate has had the opportunity to set focus and
  // did not do so.
  void SetInitialFocus();

  // Called after the WM_ACTIVATE message has been processed by the default
  // windows procedure.
  void PostProcessActivateMessage(int activation_state, bool minimized);

  // Enables disabled owner windows that may have been disabled due to this
  // window's modality.
  void RestoreEnabledIfNecessary();

  // Executes the specified SC_command.
  void ExecuteSystemMenuCommand(int command);

  // Start tracking all mouse events so that this window gets sent mouse leave
  // messages too.
  void TrackMouseEvents(DWORD mouse_tracking_flags);

  // Responds to the client area changing size, either at window creation time
  // or subsequently.
  void ClientAreaSizeChanged();

  // Returns the insets of the client area relative to the non-client area of
  // the window.
  bool GetClientAreaInsets(gfx::Insets* insets) const;

  // Resets the window region for the current widget bounds if necessary.
  // If |force| is true, the window region is reset to NULL even for native
  // frame windows.
  void ResetWindowRegion(bool force, bool redraw);

  // Enables or disables rendering of the non-client (glass) area by DWM,
  // under Vista and above, depending on whether the caller has requested a
  // custom frame.
  void UpdateDwmNcRenderingPolicy();

  // Calls DefWindowProc, safely wrapping the call in a ScopedRedrawLock to
  // prevent frame flicker. DefWindowProc handling can otherwise render the
  // classic-look window title bar directly.
  LRESULT DefWindowProcWithRedrawLock(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param);

  // Lock or unlock the window from being able to redraw itself in response to
  // updates to its invalid region.
  class ScopedRedrawLock;
  void LockUpdates(bool force);
  void UnlockUpdates(bool force);

  // Stops ignoring SetWindowPos() requests (see below).
  void StopIgnoringPosChanges() { ignore_window_pos_changes_ = false; }

  // Synchronously updates the invalid contents of the Widget. Valid for
  // layered windows only.
  void RedrawLayeredWindowContents();

  // Attempts to force the window to be redrawn, ensuring that it gets
  // onscreen.
  void ForceRedrawWindow(int attempts);

  // Message Handlers ----------------------------------------------------------

  BEGIN_SAFE_MSG_MAP_EX(HWNDMessageHandler)
    // Range handlers must go first!
    CR_MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    CR_MESSAGE_RANGE_HANDLER_EX(WM_NCMOUSEMOVE,
                                WM_NCXBUTTONDBLCLK,
                                OnMouseRange)

    // CustomFrameWindow hacks
    CR_MESSAGE_HANDLER_EX(WM_NCUAHDRAWCAPTION, OnNCUAHDrawCaption)
    CR_MESSAGE_HANDLER_EX(WM_NCUAHDRAWFRAME, OnNCUAHDrawFrame)

    // Vista and newer
    CR_MESSAGE_HANDLER_EX(WM_DWMCOMPOSITIONCHANGED, OnDwmCompositionChanged)

    // Non-atlcrack.h handlers
    CR_MESSAGE_HANDLER_EX(WM_GETOBJECT, OnGetObject)

    // Mouse events.
    CR_MESSAGE_HANDLER_EX(WM_MOUSEACTIVATE, OnMouseActivate)
    CR_MESSAGE_HANDLER_EX(WM_MOUSELEAVE, OnMouseRange)
    CR_MESSAGE_HANDLER_EX(WM_NCMOUSELEAVE, OnMouseRange)
    CR_MESSAGE_HANDLER_EX(WM_SETCURSOR, OnSetCursor);

    // Key events.
    CR_MESSAGE_HANDLER_EX(WM_KEYDOWN, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_KEYUP, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSKEYDOWN, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSKEYUP, OnKeyEvent)

    // IME Events.
    CR_MESSAGE_HANDLER_EX(WM_IME_SETCONTEXT, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_STARTCOMPOSITION, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_COMPOSITION, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_ENDCOMPOSITION, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_REQUEST, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_IME_NOTIFY, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_CHAR, OnImeMessages)
    CR_MESSAGE_HANDLER_EX(WM_SYSCHAR, OnImeMessages)

    // Scroll events
    CR_MESSAGE_HANDLER_EX(WM_VSCROLL, OnScrollMessage)
    CR_MESSAGE_HANDLER_EX(WM_HSCROLL, OnScrollMessage)

    // Touch Events.
    CR_MESSAGE_HANDLER_EX(WM_TOUCH, OnTouchEvent)

    // Uses the general handler macro since the specific handler macro
    // MSG_WM_NCACTIVATE would convert WPARAM type to BOOL type. The high
    // word of WPARAM could be set when the window is minimized or restored.
    CR_MESSAGE_HANDLER_EX(WM_NCACTIVATE, OnNCActivate)

    // This list is in _ALPHABETICAL_ order! OR I WILL HURT YOU.
    CR_MSG_WM_ACTIVATEAPP(OnActivateApp)
    CR_MSG_WM_APPCOMMAND(OnAppCommand)
    CR_MSG_WM_CANCELMODE(OnCancelMode)
    CR_MSG_WM_CAPTURECHANGED(OnCaptureChanged)
    CR_MSG_WM_CLOSE(OnClose)
    CR_MSG_WM_COMMAND(OnCommand)
    CR_MSG_WM_CREATE(OnCreate)
    CR_MSG_WM_DESTROY(OnDestroy)
    CR_MSG_WM_DISPLAYCHANGE(OnDisplayChange)
    CR_MSG_WM_ENTERMENULOOP(OnEnterMenuLoop)
    CR_MSG_WM_EXITMENULOOP(OnExitMenuLoop)
    CR_MSG_WM_ENTERSIZEMOVE(OnEnterSizeMove)
    CR_MSG_WM_ERASEBKGND(OnEraseBkgnd)
    CR_MSG_WM_EXITSIZEMOVE(OnExitSizeMove)
    CR_MSG_WM_GETMINMAXINFO(OnGetMinMaxInfo)
    CR_MSG_WM_INITMENU(OnInitMenu)
    CR_MSG_WM_INPUTLANGCHANGE(OnInputLangChange)
    CR_MSG_WM_KILLFOCUS(OnKillFocus)
    CR_MSG_WM_MOVE(OnMove)
    CR_MSG_WM_MOVING(OnMoving)
    CR_MSG_WM_NCCALCSIZE(OnNCCalcSize)
    CR_MSG_WM_NCHITTEST(OnNCHitTest)
    CR_MSG_WM_NCPAINT(OnNCPaint)
    CR_MSG_WM_NOTIFY(OnNotify)
    CR_MSG_WM_PAINT(OnPaint)
    CR_MSG_WM_SETFOCUS(OnSetFocus)
    CR_MSG_WM_SETICON(OnSetIcon)
    CR_MSG_WM_SETTEXT(OnSetText)
    CR_MSG_WM_SETTINGCHANGE(OnSettingChange)
    CR_MSG_WM_SIZE(OnSize)
    CR_MSG_WM_SYSCOMMAND(OnSysCommand)
    CR_MSG_WM_THEMECHANGED(OnThemeChanged)
    CR_MSG_WM_WINDOWPOSCHANGED(OnWindowPosChanged)
    CR_MSG_WM_WINDOWPOSCHANGING(OnWindowPosChanging)
    CR_MSG_WM_WTSSESSION_CHANGE(OnSessionChange)
  CR_END_MSG_MAP()

  // Message Handlers.
  // This list is in _ALPHABETICAL_ order!
  // TODO(beng): Once this object becomes the WindowImpl, these methods can
  //             be made private.
  void OnActivateApp(BOOL active, DWORD thread_id);
  // TODO(beng): return BOOL is temporary until this object becomes a
  //             WindowImpl.
  BOOL OnAppCommand(HWND window, short command, WORD device, int keystate);
  void OnCancelMode();
  void OnCaptureChanged(HWND window);
  void OnClose();
  void OnCommand(UINT notification_code, int command, HWND window);
  LRESULT OnCreate(CREATESTRUCT* create_struct);
  void OnDestroy();
  void OnDisplayChange(UINT bits_per_pixel, const gfx::Size& screen_size);
  LRESULT OnDwmCompositionChanged(UINT msg, WPARAM w_param, LPARAM l_param);
  void OnEnterMenuLoop(BOOL from_track_popup_menu);
  void OnEnterSizeMove();
  LRESULT OnEraseBkgnd(HDC dc);
  void OnExitMenuLoop(BOOL is_shortcut_menu);
  void OnExitSizeMove();
  void OnGetMinMaxInfo(MINMAXINFO* minmax_info);
  LRESULT OnGetObject(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnImeMessages(UINT message, WPARAM w_param, LPARAM l_param);
  void OnInitMenu(HMENU menu);
  void OnInputLangChange(DWORD character_set, HKL input_language_id);
  LRESULT OnKeyEvent(UINT message, WPARAM w_param, LPARAM l_param);
  void OnKillFocus(HWND focused_window);
  LRESULT OnMouseActivate(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param);
  void OnMove(const gfx::Point& point);
  void OnMoving(UINT param, const RECT* new_bounds);
  LRESULT OnNCActivate(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);
  LRESULT OnNCHitTest(const gfx::Point& point);
  void OnNCPaint(HRGN rgn);
  LRESULT OnNCUAHDrawCaption(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCUAHDrawFrame(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNotify(int w_param, NMHDR* l_param);
  void OnPaint(HDC dc);
  LRESULT OnReflectedMessage(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnScrollMessage(UINT message, WPARAM w_param, LPARAM l_param);
  void OnSessionChange(WPARAM status_code, PWTSSESSION_NOTIFICATION session_id);
  LRESULT OnSetCursor(UINT message, WPARAM w_param, LPARAM l_param);
  void OnSetFocus(HWND last_focused_window);
  LRESULT OnSetIcon(UINT size_type, HICON new_icon);
  LRESULT OnSetText(const wchar_t* text);
  void OnSettingChange(UINT flags, const wchar_t* section);
  void OnSize(UINT param, const gfx::Size& size);
  void OnSysCommand(UINT notification_code, const gfx::Point& point);
  void OnThemeChanged();
  LRESULT OnTouchEvent(UINT message, WPARAM w_param, LPARAM l_param);
  void OnWindowPosChanging(WINDOWPOS* window_pos);
  void OnWindowPosChanged(WINDOWPOS* window_pos);

  typedef std::vector<ui::TouchEvent> TouchEvents;
  // Helper to handle the list of touch events passed in. We need this because
  // touch events on windows don't fire if we enter a modal loop in the context
  // of a touch event.
  void HandleTouchEvents(const TouchEvents& touch_events);

  // Resets the flag which indicates that we are in the context of a touch down
  // event.
  void ResetTouchDownContext();

  // Helper to handle mouse events.
  // The |message|, |w_param|, |l_param| parameters identify the Windows mouse
  // message and its parameters respectively.
  // The |track_mouse| parameter indicates if we should track the mouse.
  LRESULT HandleMouseEventInternal(UINT message,
                                   WPARAM w_param,
                                   LPARAM l_param,
                                   bool track_mouse);

  // Returns true if the mouse message passed in is an OS synthesized mouse
  // message.
  // |message| identifies the mouse message.
  // |message_time| is the time when the message occurred.
  // |l_param| indicates the location of the mouse message.
  bool IsSynthesizedMouseMessage(unsigned int message,
                                 int message_time,
                                 LPARAM l_param);

  HWNDMessageHandlerDelegate* delegate_;

  scoped_ptr<FullscreenHandler> fullscreen_handler_;

  // Set to true in Close() and false is CloseNow().
  bool waiting_for_close_now_;

  bool remove_standard_frame_;

  bool use_system_default_icon_;

  // Whether all ancestors have been enabled. This is only used if is_modal_ is
  // true.
  bool restored_enabled_;

  // The current cursor.
  HCURSOR current_cursor_;

  // The last cursor that was active before the current one was selected. Saved
  // so that we can restore it.
  HCURSOR previous_cursor_;

  // Event handling ------------------------------------------------------------

  // The flags currently being used with TrackMouseEvent to track mouse
  // messages. 0 if there is no active tracking. The value of this member is
  // used when tracking is canceled.
  DWORD active_mouse_tracking_flags_;

  // Set to true when the user presses the right mouse button on the caption
  // area. We need this so we can correctly show the context menu on mouse-up.
  bool is_right_mouse_pressed_on_caption_;

  // The set of touch devices currently down.
  TouchIDs touch_ids_;

  // ScopedRedrawLock ----------------------------------------------------------

  // Represents the number of ScopedRedrawLocks active against this widget.
  // If this is greater than zero, the widget should be locked against updates.
  int lock_updates_count_;

  // Window resizing -----------------------------------------------------------

  // When true, this flag makes us discard incoming SetWindowPos() requests that
  // only change our position/size.  (We still allow changes to Z-order,
  // activation, etc.)
  bool ignore_window_pos_changes_;

  // The last-seen monitor containing us, and its rect and work area.  These are
  // used to catch updates to the rect and work area and react accordingly.
  HMONITOR last_monitor_;
  gfx::Rect last_monitor_rect_, last_work_area_;

  // Layered windows -----------------------------------------------------------

  // Should we keep an off-screen buffer? This is false by default, set to true
  // when WS_EX_LAYERED is specified before the native window is created.
  //
  // NOTE: this is intended to be used with a layered window (a window with an
  // extended window style of WS_EX_LAYERED). If you are using a layered window
  // and NOT changing the layered alpha or anything else, then leave this value
  // alone. OTOH if you are invoking SetLayeredWindowAttributes then you'll
  // most likely want to set this to false, or after changing the alpha toggle
  // the extended style bit to false than back to true. See MSDN for more
  // details.
  bool use_layered_buffer_;

  // The default alpha to be applied to the layered window.
  BYTE layered_alpha_;

  // A canvas that contains the window contents in the case of a layered
  // window.
  scoped_ptr<gfx::Canvas> layered_window_contents_;

  // We must track the invalid rect ourselves, for two reasons:
  // For layered windows, Windows will not do this properly with
  // InvalidateRect()/GetUpdateRect(). (In fact, it'll return misleading
  // information from GetUpdateRect()).
  // We also need to keep track of the invalid rectangle for the RootView should
  // we need to paint the non-client area. The data supplied to WM_NCPAINT seems
  // to be insufficient.
  gfx::Rect invalid_rect_;

  // Set to true when waiting for RedrawLayeredWindowContents().
  bool waiting_for_redraw_layered_window_contents_;

  // True the first time nccalc is called on a sizable widget
  bool is_first_nccalc_;

  // Copy of custom window region specified via SetRegion(), if any.
  base::win::ScopedRegion custom_window_region_;

  // If > 0 indicates a menu is running (we're showing a native menu).
  int menu_depth_;

  // A factory used to lookup appbar autohide edges.
  base::WeakPtrFactory<HWNDMessageHandler> autohide_factory_;

  // Generates touch-ids for touch-events.
  ui::SequentialIDGenerator id_generator_;

  // Indicates if the window needs the WS_VSCROLL and WS_HSCROLL styles.
  bool needs_scroll_styles_;

  // Set to true if we are in the context of a sizing operation.
  bool in_size_loop_;

  // Stores a pointer to the WindowEventTarget interface implemented by this
  // class. Allows callers to retrieve the interface pointer.
  scoped_ptr<ui::ViewProp> prop_window_target_;

  // Number of active touch down contexts. This is incremented on touch down
  // events and decremented later using a delayed task.
  // We need this to ignore WM_MOUSEACTIVATE messages generated in response to
  // touch input. This is fine because activation still works correctly via
  // native SetFocus calls invoked in the views code.
  int touch_down_contexts_;

  // Time the last touch message was received. Used to flag mouse messages
  // synthesized by Windows for touch which are not flagged by the OS as
  // synthesized mouse messages. For more information please refer to
  // the IsMouseEventFromTouch function.
  static long last_touch_message_time_;

  // Time the last WM_MOUSEHWHEEL message is received. Please refer to the
  // HandleMouseEventInternal function as to why this is needed.
  long last_mouse_hwheel_time_;

  DISALLOW_COPY_AND_ASSIGN(HWNDMessageHandler);
};

}  // namespace views

#endif  // UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_H_
