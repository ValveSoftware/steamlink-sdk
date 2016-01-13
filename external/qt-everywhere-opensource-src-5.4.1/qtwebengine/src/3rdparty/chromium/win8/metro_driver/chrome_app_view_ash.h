// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_CHROME_APP_VIEW_ASH_H_
#define WIN8_METRO_DRIVER_CHROME_APP_VIEW_ASH_H_

#include <windows.applicationmodel.core.h>
#include <windows.ui.core.h>
#include <windows.ui.input.h>
#include <windows.ui.viewmanagement.h>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "ui/events/event_constants.h"
#include "win8/metro_driver/direct3d_helper.h"
#include "win8/metro_driver/ime/ime_popup_observer.h"
#include "win8/metro_driver/ime/input_source_observer.h"
#include "win8/metro_driver/ime/text_service_delegate.h"

namespace base {
class FilePath;
}

namespace IPC {
class Listener;
class ChannelProxy;
}

namespace metro_driver {
class InputSource;
class TextService;
}

namespace metro_viewer {
struct CharacterBounds;
struct UnderlineInfo;
}

class OpenFilePickerSession;
class SaveFilePickerSession;
class FolderPickerSession;
class FilePickerSessionBase;

struct MetroViewerHostMsg_SaveAsDialogParams;

enum MetroTerminateMethod {
  TERMINATE_USING_KEY_SEQUENCE = 1,
  TERMINATE_USING_PROCESS_EXIT = 2,
};

class ChromeAppViewAsh
    : public mswr::RuntimeClass<winapp::Core::IFrameworkView>,
      public metro_driver::ImePopupObserver,
      public metro_driver::InputSourceObserver,
      public metro_driver::TextServiceDelegate {
 public:
  ChromeAppViewAsh();
  ~ChromeAppViewAsh();

  // IViewProvider overrides.
  IFACEMETHOD(Initialize)(winapp::Core::ICoreApplicationView* view);
  IFACEMETHOD(SetWindow)(winui::Core::ICoreWindow* window);
  IFACEMETHOD(Load)(HSTRING entryPoint);
  IFACEMETHOD(Run)();
  IFACEMETHOD(Uninitialize)();

  // Helper function to unsnap the chrome metro app if it is snapped.
  // Returns S_OK on success.
  static HRESULT Unsnap();

  void OnActivateDesktop(const base::FilePath& file_path, bool ash_exit);
  void OnOpenURLOnDesktop(const base::FilePath& shortcut,
                          const base::string16& url);
  void OnSetCursor(HCURSOR cursor);
  void OnDisplayFileOpenDialog(const base::string16& title,
                               const base::string16& filter,
                               const base::FilePath& default_path,
                               bool allow_multiple_files);
  void OnDisplayFileSaveAsDialog(
      const MetroViewerHostMsg_SaveAsDialogParams& params);
  void OnDisplayFolderPicker(const base::string16& title);
  void OnSetCursorPos(int x, int y);

  // This function is invoked when the open file operation completes. The
  // result of the operation is passed in along with the OpenFilePickerSession
  // instance which is deleted after we read the required information from
  // the OpenFilePickerSession class.
  void OnOpenFileCompleted(OpenFilePickerSession* open_file_picker,
                           bool success);

  // This function is invoked when the save file operation completes. The
  // result of the operation is passed in along with the SaveFilePickerSession
  // instance which is deleted after we read the required information from
  // the SaveFilePickerSession class.
  void OnSaveFileCompleted(SaveFilePickerSession* save_file_picker,
                           bool success);

  // This function is invoked when the folder picker operation completes. The
  // result of the operation is passed in along with the FolderPickerSession
  // instance which is deleted after we read the required information from
  // the FolderPickerSession class.
  void OnFolderPickerCompleted(FolderPickerSession* folder_picker,
                               bool success);

  void OnImeCancelComposition();
  void OnImeUpdateTextInputClient(
      const std::vector<int32>& input_scopes,
      const std::vector<metro_viewer::CharacterBounds>& character_bounds);

  void OnMetroExit(MetroTerminateMethod method);

  HWND core_window_hwnd() const { return  core_window_hwnd_; }


 private:
  class PointerInfoHandler;

  // ImePopupObserver overrides.
  virtual void OnImePopupChanged(ImePopupObserver::EventType event) OVERRIDE;

  // InputSourceObserver overrides.
  virtual void OnInputSourceChanged() OVERRIDE;

  // TextServiceDelegate overrides.
  virtual void OnCompositionChanged(
      const base::string16& text,
      int32 selection_start,
      int32 selection_end,
      const std::vector<metro_viewer::UnderlineInfo>& underlines) OVERRIDE;
  virtual void OnTextCommitted(const base::string16& text) OVERRIDE;

  // Convenience for sending a MetroViewerHostMsg_MouseButton with the specified
  // parameters.
  void SendMouseButton(int x,
                       int y,
                       int extra,
                       ui::EventType event_type,
                       uint32 flags,
                       ui::EventFlags changed_button,
                       bool is_horizontal_wheel);

  // Win8 only generates a mouse press for the initial button that goes down and
  // a release when the last button is released. Any intermediary presses (or
  // releases) do not result in a new press/release event. Instead a move is
  // generated with the winui::Input::PointerUpdateKind identifying what
  // changed. This function generates the necessary intermediary events (as
  // necessary).
  void GenerateMouseEventFromMoveIfNecessary(const PointerInfoHandler& pointer);

  HRESULT OnActivate(winapp::Core::ICoreApplicationView* view,
                     winapp::Activation::IActivatedEventArgs* args);

  HRESULT OnPointerMoved(winui::Core::ICoreWindow* sender,
                         winui::Core::IPointerEventArgs* args);

  HRESULT OnPointerPressed(winui::Core::ICoreWindow* sender,
                           winui::Core::IPointerEventArgs* args);

  HRESULT OnPointerReleased(winui::Core::ICoreWindow* sender,
                            winui::Core::IPointerEventArgs* args);

  HRESULT OnWheel(winui::Core::ICoreWindow* sender,
                  winui::Core::IPointerEventArgs* args);

  HRESULT OnKeyDown(winui::Core::ICoreWindow* sender,
                    winui::Core::IKeyEventArgs* args);

  HRESULT OnKeyUp(winui::Core::ICoreWindow* sender,
                  winui::Core::IKeyEventArgs* args);

  // Invoked for system keys like Alt, etc.
  HRESULT OnAcceleratorKeyDown(winui::Core::ICoreDispatcher* sender,
                               winui::Core::IAcceleratorKeyEventArgs* args);

  HRESULT OnCharacterReceived(winui::Core::ICoreWindow* sender,
                              winui::Core::ICharacterReceivedEventArgs* args);

  HRESULT OnWindowActivated(winui::Core::ICoreWindow* sender,
                            winui::Core::IWindowActivatedEventArgs* args);

  // Helper to handle search requests received via the search charm in ASH.
  HRESULT HandleSearchRequest(winapp::Activation::IActivatedEventArgs* args);
  // Helper to handle http/https url requests in ASH.
  HRESULT HandleProtocolRequest(winapp::Activation::IActivatedEventArgs* args);

  HRESULT OnEdgeGestureCompleted(winui::Input::IEdgeGesture* gesture,
                                 winui::Input::IEdgeGestureEventArgs* args);

  // Tasks posted to the UI thread to initiate the search/url navigation
  // requests.
  void OnSearchRequest(const base::string16& search_string);
  void OnNavigateToUrl(const base::string16& url);

  HRESULT OnSizeChanged(winui::Core::ICoreWindow* sender,
                        winui::Core::IWindowSizeChangedEventArgs* args);

  mswr::ComPtr<winui::Core::ICoreWindow> window_;
  mswr::ComPtr<winapp::Core::ICoreApplicationView> view_;
  EventRegistrationToken activated_token_;
  EventRegistrationToken pointermoved_token_;
  EventRegistrationToken pointerpressed_token_;
  EventRegistrationToken pointerreleased_token_;
  EventRegistrationToken wheel_token_;
  EventRegistrationToken keydown_token_;
  EventRegistrationToken keyup_token_;
  EventRegistrationToken character_received_token_;
  EventRegistrationToken accel_keydown_token_;
  EventRegistrationToken accel_keyup_token_;
  EventRegistrationToken window_activated_token_;
  EventRegistrationToken sizechange_token_;
  EventRegistrationToken edgeevent_token_;

  // Keep state about which button is currently down, if any, as PointerMoved
  // events do not contain that state, but Ash's MouseEvents need it. Value is
  // as a bitmask of ui::EventFlags.
  uint32 mouse_down_flags_;

  // Set the D3D swap chain and nothing else.
  metro_driver::Direct3DHelper direct3d_helper_;

  // The channel to Chrome, in particular to the MetroViewerProcessHost.
  IPC::ChannelProxy* ui_channel_;

  // The actual window behind the view surface.
  HWND core_window_hwnd_;

  // UI message loop to allow message passing into this thread.
  base::MessageLoopForUI ui_loop_;

  // For IME support.
  scoped_ptr<metro_driver::InputSource> input_source_;
  scoped_ptr<metro_driver::TextService> text_service_;

  // The metro device scale factor as reported by the winrt interfaces.
  float metro_dpi_scale_;
  // The win32 dpi scale which is queried via GetDeviceCaps. Please refer to
  // ui/gfx/win/dpi.cc for more information.
  float win32_dpi_scale_;
};

#endif  // WIN8_METRO_DRIVER_CHROME_APP_VIEW_ASH_H_
