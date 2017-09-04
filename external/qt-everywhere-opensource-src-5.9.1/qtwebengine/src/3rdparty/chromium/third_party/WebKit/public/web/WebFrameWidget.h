/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebFrameWidget_h
#define WebFrameWidget_h

#include "../platform/WebCommon.h"
#include "../platform/WebDragOperation.h"
#include "../platform/WebPageVisibilityState.h"
#include "public/web/WebWidget.h"

namespace blink {

class WebDragData;
class WebLocalFrame;
class WebInputMethodController;
class WebView;
class WebWidgetClient;

class WebFrameWidget : public WebWidget {
 public:
  BLINK_EXPORT static WebFrameWidget* create(WebWidgetClient*, WebLocalFrame*);
  // Creates a frame widget for a WebView. Temporary helper to help transition
  // away from WebView inheriting WebWidget.
  // TODO(dcheng): Remove once transition is complete.
  BLINK_EXPORT static WebFrameWidget* create(WebWidgetClient*,
                                             WebView*,
                                             WebLocalFrame* mainFrame);

  // Sets the visibility of the WebFrameWidget.
  // We still track page-level visibility, but additionally we need to notify a
  // WebFrameWidget when its owning RenderWidget receives a Show or Hide
  // directive, so that it knows whether it needs to draw or not.
  virtual void setVisibilityState(WebPageVisibilityState visibilityState) {}

  // Makes the WebFrameWidget transparent.  This is useful if you want to have
  // some custom background rendered behind it.
  virtual bool isTransparent() const = 0;
  virtual void setIsTransparent(bool) = 0;

  // Sets the base color used for this WebFrameWidget's background. This is in
  // effect the default background color used for pages with no
  // background-color style in effect, or used as the alpha-blended basis for
  // any pages with translucent background-color style. (For pages with opaque
  // background-color style, this property is effectively ignored).
  // Setting this takes effect for the currently loaded page, if any, and
  // persists across subsequent navigations. Defaults to white prior to the
  // first call to this method.
  virtual void setBaseBackgroundColor(WebColor) = 0;

  // Returns the local root of this WebFrameWidget.
  virtual WebLocalFrame* localRoot() const = 0;

  // WebWidget implementation.
  bool isWebFrameWidget() const final { return true; }

  // Current instance of the active WebInputMethodController, that is, the
  // WebInputMethodController corresponding to (and owned by) the focused
  // WebLocalFrameImpl. It might return nullptr when there are no focused
  // frames or possibly when the WebFrameWidget does not accept IME events.
  virtual WebInputMethodController* getActiveWebInputMethodController()
      const = 0;

  // Callback methods when a drag-and-drop operation is trying to drop something
  // on the WebFrameWidget.
  virtual WebDragOperation dragTargetDragEnter(
      const WebDragData&,
      const WebPoint& pointInViewport,
      const WebPoint& screenPoint,
      WebDragOperationsMask operationsAllowed,
      int modifiers) = 0;
  virtual WebDragOperation dragTargetDragOver(
      const WebPoint& pointInViewport,
      const WebPoint& screenPoint,
      WebDragOperationsMask operationsAllowed,
      int modifiers) = 0;
  virtual void dragTargetDragLeave() = 0;
  virtual void dragTargetDrop(const WebDragData&,
                              const WebPoint& pointInViewport,
                              const WebPoint& screenPoint,
                              int modifiers) = 0;

  // Notifies the WebFrameWidget that a drag has terminated.
  virtual void dragSourceEndedAt(const WebPoint& pointInViewport,
                                 const WebPoint& screenPoint,
                                 WebDragOperation) = 0;

  // Notfies the WebFrameWidget that the system drag and drop operation has
  // ended.
  virtual void dragSourceSystemDragEnded() = 0;
};

}  // namespace blink

#endif
