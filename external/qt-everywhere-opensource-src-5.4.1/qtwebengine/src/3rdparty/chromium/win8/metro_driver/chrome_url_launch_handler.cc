// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "chrome_url_launch_handler.h"
#include "chrome_app_view.h"

#include <assert.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>

#include "base/command_line.h"

#include "winrt_utils.h"

typedef winfoundtn::ITypedEventHandler<
    winapp::Search::SearchPane*,
    winapp::Search::SearchPaneQuerySubmittedEventArgs*> QuerySubmittedHandler;

ChromeUrlLaunchHandler::ChromeUrlLaunchHandler() {
  globals.is_initial_activation = true;
  globals.initial_activation_kind = winapp::Activation::ActivationKind_Launch;
  DVLOG(1) << __FUNCTION__;
}

// TODO(ananta)
// Remove this once we consolidate metro driver with chrome.
const wchar_t kMetroNavigationAndSearchMessage[] =
    L"CHROME_METRO_NAV_SEARCH_REQUEST";

ChromeUrlLaunchHandler::~ChromeUrlLaunchHandler() {
  DVLOG(1) << __FUNCTION__;
  search_pane_->remove_QuerySubmitted(query_submitted_token_);
}

HRESULT ChromeUrlLaunchHandler::Initialize() {
  mswr::ComPtr<winapp::Search::ISearchPaneStatics> search_pane_statics;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_ApplicationModel_Search_SearchPane,
      search_pane_statics.GetAddressOf());
  CheckHR(hr, "Failed to activate ISearchPaneStatics");

  hr = search_pane_statics->GetForCurrentView(&search_pane_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get search pane for current view";
    return hr;
  }

  hr = search_pane_->add_QuerySubmitted(mswr::Callback<QuerySubmittedHandler>(
      this,
      &ChromeUrlLaunchHandler::OnQuerySubmitted).Get(),
      &query_submitted_token_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to register for Query Submitted event";
    return hr;
  }
  return hr;
}

HRESULT ChromeUrlLaunchHandler::OnQuerySubmitted(
    winapp::Search::ISearchPane* search_pane,
    winapp::Search::ISearchPaneQuerySubmittedEventArgs* args) {
  DVLOG(1) << "OnQuerySubmitted";
  HandleSearchRequest(args);
  return S_OK;
}

template<class T>
void ChromeUrlLaunchHandler::HandleSearchRequest(T* args) {
  DVLOG(1) << __FUNCTION__;
  mswrw::HString search_string;
  args->get_QueryText(search_string.GetAddressOf());
  base::string16 search_text(MakeStdWString(search_string.Get()));
  globals.search_string = search_text;
  DVLOG(1) << search_text.c_str();
  // If this is the initial activation then we wait for Chrome to initiate the
  // navigation. In all other cases navigate right away.
  if (!globals.is_initial_activation)
    InitiateNavigationOrSearchRequest(NULL, globals.search_string.c_str());
}

void ChromeUrlLaunchHandler::HandleProtocolLaunch(
    winapp::Activation::IProtocolActivatedEventArgs* args) {
  DVLOG(1) << __FUNCTION__;
  mswr::ComPtr<winfoundtn::IUriRuntimeClass> uri;
  args->get_Uri(&uri);
  mswrw::HString url;
  uri->get_AbsoluteUri(url.GetAddressOf());
  base::string16 actual_url(MakeStdWString(url.Get()));
  globals.navigation_url = actual_url;

  // If this is the initial activation then we wait for Chrome to initiate the
  // navigation. In all other cases navigate right away.
  if (!globals.is_initial_activation)
    InitiateNavigationOrSearchRequest(globals.navigation_url.c_str(), 0);
}

// |launch_args| is an encoded command line, minus the executable name. To
// find the URL to launch the first argument is used. If any other parameters
// are encoded in |launch_args| they are ignored.
base::string16 ChromeUrlLaunchHandler::GetUrlFromLaunchArgs(
    const base::string16& launch_args) {
  if (launch_args == L"opennewwindow") {
    VLOG(1) << "Returning new tab url";
    return L"chrome://newtab";
  }
  base::string16 dummy_command_line(L"dummy.exe ");
  dummy_command_line.append(launch_args);
  CommandLine command_line = CommandLine::FromString(dummy_command_line);
  CommandLine::StringVector args = command_line.GetArgs();
  if (args.size() > 0)
    return args[0];

  return base::string16();
}

void ChromeUrlLaunchHandler::HandleLaunch(
    winapp::Activation::ILaunchActivatedEventArgs* args) {
  mswrw::HString launch_args;
  args->get_Arguments(launch_args.GetAddressOf());
  base::string16 actual_launch_args(MakeStdWString(launch_args.Get()));
  globals.navigation_url = GetUrlFromLaunchArgs(actual_launch_args);
  DVLOG(1) << __FUNCTION__ << ", launch_args=" << actual_launch_args
           << ", url=" << globals.navigation_url
           << ", is_initial_activation=" << globals.is_initial_activation;

  // If this is the initial launch then we wait for Chrome to initiate the
  // navigation. In all other cases navigate right away.
  if (!globals.is_initial_activation)
    InitiateNavigationOrSearchRequest(globals.navigation_url.c_str(), 0);
}

void ChromeUrlLaunchHandler::Activate(
    winapp::Activation::IActivatedEventArgs* args) {
  winapp::Activation::ActivationKind activation_kind;
  CheckHR(args->get_Kind(&activation_kind));

  DVLOG(1) << __FUNCTION__ << ", activation_kind=" << activation_kind;

  if (globals.is_initial_activation)
    globals.initial_activation_kind = activation_kind;

  if (activation_kind == winapp::Activation::ActivationKind_Launch) {
    mswr::ComPtr<winapp::Activation::ILaunchActivatedEventArgs> launch_args;
    if (args->QueryInterface(winapp::Activation::IID_ILaunchActivatedEventArgs,
                             &launch_args) == S_OK) {
      DVLOG(1) << "Activate: ActivationKind_Launch";
      HandleLaunch(launch_args.Get());
    }
  } else if (activation_kind ==
             winapp::Activation::ActivationKind_Search) {
    mswr::ComPtr<winapp::Activation::ISearchActivatedEventArgs> search_args;
    if (args->QueryInterface(winapp::Activation::IID_ISearchActivatedEventArgs,
                             &search_args) == S_OK) {
      DVLOG(1) << "Activate: ActivationKind_Search";
      HandleSearchRequest(search_args.Get());
    }
  } else if (activation_kind ==
             winapp::Activation::ActivationKind_Protocol) {
    mswr::ComPtr<winapp::Activation::IProtocolActivatedEventArgs>
        protocol_args;
    if (args->QueryInterface(
            winapp::Activation::IID_IProtocolActivatedEventArgs,
            &protocol_args) == S_OK) {
      DVLOG(1) << "Activate: ActivationKind_Protocol";
      HandleProtocolLaunch(protocol_args.Get());
    }
  } else {
    DVLOG(1) << "Activate: Unhandled mode: " << activation_kind;
  }
}

void ChromeUrlLaunchHandler::InitiateNavigationOrSearchRequest(
    const wchar_t* url, const wchar_t* search_string) {
  DVLOG(1) << __FUNCTION__;
  if (!url && !search_string) {
    NOTREACHED();
    return;
  }

  DVLOG(1) << (url ? url : L"NULL url");
  DVLOG(1) << (search_string ? search_string : L"NULL search string");

  if (globals.host_windows.empty()) {
    DVLOG(1) << "No chrome windows registered. Ignoring nav request";
    return;
  }

  // Custom registered message to navigate or search in chrome. WPARAM
  // points to the URL and LPARAM contains the search string. They are
  // mutually exclusive.
  static const UINT navigation_search_message =
      RegisterWindowMessage(kMetroNavigationAndSearchMessage);

  if (url) {
    VLOG(1) << "Posting url:" << url;
    PostMessage(globals.host_windows.front().first, navigation_search_message,
                reinterpret_cast<WPARAM>(url), 0);
  } else {
    VLOG(1) << "Posting search string:" << search_string;
    PostMessage(globals.host_windows.front().first, navigation_search_message,
                0, reinterpret_cast<LPARAM>(search_string));
  }
}
