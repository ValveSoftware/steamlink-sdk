// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/plugin_constants_win.h"

namespace content {

const base::char16 kNativeWindowClassName[] = L"NativeWindowClass";
const base::char16 kWrapperNativeWindowClassName[] =
    L"WrapperNativeWindowClass";
const base::char16 kPluginNameAtomProperty[] = L"PluginNameAtom";
const base::char16 kPluginVersionAtomProperty[] = L"PluginVersionAtom";
const base::char16 kDummyActivationWindowName[] = L"DummyWindowForActivation";
const base::char16 kPaintMessageName[] = L"Chrome_CustomPaintil";
const base::char16 kRegistryMozillaPlugins[] = L"SOFTWARE\\MozillaPlugins";
const base::char16 kMozillaActiveXPlugin[] = L"npmozax.dll";
const base::char16 kNewWMPPlugin[] = L"np-mswmp.dll";
const base::char16 kOldWMPPlugin[] = L"npdsplay.dll";
const base::char16 kYahooApplicationStatePlugin[] = L"npystate.dll";
const base::char16 kWanWangProtocolHandlerPlugin[] = L"npww.dll";
const base::char16 kFlashPlugin[] = L"npswf32.dll";
const base::char16 kAcrobatReaderPlugin[] = L"nppdf32.dll";
const base::char16 kRealPlayerPlugin[] = L"nppl3260.dll";
const base::char16 kSilverlightPlugin[] = L"npctrl.dll";
const base::char16 kJavaPlugin1[] = L"npjp2.dll";
const base::char16 kJavaPlugin2[] = L"npdeploytk.dll";
const char kGPUPluginMimeType[] = "application/vnd.google.chrome.gpu-plugin";
const base::char16 kPluginDummyParentProperty[] = L"NPAPIPluginDummyParent";

}  // namespace content
