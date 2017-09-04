// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_BLIMP_HELIUM_EXPORT_H_
#define BLIMP_HELIUM_BLIMP_HELIUM_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(HELIUM_IMPLEMENTATION)
#define BLIMP_HELIUM_EXPORT __declspec(dllexport)
#else
#define BLIMP_HELIUM_EXPORT __declspec(dllimport)
#endif  // defined(HELIUM_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(HELIUM_IMPLEMENTATION)
#define BLIMP_HELIUM_EXPORT __attribute__((visibility("default")))
#else
#define BLIMP_HELIUM_EXPORT
#endif  // defined(HELIUM_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define BLIMP_HELIUM_EXPORT
#endif

#endif  // BLIMP_HELIUM_BLIMP_HELIUM_EXPORT_H_
