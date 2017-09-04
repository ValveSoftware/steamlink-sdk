// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/text_input_client_observer.h"

#include <stddef.h>

#include <memory>

#include "build/build_config.h"
#include "content/common/text_input_client_messages.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/mac/WebSubstringUtil.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

TextInputClientObserver::TextInputClientObserver(RenderWidget* render_widget)
    : render_widget_(render_widget) {}

TextInputClientObserver::~TextInputClientObserver() {
}

bool TextInputClientObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TextInputClientObserver, message)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_StringAtPoint,
                        OnStringAtPoint)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_CharacterIndexForPoint,
                        OnCharacterIndexForPoint)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_FirstRectForCharacterRange,
                        OnFirstRectForCharacterRange)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_StringForRange, OnStringForRange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool TextInputClientObserver::Send(IPC::Message* message) {
  return render_widget_->Send(message);
}

blink::WebFrameWidget* TextInputClientObserver::GetWebFrameWidget() const {
  blink::WebWidget* widget = render_widget_->GetWebWidget();
  if (!widget->isWebFrameWidget()) {
    // When a page navigation occurs, for a brief period
    // RenderViewImpl::GetWebWidget() will return a WebViewImpl instead of a
    // WebViewFrameWidget. Therefore, casting to WebFrameWidget is invalid and
    // could cause crashes. Also, WebView::mainFrame() could be a remote frame
    // which will yield a nullptr for localRoot() (https://crbug.com/664890).
    return nullptr;
  }
  return static_cast<blink::WebFrameWidget*>(widget);
}

blink::WebLocalFrame* TextInputClientObserver::GetFocusedFrame() const {
  if (auto* frame_widget = GetWebFrameWidget()) {
    blink::WebLocalFrame* localRoot = frame_widget->localRoot();
    RenderFrameImpl* render_frame = RenderFrameImpl::FromWebFrame(localRoot);
    if (!render_frame) {
      // TODO(ekaramad): Can this ever be nullptr? (https://crbug.com/664890).
      return nullptr;
    }
    blink::WebLocalFrame* focused =
        render_frame->render_view()->webview()->focusedFrame();
    return focused->localRoot() == localRoot ? focused : nullptr;
  }
  return nullptr;
}

#if defined(ENABLE_PLUGINS)
PepperPluginInstanceImpl* TextInputClientObserver::GetFocusedPepperPlugin()
    const {
  blink::WebLocalFrame* focusedFrame = GetFocusedFrame();
  return focusedFrame
             ? RenderFrameImpl::FromWebFrame(focusedFrame)
                   ->focused_pepper_plugin()
             : nullptr;
}
#endif

void TextInputClientObserver::OnStringAtPoint(gfx::Point point) {
#if defined(OS_MACOSX)
  blink::WebPoint baselinePoint;
  NSAttributedString* string = nil;

  if (auto* frame_widget = GetWebFrameWidget()) {
    string = blink::WebSubstringUtil::attributedWordAtPoint(frame_widget, point,
                                                   baselinePoint);
  }

  std::unique_ptr<const mac::AttributedStringCoder::EncodedString> encoded(
      mac::AttributedStringCoder::Encode(string));
  Send(new TextInputClientReplyMsg_GotStringAtPoint(
      render_widget_->routing_id(), *encoded.get(), baselinePoint));
#else
  NOTIMPLEMENTED();
#endif
}

void TextInputClientObserver::OnCharacterIndexForPoint(gfx::Point point) {
  blink::WebPoint web_point(point);
  uint32_t index = 0U;
  if (auto* frame = GetFocusedFrame())
    index = static_cast<uint32_t>(frame->characterIndexForPoint(web_point));

  Send(new TextInputClientReplyMsg_GotCharacterIndexForPoint(
      render_widget_->routing_id(), index));
}

void TextInputClientObserver::OnFirstRectForCharacterRange(gfx::Range range) {
  gfx::Rect rect;
#if defined(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* focused_plugin = GetFocusedPepperPlugin();
  if (focused_plugin) {
    rect = focused_plugin->GetCaretBounds();
  } else
#endif
  {
    blink::WebLocalFrame* frame = GetFocusedFrame();
    // TODO(yabinh): Null check should not be necessary.
    // See crbug.com/304341
    if (frame) {
      blink::WebRect web_rect;
      frame->firstRectForCharacterRange(range.start(), range.length(),
                                        web_rect);
      rect = web_rect;
    }
  }
  Send(new TextInputClientReplyMsg_GotFirstRectForRange(
      render_widget_->routing_id(), rect));
}

void TextInputClientObserver::OnStringForRange(gfx::Range range) {
#if defined(OS_MACOSX)
  blink::WebPoint baselinePoint;
  NSAttributedString* string = nil;
  blink::WebLocalFrame* frame = GetFocusedFrame();
  // TODO(yabinh): Null check should not be necessary.
  // See crbug.com/304341
  if (frame) {
    string = blink::WebSubstringUtil::attributedSubstringInRange(
        frame, range.start(), range.length(), &baselinePoint);
  }
  std::unique_ptr<const mac::AttributedStringCoder::EncodedString> encoded(
      mac::AttributedStringCoder::Encode(string));
  Send(new TextInputClientReplyMsg_GotStringForRange(
      render_widget_->routing_id(), *encoded.get(), baselinePoint));
#else
  NOTIMPLEMENTED();
#endif
}

}  // namespace content
