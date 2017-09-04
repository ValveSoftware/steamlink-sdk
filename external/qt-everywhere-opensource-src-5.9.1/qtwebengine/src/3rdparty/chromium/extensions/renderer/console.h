// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_CONSOLE_H_
#define EXTENSIONS_RENDERER_CONSOLE_H_

#include <string>

#include "content/public/common/console_message_level.h"
#include "v8/include/v8.h"

namespace content {
class RenderFrame;
}

namespace extensions {

// Utility for logging messages to RenderFrames.
namespace console {

// Adds |message| to the console of |render_frame|. If |render_frame| is null,
// LOG()s the message instead.
void Debug(content::RenderFrame* render_frame, const std::string& message);
void Log(content::RenderFrame* render_frame, const std::string& message);
void Warn(content::RenderFrame* render_frame, const std::string& message);
void Error(content::RenderFrame* render_frame, const std::string& message);

// Logs an Error then crashes the current process.
void Fatal(content::RenderFrame* render_frame, const std::string& message);

void AddMessage(content::RenderFrame* render_frame,
                content::ConsoleMessageLevel level,
                const std::string& message);

// Returns a new v8::Object with each standard log method (Debug/Log/Warn/Error)
// bound to respective debug/log/warn/error methods. This is a direct drop-in
// replacement for the standard devtools console.* methods usually accessible
// from JS.
v8::Local<v8::Object> AsV8Object(v8::Isolate* isolate);

}  // namespace console

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_CONSOLE_H_
