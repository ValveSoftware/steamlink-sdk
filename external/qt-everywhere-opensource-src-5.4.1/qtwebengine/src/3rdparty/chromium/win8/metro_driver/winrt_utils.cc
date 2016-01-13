// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "winrt_utils.h"

#include <shlobj.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"

void CheckHR(HRESULT hr, const char* message) {
  if (FAILED(hr)) {
    if (message)
      PLOG(DFATAL) << message << ", hr = " << std::hex << hr;
    else
      PLOG(DFATAL) << "COM ERROR" << ", hr = " << std::hex << hr;
  }
}

HSTRING MakeHString(const base::string16& str) {
  HSTRING hstr;
  if (FAILED(::WindowsCreateString(str.c_str(), static_cast<UINT32>(str.size()),
                                   &hstr))) {
    PLOG(DFATAL) << "Hstring creation failed";
  }
  return hstr;
}

base::string16 MakeStdWString(HSTRING hstring) {
  const wchar_t* str;
  UINT32 size = 0;
  str = ::WindowsGetStringRawBuffer(hstring, &size);
  if (!size)
    return base::string16();
  return base::string16(str, size);
}

namespace {

#define IMPLEMENT_CREATE_PROPERTY(Name, Type) \
HRESULT Create ## Name ## Property(Type value, \
                                   winfoundtn::IPropertyValue** prop) { \
  mswr::ComPtr<winfoundtn::IPropertyValueStatics> property_value_statics; \
  HRESULT hr = winrt_utils::CreateActivationFactory( \
      RuntimeClass_Windows_Foundation_PropertyValue, \
      property_value_statics.GetAddressOf()); \
  CheckHR(hr, "Can't create IPropertyValueStatics"); \
  hr = property_value_statics->Create ## Name ## ( \
      value, \
      reinterpret_cast<IInspectable**>(prop)); \
  CheckHR(hr, "Failed to create Property"); \
  return hr; \
}

#define COMPARE_ATOMIC_PROPERTY_VALUES(Name, Type) \
  Type lhs_value; \
  hr = lhs->Get ## Name ##(&lhs_value); \
  CheckHR(hr, "Can't get value for lhs"); \
  Type rhs_value; \
  hr = rhs->Get ## Name ##(&rhs_value); \
  CheckHR(hr, "Can't get value for rhs"); \
  if (lhs_value < rhs_value) \
    *result = -1; \
  else if (lhs_value > rhs_value) \
    *result = 1; \
  else \
    *result = 0; \
  hr = S_OK

}  // namespace

namespace winrt_utils {

IMPLEMENT_CREATE_PROPERTY(String, HSTRING);
IMPLEMENT_CREATE_PROPERTY(Int16, INT16);
IMPLEMENT_CREATE_PROPERTY(Int32, INT32);
IMPLEMENT_CREATE_PROPERTY(Int64, INT64);
IMPLEMENT_CREATE_PROPERTY(UInt8, UINT8);
IMPLEMENT_CREATE_PROPERTY(UInt16, UINT16);
IMPLEMENT_CREATE_PROPERTY(UInt32, UINT32);
IMPLEMENT_CREATE_PROPERTY(UInt64, UINT64);

HRESULT CompareProperties(winfoundtn::IPropertyValue* lhs,
                          winfoundtn::IPropertyValue* rhs,
                          INT32* result) {
  if (result == nullptr) {
    PLOG(DFATAL) << "Invalid argument to CompareProperties.";
    return E_INVALIDARG;
  }

  if (lhs == rhs) {
    *result = 0;
    return S_OK;
  }

  winfoundtn::PropertyType lhs_property_type;
  HRESULT hr = lhs->get_Type(&lhs_property_type);
  if (FAILED(hr)) {
    PLOG(DFATAL) << "Can't get property type for lhs, hr=" << std::hex << hr;
  }

  winfoundtn::PropertyType rhs_property_type;
  hr = rhs->get_Type(&rhs_property_type);
  CheckHR(hr, "Can't get property type for rhs");

  if (lhs_property_type != rhs_property_type)
    return E_INVALIDARG;

  switch (lhs_property_type) {
    case winfoundtn::PropertyType::PropertyType_String: {
      mswrw::HString lhs_string;
      hr = lhs->GetString(lhs_string.GetAddressOf());
      CheckHR(hr, "Can't get string for lhs");

      mswrw::HString rhs_string;
      hr = rhs->GetString(rhs_string.GetAddressOf());
      CheckHR(hr, "Can't get string for rhs");

      hr = WindowsCompareStringOrdinal(
          lhs_string.Get(), rhs_string.Get(), result);
      break;
    }
    case winfoundtn::PropertyType::PropertyType_Char16: {
      COMPARE_ATOMIC_PROPERTY_VALUES(Char16, wchar_t);
      break;
    }
    case winfoundtn::PropertyType::PropertyType_Double: {
      COMPARE_ATOMIC_PROPERTY_VALUES(Double, double);
      break;
    }
    case winfoundtn::PropertyType::PropertyType_Int16: {
      COMPARE_ATOMIC_PROPERTY_VALUES(Int16, INT16);
      break;
    }
    case winfoundtn::PropertyType::PropertyType_Int32: {
      COMPARE_ATOMIC_PROPERTY_VALUES(Int32, INT32);
      break;
    }
    case winfoundtn::PropertyType::PropertyType_Int64: {
      COMPARE_ATOMIC_PROPERTY_VALUES(Int64, INT64);
      break;
    }
    case winfoundtn::PropertyType::PropertyType_UInt8: {
      COMPARE_ATOMIC_PROPERTY_VALUES(UInt8, UINT8);
      break;
    }
    case winfoundtn::PropertyType::PropertyType_UInt16: {
      COMPARE_ATOMIC_PROPERTY_VALUES(UInt16, UINT16);
      break;
    }
    case winfoundtn::PropertyType::PropertyType_UInt32: {
      COMPARE_ATOMIC_PROPERTY_VALUES(UInt32, UINT32);
      break;
    }
    case winfoundtn::PropertyType::PropertyType_UInt64: {
      COMPARE_ATOMIC_PROPERTY_VALUES(UInt64, UINT64);
      break;
    }
    default: {
      hr = E_NOTIMPL;
    }
  }
  return hr;
}

bool GetArgumentsFromShortcut(const base::FilePath& shortcut,
                              base::string16* arguments) {
  HRESULT result;
  base::win::ScopedComPtr<IShellLink> i_shell_link;
  bool is_resolved = false;


  base::win::ScopedCOMInitializer sta_com_initializer;

  // Get pointer to the IShellLink interface
  result = i_shell_link.CreateInstance(CLSID_ShellLink, NULL,
                                       CLSCTX_INPROC_SERVER);
  if (SUCCEEDED(result)) {
    base::win::ScopedComPtr<IPersistFile> persist;
    // Query IShellLink for the IPersistFile interface
    result = persist.QueryFrom(i_shell_link);
    if (SUCCEEDED(result)) {
      WCHAR temp_arguments[MAX_PATH];
      // Load the shell link
      result = persist->Load(shortcut.value().c_str(), STGM_READ);
      if (SUCCEEDED(result)) {
        result = i_shell_link->GetArguments(temp_arguments, MAX_PATH);
        *arguments = temp_arguments;
        is_resolved = true;
      }
    }
  }

  return is_resolved;
}

base::string16 ReadArgumentsFromPinnedTaskbarShortcut() {
  wchar_t path_buffer[MAX_PATH] = {};

  if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL,
                                SHGFP_TYPE_CURRENT, path_buffer))) {
    base::FilePath shortcut(path_buffer);
    shortcut = shortcut.Append(
        L"Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar");

    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    base::string16 link_name = dist->GetShortcutName(
        BrowserDistribution::SHORTCUT_CHROME) + installer::kLnkExt;
    shortcut = shortcut.Append(link_name);

    base::string16 arguments;
    if (GetArgumentsFromShortcut(shortcut, &arguments)) {
      return arguments;
    }
  }

  return L"";
}

}  // namespace winrt_utils
