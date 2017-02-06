// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIDEVINE_CDM_WIDEVINE_CDM_COMMON_H_
#define WIDEVINE_CDM_WIDEVINE_CDM_COMMON_H_

// This file defines constants common to all Widevine CDM versions.

// Widevine CDM version contains 4 components, e.g. 1.4.0.195.
const int kWidevineCdmVersionNumComponents = 4;

// "alpha" is a temporary name until a convention is defined.
const char kWidevineKeySystem[] = "com.widevine.alpha";

// This type is used to register the Widevine CDM.
const char kWidevineCdmType[] = "Widevine";

// Widevine CDM files are in a directory with this name.
const char kWidevineCdmBaseDirectory[] = "WidevineCdm";

// This name is used by UMA. Do not change it!
const char kWidevineKeySystemNameForUMA[] = "Widevine";

const char kWidevineCdmDisplayName[] = "Widevine Content Decryption Module";

// Will be parsed as HTML.
const char kWidevineCdmDescription[] =
    "Enables Widevine licenses for playback of HTML audio/video content.";

#if defined(ENABLE_PEPPER_CDMS)
const char kWidevineCdmPluginMimeType[] = "application/x-ppapi-widevine-cdm";
const char kWidevineCdmPluginMimeTypeDescription[] =
    "Widevine Content Decryption Module";

// Name of the CDM library.
const char kWidevineCdmLibraryName[] = "widevinecdm";

// File name of the adapter on different platforms.
const char kWidevineCdmAdapterFileName[] =
#if defined(OS_MACOSX)
    "widevinecdmadapter.plugin";
#elif defined(OS_WIN)
    "widevinecdmadapter.dll";
#else  // OS_LINUX, etc.
    "libwidevinecdmadapter.so";
#endif

// The following strings are used to communicate supported codecs (from the
// component manifest) via WebPluginInfo::WebPluginMimeType's additional params.
const char kCdmSupportedCodecsParamName[] = "codecs";
const char kCdmSupportedCodecsValueDelimiter = ',';
const char kCdmSupportedCodecVp8[] = "vp8";
const char kCdmSupportedCodecVp9[] = "vp9.0";
#if defined(USE_PROPRIETARY_CODECS)
const char kCdmSupportedCodecAvc1[] = "avc1";
#endif  // defined(USE_PROPRIETARY_CODECS)

#if (defined(OS_MACOSX) || defined(OS_WIN)) && !defined(TOOLKIT_QT)
// CDM is installed by the component installer instead of the Chrome installer.
#define WIDEVINE_CDM_IS_COMPONENT
#endif  // defined(OS_MACOSX) || defined(OS_WIN)
#endif  // defined(ENABLE_PEPPER_CDMS)

#endif  // WIDEVINE_CDM_WIDEVINE_CDM_COMMON_H_
