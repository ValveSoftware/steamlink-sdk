// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_POWER_MONITOR_POWER_MONITOR_EXPORT_H_
#define DEVICE_POWER_MONITOR_POWER_MONITOR_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(DEVICE_POWER_MONITOR_IMPLEMENTATION)
#define DEVICE_POWER_MONITOR_EXPORT __declspec(dllexport)
#else
#define DEVICE_POWER_MONITOR_EXPORT __declspec(dllimport)
#endif  // defined(DEVICE_POWER_MONITOR_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(DEVICE_POWER_MONITOR_IMPLEMENTATION)
#define DEVICE_POWER_MONITOR_EXPORT __attribute__((visibility("default")))
#else
#define DEVICE_POWER_MONITOR_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define DEVICE_POWER_MONITOR_EXPORT
#endif

#endif  // DEVICE_POWER_MONITOR_POWER_MONITOR_EXPORT_H_
