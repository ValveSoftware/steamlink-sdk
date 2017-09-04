// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_TIME_ZONE_MONITOR_DEVICE_TIME_ZONE_MONITOR_EXPORT_H_
#define DEVICE_TIME_ZONE_MONITOR_DEVICE_TIME_ZONE_MONITOR_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(DEVICE_TIME_ZONE_MONITOR_IMPLEMENTATION)
#define DEVICE_TIME_ZONE_MONITOR_EXPORT __declspec(dllexport)
#else
#define DEVICE_TIME_ZONE_MONITOR_EXPORT __declspec(dllimport)
#endif  // defined(DEVICE_TIME_ZONE_MONITOR_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(DEVICE_TIME_ZONE_MONITOR_IMPLEMENTATION)
#define DEVICE_TIME_ZONE_MONITOR_EXPORT __attribute__((visibility("default")))
#else
#define DEVICE_TIME_ZONE_MONITOR_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define DEVICE_TIME_ZONE_MONITOR_EXPORT
#endif

#endif  // DEVICE_TIME_ZONE_MONITOR_DEVICE_TIME_ZONE_MONITOR_EXPORT_H_
