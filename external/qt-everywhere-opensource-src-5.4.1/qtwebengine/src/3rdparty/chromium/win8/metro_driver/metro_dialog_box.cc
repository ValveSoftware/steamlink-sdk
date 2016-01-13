// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/metro_driver/stdafx.h"

#include "win8/metro_driver/chrome_app_view.h"
#include "win8/metro_driver/metro_dialog_box.h"
#include "win8/metro_driver/winrt_utils.h"

typedef winfoundtn::Collections::
    IVector<ABI::Windows::UI::Popups::IUICommand*> WindowsUICommands;

typedef winfoundtn::IAsyncOperation<ABI::Windows::UI::Popups::IUICommand*>
    AsyncCommandStatus;

MetroDialogBox::MetroDialogBox() {
  DVLOG(1) << __FUNCTION__;
  dialog_box_info_.button1_handler = NULL;
  dialog_box_info_.button2_handler = NULL;
}

MetroDialogBox::~MetroDialogBox() {
  DVLOG(1) << __FUNCTION__;
}

void MetroDialogBox::Show(
    const DialogBoxInfo& dialog_box_info) {
  DVLOG(1) << __FUNCTION__;

  // Only one dialog can be displayed at a given time.
  DCHECK(dialog_box_.Get() == NULL);

  // The message dialog display does not work correctly in snapped mode.
  mswr::ComPtr<winui::Popups::IMessageDialogFactory> message_dialog_factory;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_Popups_MessageDialog,
      message_dialog_factory.GetAddressOf());
  CheckHR(hr, "Failed to activate IMessageDialogFactory");

  mswrw::HString message_title;
  message_title.Attach(MakeHString(dialog_box_info.title));

  mswrw::HString message_content;
  message_content.Attach(MakeHString(dialog_box_info.content));

  hr = message_dialog_factory->CreateWithTitle(
      message_content.Get(),
      message_title.Get(),
      dialog_box_.GetAddressOf());
  CheckHR(hr, "Failed to create message dialog");

  mswr::ComPtr<WindowsUICommands> commands;
  hr = dialog_box_->get_Commands(commands.GetAddressOf());
  CheckHR(hr, "Failed to create ui command collection");

  mswr::ComPtr<winui::Popups::IUICommandFactory> ui_command_factory;
  hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_Popups_UICommand,
      ui_command_factory.GetAddressOf());
  CheckHR(hr, "Failed to activate IUICommandFactory");

  mswrw::HString label1;
  label1.Attach(MakeHString(dialog_box_info.button1_label));

  mswr::ComPtr<winui::Popups::IUICommand> label1_command;
  hr = ui_command_factory->CreateWithHandler(
      label1.Get(), this, label1_command.GetAddressOf());
  CheckHR(hr, "Failed to add button1");

  mswrw::HString label2;
  label2.Attach(MakeHString(dialog_box_info.button2_label));

  mswr::ComPtr<winui::Popups::IUICommand> label2_command;
  hr = ui_command_factory->CreateWithHandler(label2.Get(), this,
                                             label2_command.GetAddressOf());
  CheckHR(hr, "Failed to add button2");

  commands->Append(label1_command.Get());
  commands->Append(label2_command.Get());

  mswr::ComPtr<AsyncCommandStatus> ret;
  hr = dialog_box_->ShowAsync(ret.GetAddressOf());
  CheckHR(hr, "Failed to show dialog");

  dialog_box_info_ = dialog_box_info;
}

// The dialog box displayed via the MessageDialog interface has the class name
// 'Shell_Dialog'. The dialog box is top level window. To find it we enumerate
// all top level windows and compare the class names. If we find a matching
// window class we compare its process id with ours and return the same.
BOOL CALLBACK DialogBoxFinder(HWND hwnd, LPARAM lparam) {
  char classname[MAX_PATH] = {0};

  if (::GetClassNameA(hwnd, classname, ARRAYSIZE(classname))) {
    if (lstrcmpiA("Shell_Dialog", classname) == 0) {
      if (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) {
        DVLOG(1) << "Found top most dialog box: " << classname;
        DVLOG(1) << "HWND: " << hwnd;
        DWORD window_pid = 0;
        DWORD window_tid = GetWindowThreadProcessId(hwnd, &window_pid);
        DVLOG(1) << "Window tid: " << window_tid;
        DVLOG(1) << "Window pid: " << window_pid;

        if (window_pid == ::GetCurrentProcessId()) {
          HWND* dialog_window = reinterpret_cast<HWND*>(lparam);
          *dialog_window = hwnd;
          return FALSE;
        }
      }
    }
  }
  return TRUE;
}

void MetroDialogBox::Dismiss() {
  DVLOG(1) << __FUNCTION__;
  if (!dialog_box_)
    return;

  dialog_box_info_.button1_handler = NULL;
  dialog_box_info_.button2_handler = NULL;
  dialog_box_info_.button1_label.clear();
  dialog_box_info_.button2_label.clear();
  dialog_box_.Reset();

  // We don't have a good way to dismiss the dialog box. Hack for now is to
  // find the dialog box class in our process and close it via the WM_CLOSE
  // message.
  HWND dialog_box = NULL;
  ::EnumWindows(&DialogBoxFinder, reinterpret_cast<LPARAM>(&dialog_box));
  if (::IsWindow(dialog_box))
    PostMessage(dialog_box, WM_CLOSE, 0, 0);
}

HRESULT STDMETHODCALLTYPE MetroDialogBox::Invoke(
    winui::Popups::IUICommand* command) {
  DVLOG(1) << __FUNCTION__;

  mswrw::HString label;
  command->get_Label(label.GetAddressOf());

  base::string16 button_label = MakeStdWString(label.Get());
  DVLOG(1) << "Clicked button label is : " << button_label;
  if (button_label == dialog_box_info_.button1_label) {
    DVLOG(1) << "Button1 clicked";
    DCHECK(dialog_box_info_.button1_handler);
    dialog_box_info_.button1_handler();
  } else if (button_label == dialog_box_info_.button2_label) {
    DVLOG(1) << "Button2 clicked";
    DCHECK(dialog_box_info_.button2_handler);
    dialog_box_info_.button2_handler();
  }
  // The dialog box is destroyed once we return from invoke. Go ahead and
  // dismiss it.
  Dismiss();
  return S_OK;
}

