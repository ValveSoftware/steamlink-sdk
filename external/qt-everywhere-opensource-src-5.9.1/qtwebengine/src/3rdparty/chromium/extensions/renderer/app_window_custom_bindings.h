// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_APP_WINDOW_CUSTOM_BINDINGS_H_
#define EXTENSIONS_RENDERER_APP_WINDOW_CUSTOM_BINDINGS_H_

#include "base/macros.h"
#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {
class ScriptContextSet;

// Implements custom bindings for the app.window API.
class AppWindowCustomBindings : public ObjectBackedNativeHandler {
 public:
  AppWindowCustomBindings(ScriptContext* context);

 private:
  void GetFrame(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Return string containing the HTML <template> for the <window-controls>
  // custom element.
  void GetWindowControlsHtmlTemplate(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  DISALLOW_COPY_AND_ASSIGN(AppWindowCustomBindings);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_APP_WINDOW_CUSTOM_BINDINGS_H_
