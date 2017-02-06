// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extensions_render_frame_observer.h"

#include <stddef.h>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/stack_frame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace extensions {

namespace {

// The delimiter for a stack trace provided by WebKit.
const char kStackFrameDelimiter[] = "\n    at ";

// Get a stack trace from a WebKit console message.
// There are three possible scenarios:
// 1. WebKit gives us a stack trace in |stack_trace|.
// 2. The stack trace is embedded in the error |message| by an internal
//    script. This will be more useful than |stack_trace|, since |stack_trace|
//    will include the internal bindings trace, instead of a developer's code.
// 3. No stack trace is included. In this case, we should mock one up from
//    the given line number and source.
// |message| will be populated with the error message only (i.e., will not
// include any stack trace).
StackTrace GetStackTraceFromMessage(base::string16* message,
                                    const base::string16& source,
                                    const base::string16& stack_trace,
                                    int32_t line_number) {
  StackTrace result;
  std::vector<base::string16> pieces;
  size_t index = 0;

  if (message->find(base::UTF8ToUTF16(kStackFrameDelimiter)) !=
          base::string16::npos) {
    base::SplitStringUsingSubstr(*message,
                                 base::UTF8ToUTF16(kStackFrameDelimiter),
                                 &pieces);
    *message = pieces[0];
    index = 1;
  } else if (!stack_trace.empty()) {
    base::SplitStringUsingSubstr(stack_trace,
                                 base::UTF8ToUTF16(kStackFrameDelimiter),
                                 &pieces);
  }

  // If we got a stack trace, parse each frame from the text.
  if (index < pieces.size()) {
    for (; index < pieces.size(); ++index) {
      std::unique_ptr<StackFrame> frame =
          StackFrame::CreateFromText(pieces[index]);
      if (frame.get())
        result.push_back(*frame);
    }
  }

  if (result.empty()) {  // If we don't have a stack trace, mock one up.
    result.push_back(
        StackFrame(line_number,
                   1u,  // column number
                   source,
                   base::string16() /* no function name */ ));
  }

  return result;
}

}  // namespace

ExtensionsRenderFrameObserver::ExtensionsRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
}

ExtensionsRenderFrameObserver::~ExtensionsRenderFrameObserver() {
}

void ExtensionsRenderFrameObserver::DetailedConsoleMessageAdded(
    const base::string16& message,
    const base::string16& source,
    const base::string16& stack_trace_string,
    uint32_t line_number,
    int32_t severity_level) {
  base::string16 trimmed_message = message;
  StackTrace stack_trace = GetStackTraceFromMessage(
      &trimmed_message,
      source,
      stack_trace_string,
      line_number);
  Send(new ExtensionHostMsg_DetailedConsoleMessageAdded(
      routing_id(), trimmed_message, source, stack_trace, severity_level));
}

void ExtensionsRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace extensions
