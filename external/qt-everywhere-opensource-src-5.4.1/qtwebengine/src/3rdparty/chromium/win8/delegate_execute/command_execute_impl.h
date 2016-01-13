// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <ShObjIdl.h>
#include <WinInet.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/process_handle.h"
#include "win8/delegate_execute/resource.h"       // main symbols

using namespace ATL;

EXTERN_C const GUID CLSID_CommandExecuteImpl;

// CommandExecuteImpl
// This class implements the IExecuteCommand and related interfaces for
// handling ShellExecute launches of the Chrome browser, i.e. whether to
// launch Chrome in metro mode or desktop mode.
// The CLSID here is a dummy CLSID not used for anything, since we register
// the class with a dynamic CLSID.  However, a static CLSID is necessary
// so that we can force at least one entry into ATL's object map (it will
// treat a 0-element object map as an initialization failure case).
class ATL_NO_VTABLE DECLSPEC_UUID("071BB5F2-85A4-424F-BFE7-5F1609BE4C2C")
    CommandExecuteImpl
    : public CComObjectRootEx<CComSingleThreadModel>,
      public CComCoClass<CommandExecuteImpl, &CLSID_CommandExecuteImpl>,
      public IExecuteCommand,
      public IObjectWithSiteImpl<CommandExecuteImpl>,
      public IInitializeCommand,
      public IObjectWithSelection,
      public IExecuteCommandApplicationHostEnvironment,
      public IForegroundTransfer {
 public:
  CommandExecuteImpl();

  DECLARE_REGISTRY_RESOURCEID(IDR_COMMANDEXECUTEIMPL)

  BEGIN_COM_MAP(CommandExecuteImpl)
    COM_INTERFACE_ENTRY(IExecuteCommand)
    COM_INTERFACE_ENTRY(IObjectWithSite)
    COM_INTERFACE_ENTRY(IInitializeCommand)
    COM_INTERFACE_ENTRY(IObjectWithSelection)
    COM_INTERFACE_ENTRY(IExecuteCommandApplicationHostEnvironment)
    COM_INTERFACE_ENTRY(IForegroundTransfer)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT FinalConstruct() {
    return S_OK;
  }

  void FinalRelease() {
  }

 public:
  // IExecuteCommand
  STDMETHOD(SetKeyState)(DWORD key_state);
  STDMETHOD(SetParameters)(LPCWSTR params);
  STDMETHOD(SetPosition)(POINT pt);
  STDMETHOD(SetShowWindow)(int show);
  STDMETHOD(SetNoShowUI)(BOOL no_show_ui);
  STDMETHOD(SetDirectory)(LPCWSTR directory);
  STDMETHOD(Execute)(void);

  // IInitializeCommand
  STDMETHOD(Initialize)(LPCWSTR name, IPropertyBag* bag);

  // IObjectWithSelection
  STDMETHOD(SetSelection)(IShellItemArray* item_array);
  STDMETHOD(GetSelection)(REFIID riid, void** selection);

  // IExecuteCommandApplicationHostEnvironment
  STDMETHOD(GetValue)(enum AHE_TYPE* pahe);

  // IForegroundTransfer
  STDMETHOD(AllowForegroundTransfer)(void* reserved);

 private:
  static bool FindChromeExe(base::FilePath* chrome_exe);

  static bool path_provider_initialized_;

  void SetHighDPIRegistryKey(bool enable);

  bool GetLaunchScheme(base::string16* display_name, INTERNET_SCHEME* scheme);
  HRESULT LaunchDesktopChrome();
  // Returns the launch mode, i.e. desktop launch/metro launch, etc.
  EC_HOST_UI_MODE GetLaunchMode();

  CComPtr<IShellItemArray> item_array_;
  base::CommandLine parameters_;
  base::FilePath chrome_exe_;
  STARTUPINFO start_info_;
  base::string16 verb_;
  base::string16 display_name_;
  INTERNET_SCHEME launch_scheme_;

  base::IntegrityLevel integrity_level_;
};

OBJECT_ENTRY_AUTO(__uuidof(CommandExecuteImpl), CommandExecuteImpl)
