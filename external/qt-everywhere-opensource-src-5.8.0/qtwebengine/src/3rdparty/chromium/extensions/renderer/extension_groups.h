// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_GROUPS_H_
#define EXTENSIONS_RENDERER_EXTENSION_GROUPS_H_

namespace extensions {

// A set of extension groups for use with blink::registerExtension and
// WebFrame::ExecuteScriptInNewWorld to control which extensions get loaded
// into which contexts.
// TODO(kalman): Remove this when https://crbug.com/481699 is fixed.
enum ExtensionGroups {
  // Use this to mark extensions to be loaded into content scripts only.
  EXTENSION_GROUP_CONTENT_SCRIPTS = 1,
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_GROUPS_H_
