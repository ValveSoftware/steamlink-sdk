// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_DRIVER_METRO_DIALOG_BOX_H_
#define CHROME_BROWSER_UI_METRO_DRIVER_METRO_DIALOG_BOX_H_

#include <windows.ui.popups.h>
#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/win/metro.h"

// Provides functionality to display a dialog box
class MetroDialogBox : public winui::Popups::IUICommandInvokedHandler {
 public:
  struct DialogBoxInfo {
    base::string16 title;
    base::string16 content;
    base::string16 button1_label;
    base::string16 button2_label;
    base::win::MetroDialogButtonPressedHandler button1_handler;
    base::win::MetroDialogButtonPressedHandler button2_handler;
  };

  MetroDialogBox();
  ~MetroDialogBox();

  // Displays the dialog box.
  void Show(const DialogBoxInfo& dialog_box_info);

  // Dismisses the dialog box.
  void Dismiss();

  // IUICommandInvokedHandler implementation.
  // Dummy implementation of IUnknown. This is fine as the lifetime of this
  // class is tied to the lifetime of the ChromeAppView instance.
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) {
    DVLOG(1) << __FUNCTION__;
    CHECK(false);
    return E_NOINTERFACE;
  }

  virtual ULONG STDMETHODCALLTYPE AddRef(void) {
    DVLOG(1) << __FUNCTION__;
    return 1;
  }

  virtual ULONG STDMETHODCALLTYPE Release(void) {
    DVLOG(1) << __FUNCTION__;
    return 1;
  }

  virtual HRESULT STDMETHODCALLTYPE Invoke(winui::Popups::IUICommand* command);

 private:
  // The actual dialog box.
  mswr::ComPtr<winui::Popups::IMessageDialog> dialog_box_;
  DialogBoxInfo dialog_box_info_;
};

#endif  // CHROME_BROWSER_UI_METRO_DRIVER_METRO_DIALOG_BOX_H_

