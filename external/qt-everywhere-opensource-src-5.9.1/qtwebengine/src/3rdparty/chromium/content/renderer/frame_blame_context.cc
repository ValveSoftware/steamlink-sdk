// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/frame_blame_context.h"

#include "base/trace_event/trace_event_argument.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/top_level_blame_context.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {
namespace {

base::trace_event::BlameContext* GetParentBlameContext(
    RenderFrameImpl* parent_frame) {
  if (parent_frame)
    return parent_frame->frameBlameContext();
  return blink::Platform::current()->topLevelBlameContext();
}

}  // namespace

const char kFrameBlameContextCategory[] = "blink";
const char kFrameBlameContextName[] = "FrameBlameContext";
const char kFrameBlameContextType[] = "RenderFrame";
const char kFrameBlameContextScope[] = "RenderFrame";

FrameBlameContext::FrameBlameContext(RenderFrameImpl* render_frame,
                                     RenderFrameImpl* parent_frame)
    : base::trace_event::BlameContext(kFrameBlameContextCategory,
                                      kFrameBlameContextName,
                                      kFrameBlameContextType,
                                      kFrameBlameContextScope,
                                      render_frame->GetRoutingID(),
                                      GetParentBlameContext(parent_frame)) {}

FrameBlameContext::~FrameBlameContext() {}

}  // namespace content
