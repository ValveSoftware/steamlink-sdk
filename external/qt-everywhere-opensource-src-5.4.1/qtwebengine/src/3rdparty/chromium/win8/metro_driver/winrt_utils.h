// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_WINRT_UTILS_H_
#define WIN8_METRO_DRIVER_WINRT_UTILS_H_

#include <string>

#include <roapi.h>
#include <windows.applicationmodel.core.h>

#include "base/strings/string16.h"

void CheckHR(HRESULT hr, const char* str = nullptr);

HSTRING MakeHString(const base::string16& str);

base::string16 MakeStdWString(HSTRING hstring);

namespace winrt_utils {

template<unsigned int size, typename T>
HRESULT CreateActivationFactory(wchar_t const (&class_name)[size], T** object) {
  mswrw::HStringReference ref_class_name(class_name);
  return winfoundtn::GetActivationFactory(ref_class_name.Get(), object);
}

#define DECLARE_CREATE_PROPERTY(Name, Type) \
HRESULT Create ## Name ## Property( \
    Type value, \
    winfoundtn::IPropertyValue** prop);

DECLARE_CREATE_PROPERTY(String, HSTRING);
DECLARE_CREATE_PROPERTY(Int16, INT16);
DECLARE_CREATE_PROPERTY(Int32, INT32);
DECLARE_CREATE_PROPERTY(Int64, INT64);
DECLARE_CREATE_PROPERTY(UInt8, UINT8);
DECLARE_CREATE_PROPERTY(UInt16, UINT16);
DECLARE_CREATE_PROPERTY(UInt32, UINT32);
DECLARE_CREATE_PROPERTY(UInt64, UINT64);

// Compares |lhs| with |rhs| and return the |result| as
// WindowsCompareStringOrdinal would do, i.e.,
// -1 if |lhs| is less than |rhs|, 0 if they are equal, and
// +1 if |lhs| is greater than |rhs|.
HRESULT CompareProperties(
    winfoundtn::IPropertyValue* lhs, winfoundtn::IPropertyValue* rhs,
    INT32* result);

// Looks for a pinned taskbar shortcut in the current user's profile. If it
// finds one, will return any arguments that have been appended to the
// shortcut's command line. This is intended for scenarios where those shortcut
// parameters are ordinarily ignored (i.e. metro apps on win8). Returns an
// empty string on failure.
base::string16 ReadArgumentsFromPinnedTaskbarShortcut();

}  // namespace winrt_utils

#endif  // WIN8_METRO_DRIVER_WINRT_UTILS_H_
