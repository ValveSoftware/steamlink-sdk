/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebViewClient_h
#define WebViewClient_h

#include "../platform/WebDragOperation.h"
#include "../platform/WebPageVisibilityState.h"
#include "../platform/WebString.h"
#include "WebAXEnums.h"
#include "WebContentDetectionResult.h"
#include "WebFrame.h"
#include "WebPopupType.h"
#include "WebTextDirection.h"
#include "WebWidgetClient.h"

namespace blink {

class WebAXObject;
class WebDateTimeChooserCompletion;
class WebDragData;
class WebFileChooserCompletion;
class WebHitTestResult;
class WebImage;
class WebNode;
class WebSpeechRecognizer;
class WebStorageNamespace;
class WebURL;
class WebURLRequest;
class WebView;
class WebWidget;
struct WebDateTimeChooserParams;
struct WebPoint;
struct WebPopupMenuInfo;
struct WebRect;
struct WebSize;
struct WebWindowFeatures;

// Since a WebView is a WebWidget, a WebViewClient is a WebWidgetClient.
// Virtual inheritance allows an implementation of WebWidgetClient to be
// easily reused as part of an implementation of WebViewClient.
class WebViewClient : protected WebWidgetClient {
public:
    // Factory methods -----------------------------------------------------

    // Create a new related WebView.  This method must clone its session storage
    // so any subsequent calls to createSessionStorageNamespace conform to the
    // WebStorage specification.
    // The request parameter is only for the client to check if the request
    // could be fulfilled.  The client should not load the request.
    // The policy parameter indicates how the new view will be displayed in
    // WebWidgetClient::show.
    virtual WebView* createView(WebLocalFrame* creator,
                                const WebURLRequest& request,
                                const WebWindowFeatures& features,
                                const WebString& name,
                                WebNavigationPolicy policy,
                                bool suppressOpener) {
        return 0;
    }

    // Create a new popup WebWidget.
    virtual WebWidget* createPopupMenu(WebPopupType) { return 0; }

    // Create a session storage namespace object associated with this WebView.
    virtual WebStorageNamespace* createSessionStorageNamespace() { return 0; }


    // Misc ----------------------------------------------------------------

    // Called when script in the page calls window.print().  If frame is
    // non-null, then it selects a particular frame, including its
    // children, to print.  Otherwise, the main frame and its children
    // should be printed.
    virtual void printPage(WebLocalFrame*) { }

    // This method enumerates all the files in the path. It returns immediately
    // and asynchronously invokes the WebFileChooserCompletion with all the
    // files in the directory. Returns false if the WebFileChooserCompletion
    // will never be called.
    virtual bool enumerateChosenDirectory(const WebString& path, WebFileChooserCompletion*) { return false; }

    // Called when PageImportanceSignals for the WebView is updated.
    virtual void pageImportanceSignalsChanged() { }

    // Editing -------------------------------------------------------------

    // These methods allow the client to intercept and overrule editing
    // operations.
    virtual void didCancelCompositionOnSelectionChange() { }
    virtual void didChangeContents() { }

    // This method is called in response to WebView's handleInputEvent()
    // when the default action for the current keyboard event is not
    // suppressed by the page, to give the embedder a chance to handle
    // the keyboard event specially.
    //
    // Returns true if the keyboard event was handled by the embedder,
    // indicating that the default action should be suppressed.
    virtual bool handleCurrentKeyboardEvent() { return false; }

    // Dialogs -------------------------------------------------------------

    // Ask users to choose date/time for the specified parameters. When a user
    // chooses a value, an implementation of this function should call
    // WebDateTimeChooserCompletion::didChooseValue or didCancelChooser. If the
    // implementation opened date/time chooser UI successfully, it should return
    // true. This function is used only if ExternalDateTimeChooser is used.
    virtual bool openDateTimeChooser(const WebDateTimeChooserParams&, WebDateTimeChooserCompletion*) { return false; }

    // Show a notification popup for the specified form validation messages
    // besides the anchor rectangle. An implementation of this function should
    // not hide the popup until hideValidationMessage call.
    virtual void showValidationMessage(const WebRect& anchorInViewport, const WebString& mainText, WebTextDirection mainTextDir, const WebString& supplementalText, WebTextDirection supplementalTextDir) { }

    // Hide notifation popup for form validation messages.
    virtual void hideValidationMessage() { }

    // Move the existing notifation popup to the new anchor position.
    virtual void moveValidationMessage(const WebRect& anchorInViewport) { }


    // UI ------------------------------------------------------------------

    // Called when script modifies window.status
    virtual void setStatusText(const WebString&) { }

    // Called when hovering over an anchor with the given URL.
    virtual void setMouseOverURL(const WebURL&) { }

    // Called when keyboard focus switches to an anchor with the given URL.
    virtual void setKeyboardFocusURL(const WebURL&) { }

    // Called when a drag-n-drop operation should begin.
    virtual void startDragging(WebLocalFrame*, const WebDragData&, WebDragOperationsMask, const WebImage&, const WebPoint& dragImageOffset) { }

    // Called to determine if drag-n-drop operations may initiate a page
    // navigation.
    virtual bool acceptsLoadDrops() { return true; }

    // Take focus away from the WebView by focusing an adjacent UI element
    // in the containing window.
    virtual void focusNext() { }
    virtual void focusPrevious() { }

    // Called when a new node gets focused. |fromNode| is the previously focused node, |toNode|
    // is the newly focused node. Either can be null.
    virtual void focusedNodeChanged(const WebNode& fromNode, const WebNode& toNode) { }

    // Indicates two things:
    //   1) This view may have a new layout now.
    //   2) Calling layout() is a no-op.
    // After calling WebWidget::layout(), expect to get this notification
    // unless the view did not need a layout.
    virtual void didUpdateLayout() { }

    // Return true to swallow the input event if the embedder will start a disambiguation popup
    virtual bool didTapMultipleTargets(const WebSize& visualViewportOffset, const WebRect& touchRect, const WebVector<WebRect>& targetRects) { return false; }

    // Returns comma separated list of accept languages.
    virtual WebString acceptLanguages() { return WebString(); }


    // Session history -----------------------------------------------------

    // Tells the embedder to navigate back or forward in session history by
    // the given offset (relative to the current position in session
    // history).
    virtual void navigateBackForwardSoon(int offset) { }

    // Returns the number of history items before/after the current
    // history item.
    virtual int historyBackListCount() { return 0; }
    virtual int historyForwardListCount() { return 0; }


    // Developer tools -----------------------------------------------------

    // Called to notify the client that the inspector's settings were
    // changed and should be saved.  See WebView::inspectorSettings.
    virtual void didUpdateInspectorSettings() { }

    virtual void didUpdateInspectorSetting(const WebString& key, const WebString& value) { }


    // Speech --------------------------------------------------------------

    // Access the embedder API for speech recognition services.
    virtual WebSpeechRecognizer* speechRecognizer() { return 0; }


    // Zoom ----------------------------------------------------------------

    // Informs the browser that the zoom levels for this frame have changed from
    // the default values.
    virtual void zoomLimitsChanged(double minimumLevel, double maximumLevel) { }

    // Informs the browser that the page scale has changed.
    virtual void pageScaleFactorChanged() { }


    // Content detection ----------------------------------------------------

    // Retrieves detectable content (e.g., email addresses, phone numbers)
    // around a hit test result. The embedder should use platform-specific
    // content detectors to analyze the region around the hit test result.
    virtual WebContentDetectionResult detectContentAround(const WebHitTestResult&) { return WebContentDetectionResult(); }

    // Schedules a new content intent with the provided url.
    // The boolean flag is set to true when the user gesture has been applied
    // to the element from the main frame.
    virtual void scheduleContentIntent(const WebURL&, bool isMainFrame) { }

    // Cancels any previously scheduled content intents that have not yet launched.
    virtual void cancelScheduledContentIntents() { }


    // Draggable regions ----------------------------------------------------

    // Informs the browser that the draggable regions have been updated.
    virtual void draggableRegionsChanged() { }

    // TODO(lfg): These methods are only exposed through WebViewClient while we
    // refactor WebView to not inherit from WebWidget.
    // WebWidgetClient overrides.
    bool allowsBrokenNullLayerTreeView() const override { return false; }
    void closeWidgetSoon() override {}
    void convertViewportToWindow(WebRect* rect) override {}
    void convertWindowToViewport(WebFloatRect* rect) override {}
    void didAutoResize(const WebSize& newSize) override {}
    void didChangeCursor(const WebCursorInfo&) override {}
    void didFocus() override {}
    void didHandleGestureEvent(const WebGestureEvent& event, bool eventCancelled) override {}
    void didInvalidateRect(const WebRect&) override {}
    void didMeaningfulLayout(WebMeaningfulLayout) override {}
    void didOverscroll(const WebFloatSize& overscrollDelta, const WebFloatSize& accumulatedOverscroll, const WebFloatPoint& positionInViewport, const WebFloatSize& velocityInViewport) override {}
    void didUpdateTextOfFocusedElementByNonUserInput() override {}
    void hasTouchEventHandlers(bool) override {}
    void initializeLayerTreeView() override {}
    WebLayerTreeView* layerTreeView() override { return 0; }
    void onMouseDown(const WebNode& mouseDownNode) override {}
    void resetInputMethod() override {}
    WebRect rootWindowRect() override { return WebRect(); }
    void scheduleAnimation() override {}
    WebScreenInfo screenInfo() override { return WebScreenInfo(); }
    void setToolTipText(const WebString&, WebTextDirection hint) override {}
    void setTouchAction(WebTouchAction touchAction) override {}
    void setWindowRect(const WebRect&) override {}
    void showImeIfNeeded() override {}
    void showUnhandledTapUIIfNeeded(const WebPoint& tappedPosition, const WebNode& tappedNode, bool pageChanged) override {}
    void show(WebNavigationPolicy) override {}
    WebRect windowRect() override { return WebRect(); }
    WebRect windowResizerRect() override { return WebRect(); }

protected:
    ~WebViewClient() { }
};

} // namespace blink

#endif
