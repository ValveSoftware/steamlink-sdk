// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_DRIVER_CHROME_URL_LAUNCH_HANDLER_H_
#define CHROME_BROWSER_UI_METRO_DRIVER_CHROME_URL_LAUNCH_HANDLER_H_

#include <string>
#include <windows.applicationmodel.core.h>
#include <Windows.applicationModel.search.h>
#include <windows.ui.core.h>

#include "winrt_utils.h"

// This class handles the various flavors of URL launches in metro, i.e.
// via the search charm, via a url being navigated from a metro app, etc.
class ChromeUrlLaunchHandler {
 public:
  ChromeUrlLaunchHandler();
  ~ChromeUrlLaunchHandler();

  HRESULT Initialize();

  // If metro chrome was launched due to a URL navigation/search request then
  // the navigation should be done when the frame window is initialized. This
  // function is called to complete the pending navigation when we receive a
  // notification from chrome that the frame window is initialized.
  void PerformPendingNavigation();

  void Activate(winapp::Activation::IActivatedEventArgs* args);

 private:
  // Invoked when we receive search notifications in metro chrome.
  template<class T> void HandleSearchRequest(T* args);

  HRESULT OnQuerySubmitted(
      winapp::Search::ISearchPane* search_pane,
      winapp::Search::ISearchPaneQuerySubmittedEventArgs* args);

  base::string16 GetUrlFromLaunchArgs(const base::string16& launch_args);

  // Invoked when a url is navigated from a metro app or in the metro
  // shelf.
  void HandleProtocolLaunch(
      winapp::Activation::IProtocolActivatedEventArgs* args);

  // Invoked when the app is launched normally
  void HandleLaunch(winapp::Activation::ILaunchActivatedEventArgs* args);

  // Helper function to initiate a navigation or search request in chrome.
  void InitiateNavigationOrSearchRequest(const wchar_t* url,
                                         const wchar_t* search_string);

  Microsoft::WRL::ComPtr<winapp::Search::ISearchPane> search_pane_;
  EventRegistrationToken query_submitted_token_;
};

#endif  // CHROME_BROWSER_UI_METRO_DRIVER_CHROME_URL_LAUNCH_HANDLER_H_
