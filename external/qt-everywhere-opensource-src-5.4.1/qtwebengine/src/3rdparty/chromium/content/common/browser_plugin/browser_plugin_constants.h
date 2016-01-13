// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef  CONTENT_COMMON_BROWSER_PLUGIN_BROWSER_PLUGIN_CONSTANTS_H_
#define  CONTENT_COMMON_BROWSER_PLUGIN_BROWSER_PLUGIN_CONSTANTS_H_

namespace content {

namespace browser_plugin {

// Internal method bindings.
extern const char kMethodInternalAttach[];

// Attributes.
extern const char kAttributeAllowTransparency[];
extern const char kAttributeAutoSize[];
extern const char kAttributeContentWindow[];
extern const char kAttributeMaxHeight[];
extern const char kAttributeMaxWidth[];
extern const char kAttributeMinHeight[];
extern const char kAttributeMinWidth[];
extern const char kAttributeName[];
extern const char kAttributePartition[];
extern const char kAttributeSrc[];

// Parameters/properties on events.
extern const char kWindowID[];

// Error messages.
extern const char kErrorCannotRemovePartition[];

// Other.
extern const int kInstanceIDNone;
extern const int kInvalidPermissionRequestID;

}  // namespace browser_plugin

}  // namespace content

#endif  // CONTENT_COMMON_BROWSER_PLUGIN_BROWSER_PLUGIN_CONSTANTS_H_
