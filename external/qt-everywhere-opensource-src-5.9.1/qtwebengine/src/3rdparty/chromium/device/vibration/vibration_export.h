// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VIBRATION_DEVICE_VIBRATION_EXPORT_H_
#define DEVICE_VIBRATION_DEVICE_VIBRATION_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(DEVICE_VIBRATION_IMPLEMENTATION)
#define DEVICE_VIBRATION_EXPORT __declspec(dllexport)
#else
#define DEVICE_VIBRATION_EXPORT __declspec(dllimport)
#endif  // defined(DEVICE_VIBRATION_IMPLEMENTATION)

#else // defined(WIN32)
#if defined(DEVICE_VIBRATION_IMPLEMENTATION)
#define DEVICE_VIBRATION_EXPORT __attribute__((visibility("default")))
#else
#define DEVICE_VIBRATION_EXPORT
#endif
#endif

#else // defined(COMPONENT_BUILD)
#define DEVICE_VIBRATION_EXPORT
#endif

#endif  // DEVICE_VIBRATION_DEVICE_VIBRATION_EXPORT_H_
