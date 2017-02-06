// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_SCHEDULER_EXPORT_H_
#define COMPONENTS_SCHEDULER_SCHEDULER_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SCHEDULER_IMPLEMENTATION)
#define SCHEDULER_EXPORT __declspec(dllexport)
#else
#define SCHEDULER_EXPORT __declspec(dllimport)
#endif  // defined(SCHEDULER_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(SCHEDULER_IMPLEMENTATION)
#define SCHEDULER_EXPORT __attribute__((visibility("default")))
#else
#define SCHEDULER_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define SCHEDULER_EXPORT
#endif

#endif  // COMPONENTS_SCHEDULER_SCHEDULER_EXPORT_H_
