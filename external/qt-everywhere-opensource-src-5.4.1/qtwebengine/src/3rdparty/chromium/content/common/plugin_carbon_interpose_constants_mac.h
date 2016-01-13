// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PLUGIN_CARBON_INTERPOSE_CONSTANTS_MAC_H_
#define CONTENT_COMMON_PLUGIN_CARBON_INTERPOSE_CONSTANTS_MAC_H_

#if !defined(__LP64__)

// Strings used in setting up Carbon interposing for the plugin process.
namespace content {

extern const char kDYLDInsertLibrariesKey[];

}  // namespace content

#endif  // !__LP64__

#endif  // CONTENT_COMMON_PLUGIN_CARBON_INTERPOSE_CONSTANTS_MAC_H_
