// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "settings_handler.h"

// This include allows to send WM_SYSCOMMANDs to chrome.
#include "chrome/app/chrome_command_ids.h"
#include "chrome_app_view.h"
#include "winrt_utils.h"

typedef winfoundtn::ITypedEventHandler<
    winui::ApplicationSettings::SettingsPane*,
    winui::ApplicationSettings::SettingsPaneCommandsRequestedEventArgs*>
        CommandsRequestedHandler;

namespace {

// String identifiers for the settings pane commands.
const wchar_t* kSettingsId = L"settings";
const wchar_t* kHelpId = L"help";
const wchar_t* kAboutId = L"about";

}

SettingsHandler::SettingsHandler() {
  DVLOG(1) << __FUNCTION__;
}

SettingsHandler::~SettingsHandler() {
  DVLOG(1) << __FUNCTION__;
}

HRESULT SettingsHandler::Initialize() {
  mswr::ComPtr<winui::ApplicationSettings::ISettingsPaneStatics>
      settings_pane_statics;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_ApplicationSettings_SettingsPane,
      settings_pane_statics.GetAddressOf());
  CheckHR(hr, "Failed to activate ISettingsPaneStatics");

  mswr::ComPtr<winui::ApplicationSettings::ISettingsPane> settings_pane;
  hr = settings_pane_statics->GetForCurrentView(&settings_pane);
  CheckHR(hr, "Failed to get ISettingsPane");

  hr = settings_pane->add_CommandsRequested(
      mswr::Callback<CommandsRequestedHandler>(
          this,
          &SettingsHandler::OnSettingsCommandsRequested).Get(),
      &settings_token_);
  CheckHR(hr, "Failed to add CommandsRequested");

  return hr;
}

HRESULT SettingsHandler::OnSettingsCommandsRequested(
    winui::ApplicationSettings::ISettingsPane* settings_pane,
    winui::ApplicationSettings::ISettingsPaneCommandsRequestedEventArgs* args) {
  mswr::ComPtr<winui::ApplicationSettings::ISettingsCommandFactory>
      settings_command_factory;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_ApplicationSettings_SettingsCommand,
      settings_command_factory.GetAddressOf());
  CheckHR(hr, "Failed to activate ISettingsCommandFactory");

  mswr::ComPtr<winui::ApplicationSettings::ISettingsPaneCommandsRequest>
      settings_command_request;
  hr = args->get_Request(&settings_command_request);
  CheckHR(hr, "Failed to get_Request");

  mswr::ComPtr<SettingsHandler::ISettingsCommandVector> application_commands;
  hr = settings_command_request->get_ApplicationCommands(&application_commands);
  CheckHR(hr, "Failed to get_ApplicationCommands");

  // TODO(mad): Internationalize the hard coded user visible strings.
  hr = AppendNewSettingsCommand(
      kSettingsId, L"Settings", settings_command_factory.Get(),
      application_commands.Get());
  CheckHR(hr, "Failed to append new settings command");

  hr = AppendNewSettingsCommand(
      kHelpId, L"Help", settings_command_factory.Get(),
      application_commands.Get());
  CheckHR(hr, "Failed to append new help command");

  hr = AppendNewSettingsCommand(
      kAboutId, L"About", settings_command_factory.Get(),
      application_commands.Get());
  CheckHR(hr, "Failed to append new about command");

  return hr;
}

HRESULT SettingsHandler::AppendNewSettingsCommand(
    const wchar_t* id,
    const wchar_t* name,
    winui::ApplicationSettings::ISettingsCommandFactory*
        settings_command_factory,
    SettingsHandler::ISettingsCommandVector* settings_command_vector) {
  mswr::ComPtr<winfoundtn::IPropertyValue> settings_id;
  HRESULT hr = GetSettingsId(id, &settings_id);
  CheckHR(hr, "Can't get settings id");

  mswrw::HString settings_name;
  settings_name.Attach(MakeHString(name));
  mswr::ComPtr<winui::Popups::IUICommand> command;
  hr = settings_command_factory->CreateSettingsCommand(
      settings_id.Get(),
      settings_name.Get(),
      mswr::Callback<winui::Popups::IUICommandInvokedHandler>(
          &SettingsHandler::OnSettings).Get(),
      command.GetAddressOf());
  CheckHR(hr, "Can't create settings command");

  hr = settings_command_vector->Append(command.Get());
  CheckHR(hr, "Failed to append settings command");

  return hr;
}

HRESULT SettingsHandler::OnSettings(winui::Popups::IUICommand* command) {
  mswr::ComPtr<winfoundtn::IPropertyValue> settings_id;
  HRESULT hr = GetSettingsId(kSettingsId, &settings_id);
  CheckHR(hr, "Failed to get settings id");

  mswr::ComPtr<winfoundtn::IPropertyValue> help_id;
  hr = GetSettingsId(kHelpId, &help_id);
  CheckHR(hr, "Failed to get settings id");

  mswr::ComPtr<winfoundtn::IPropertyValue> about_id;
  hr = GetSettingsId(kAboutId, &about_id);
  CheckHR(hr, "Failed to get settings id");

  mswr::ComPtr<winfoundtn::IPropertyValue> command_id;
  hr = command->get_Id(&command_id);
  CheckHR(hr, "Failed to get command id");

  INT32 result = -1;
  hr = winrt_utils::CompareProperties(
      command_id.Get(), settings_id.Get(), &result);
  CheckHR(hr, "Failed to compare ids");

  HWND chrome_window = globals.host_windows.front().first;

  if (result == 0) {
    ::PostMessageW(chrome_window, WM_SYSCOMMAND, IDC_OPTIONS, 0);
    return S_OK;
  }

  hr = winrt_utils::CompareProperties(command_id.Get(), help_id.Get(), &result);
  CheckHR(hr, "Failed to compare ids");
  if (result == 0) {
    ::PostMessageW(chrome_window, WM_SYSCOMMAND, IDC_HELP_PAGE_VIA_MENU, 0);
    return S_OK;
  }

  hr = winrt_utils::CompareProperties(
      command_id.Get(), about_id.Get(), &result);
  CheckHR(hr, "Failed to compare ids");
  if (result == 0) {
    ::PostMessageW(chrome_window, WM_SYSCOMMAND, IDC_ABOUT, 0);
    return S_OK;
  }

  return S_OK;
}

HRESULT SettingsHandler::GetSettingsId(
    const wchar_t* value, winfoundtn::IPropertyValue** settings_id) {
  mswrw::HString property_value_string;
  property_value_string.Attach(MakeHString(value));
  return winrt_utils::CreateStringProperty(property_value_string.Get(),
                                           settings_id);
}
