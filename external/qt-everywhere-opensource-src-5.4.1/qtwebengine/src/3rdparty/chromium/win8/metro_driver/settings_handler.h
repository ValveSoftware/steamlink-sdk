// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_DRIVER_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_METRO_DRIVER_SETTINGS_HANDLER_H_

#include <windows.ui.applicationsettings.h>
#include <windows.ui.popups.h>

#include "winrt_utils.h"

// This class handles the settings charm.
class SettingsHandler {
 public:
  SettingsHandler();
  ~SettingsHandler();

  HRESULT Initialize();

 private:
  typedef winfoundtn::Collections::IVector<
      winui::ApplicationSettings::SettingsCommand*> ISettingsCommandVector;

  HRESULT OnSettingsCommandsRequested(
      winui::ApplicationSettings::ISettingsPane* settings_pane,
      winui::ApplicationSettings::
          ISettingsPaneCommandsRequestedEventArgs* args);

  HRESULT AppendNewSettingsCommand(
      const wchar_t* id,
      const wchar_t* name,
      winui::ApplicationSettings::ISettingsCommandFactory*
          settings_command_factory,
      ISettingsCommandVector* settings_command_vector);

  static HRESULT OnSettings(winui::Popups::IUICommand* command);
  static HRESULT GetSettingsId(const wchar_t* value,
                               winfoundtn::IPropertyValue** settings_id);

  EventRegistrationToken settings_token_;
};

#endif  // CHROME_BROWSER_UI_METRO_DRIVER_SETTINGS_HANDLER_H_
