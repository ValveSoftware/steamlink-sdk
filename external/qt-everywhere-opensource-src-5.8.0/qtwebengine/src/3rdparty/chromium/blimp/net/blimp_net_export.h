// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_NET_EXPORT_H_
#define BLIMP_NET_BLIMP_NET_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(BLIMP_NET_IMPLEMENTATION)
#define BLIMP_NET_EXPORT __declspec(dllexport)
#else
#define BLIMP_NET_EXPORT __declspec(dllimport)
#endif  // defined(BLIMP_NET_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(BLIMP_NET_IMPLEMENTATION)
#define BLIMP_NET_EXPORT __attribute__((visibility("default")))
#else
#define BLIMP_NET_EXPORT
#endif  // defined(BLIMP_NET_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define BLIMP_NET_EXPORT
#endif

#endif  // BLIMP_NET_BLIMP_NET_EXPORT_H_
