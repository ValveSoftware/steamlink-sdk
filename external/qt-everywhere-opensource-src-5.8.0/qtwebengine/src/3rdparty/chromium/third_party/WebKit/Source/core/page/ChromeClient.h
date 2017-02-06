/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple, Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef ChromeClient_h
#define ChromeClient_h

#include "base/gtest_prod_util.h"
#include "core/CoreExport.h"
#include "core/dom/AXObjectCache.h"
#include "core/inspector/ConsoleTypes.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/NavigationPolicy.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/Cursor.h"
#include "platform/HostWindow.h"
#include "platform/PopupMenu.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"
#include "public/platform/BlameContext.h"
#include "public/platform/WebDragOperation.h"
#include "public/platform/WebEventListenerProperties.h"
#include "public/platform/WebFocusType.h"
#include "wtf/Forward.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class AXObject;
class ColorChooser;
class ColorChooserClient;
class CompositorAnimationTimeline;
class CompositorProxyClient;
class DateTimeChooser;
class DateTimeChooserClient;
class Element;
class FileChooser;
class FloatPoint;
class Frame;
class GraphicsContext;
class GraphicsLayer;
class HTMLFormControlElement;
class HTMLInputElement;
class HTMLSelectElement;
class HitTestResult;
class IntRect;
class LocalFrame;
class Node;
class Page;
class PaintArtifact;
class PopupOpeningObserver;
class WebDragData;
class WebFrameScheduler;
class WebImage;

struct CompositedSelection;
struct DateTimeChooserParameters;
struct FrameLoadRequest;
struct GraphicsDeviceAdapter;
struct ViewportDescription;
struct WebPoint;
struct WindowFeatures;

class CORE_EXPORT ChromeClient : public HostWindow {
public:
    virtual void chromeDestroyed() = 0;

    // The specified rectangle is adjusted for the minimum window size and the
    // screen, then setWindowRect with the adjusted rectangle is called.
    void setWindowRectWithAdjustment(const IntRect&);
    virtual IntRect windowRect() = 0;

    virtual IntRect pageRect() = 0;

    virtual void focus() = 0;

    virtual bool canTakeFocus(WebFocusType) = 0;
    virtual void takeFocus(WebFocusType) = 0;

    virtual void focusedNodeChanged(Node*, Node*) = 0;

    virtual bool hadFormInteraction() const = 0;

    virtual void beginLifecycleUpdates() = 0;

    // Start a system drag and drop operation.
    virtual void startDragging(LocalFrame*, const WebDragData&, WebDragOperationsMask, const WebImage& dragImage, const WebPoint& dragImageOffset) = 0;
    virtual bool acceptsLoadDrops() const = 0;

    // The LocalFrame pointer provides the ChromeClient with context about which
    // LocalFrame wants to create the new Page. Also, the newly created window
    // should not be shown to the user until the ChromeClient of the newly
    // created Page has its show method called.
    // The FrameLoadRequest parameter is only for ChromeClient to check if the
    // request could be fulfilled. The ChromeClient should not load the request.
    virtual Page* createWindow(LocalFrame*, const FrameLoadRequest&, const WindowFeatures&, NavigationPolicy) = 0;
    virtual void show(NavigationPolicy = NavigationPolicyIgnore) = 0;

    void setWindowFeatures(const WindowFeatures&);

    // All the parameters should be in viewport space. That is, if an event
    // scrolls by 10 px, but due to a 2X page scale we apply a 5px scroll to the
    // root frame, all of which is handled as overscroll, we should return 10px
    // as the overscrollDelta.
    virtual void didOverscroll(
        const FloatSize& overscrollDelta,
        const FloatSize& accumulatedOverscroll,
        const FloatPoint& positionInViewport,
        const FloatSize& velocityInViewport) = 0;

    virtual void setToolbarsVisible(bool) = 0;
    virtual bool toolbarsVisible() = 0;

    virtual void setStatusbarVisible(bool) = 0;
    virtual bool statusbarVisible() = 0;

    virtual void setScrollbarsVisible(bool) = 0;
    virtual bool scrollbarsVisible() = 0;

    virtual void setMenubarVisible(bool) = 0;
    virtual bool menubarVisible() = 0;

    virtual void setResizable(bool) = 0;

    virtual bool shouldReportDetailedMessageForSource(LocalFrame&, const String& source) = 0;
    virtual void addMessageToConsole(LocalFrame*, MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID, const String& stackTrace) = 0;

    virtual bool canOpenBeforeUnloadConfirmPanel() = 0;
    bool openBeforeUnloadConfirmPanel(const String& message, LocalFrame*, bool isReload);

    virtual void closeWindowSoon() = 0;

    bool openJavaScriptAlert(LocalFrame*, const String&);
    bool openJavaScriptConfirm(LocalFrame*, const String&);
    bool openJavaScriptPrompt(LocalFrame*, const String& message, const String& defaultValue, String& result);
    virtual void setStatusbarText(const String&) = 0;
    virtual bool tabsToLinks() = 0;

    virtual void* webView() const = 0;

    virtual IntRect windowResizerRect() const = 0;

    // Methods used by HostWindow.
    virtual WebScreenInfo screenInfo() const = 0;
    virtual void setCursor(const Cursor&, LocalFrame* localRoot) = 0;
    // End methods used by HostWindow.
    virtual Cursor lastSetCursorForTesting() const = 0;

    virtual void dispatchViewportPropertiesDidChange(const ViewportDescription&) const { }

    virtual void contentsSizeChanged(LocalFrame*, const IntSize&) const = 0;
    virtual void pageScaleFactorChanged() const { }
    virtual float clampPageScaleFactorToLimits(float scale) const { return scale; }
    virtual void layoutUpdated(LocalFrame*) const { }

    void mouseDidMoveOverElement(const HitTestResult&);
    virtual void setToolTip(const String&, TextDirection) = 0;
    void clearToolTip();

    void print(LocalFrame*);

    virtual void annotatedRegionsChanged() = 0;

    virtual ColorChooser* openColorChooser(LocalFrame*, ColorChooserClient*, const Color&) = 0;

    // This function is used for:
    //  - Mandatory date/time choosers if !ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    //  - Date/time choosers for types for which LayoutTheme::supportsCalendarPicker
    //    returns true, if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    //  - <datalist> UI for date/time input types regardless of
    //    ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    virtual DateTimeChooser* openDateTimeChooser(DateTimeChooserClient*, const DateTimeChooserParameters&) = 0;

    virtual void openTextDataListChooser(HTMLInputElement&)= 0;

    virtual void openFileChooser(LocalFrame*, PassRefPtr<FileChooser>) = 0;

    // Asychronous request to enumerate all files in a directory chosen by the user.
    virtual void enumerateChosenDirectory(FileChooser*) = 0;

    // Pass 0 as the GraphicsLayer to detach the root layer.
    // This sets the graphics layer for the LocalFrame's WebWidget, if it has
    // one. Otherwise it sets it for the WebViewImpl.
    virtual void attachRootGraphicsLayer(GraphicsLayer*, LocalFrame* localRoot) = 0;

    // In Slimming Paint v2, called when the paint artifact is updated, to allow
    // the underlying web widget to composite it.
    virtual void didPaint(const PaintArtifact&) { }

    virtual void attachCompositorAnimationTimeline(CompositorAnimationTimeline*, LocalFrame* localRoot) { }
    virtual void detachCompositorAnimationTimeline(CompositorAnimationTimeline*, LocalFrame* localRoot) { }

    virtual void enterFullScreenForElement(Element*) { }
    virtual void exitFullScreenForElement(Element*) { }

    virtual void clearCompositedSelection() { }
    virtual void updateCompositedSelection(const CompositedSelection&) { }

    virtual void setEventListenerProperties(WebEventListenerClass, WebEventListenerProperties) = 0;
    virtual WebEventListenerProperties eventListenerProperties(WebEventListenerClass) const = 0;
    virtual void setHasScrollEventHandlers(bool) = 0;
    virtual bool hasScrollEventHandlers() const = 0;

    virtual void setTouchAction(TouchAction) = 0;

    // Checks if there is an opened popup, called by LayoutMenuList::showPopup().
    virtual bool hasOpenedPopup() const = 0;
    virtual PopupMenu* openPopupMenu(LocalFrame&, HTMLSelectElement&) = 0;
    virtual DOMWindow* pagePopupWindowForTesting() const = 0;

    virtual void postAccessibilityNotification(AXObject*, AXObjectCache::AXNotification) { }
    virtual String acceptLanguages() = 0;

    enum DialogType {
        AlertDialog = 0,
        ConfirmDialog = 1,
        PromptDialog = 2,
        HTMLDialog = 3
    };
    virtual bool shouldOpenModalDialogDuringPageDismissal(const DialogType&, const String&, Document::PageDismissalType) const { return true; }

    virtual bool isSVGImageChromeClient() const { return false; }

    virtual bool requestPointerLock(LocalFrame*) { return false; }
    virtual void requestPointerUnlock(LocalFrame*) {}

    virtual IntSize minimumWindowSize() const { return IntSize(100, 100); }

    virtual bool isChromeClientImpl() const { return false; }

    virtual void didAssociateFormControls(const HeapVector<Member<Element>>&, LocalFrame*) { }
    virtual void didChangeValueInTextField(HTMLFormControlElement&) { }
    virtual void didEndEditingOnTextField(HTMLInputElement&) { }
    virtual void handleKeyboardEventOnTextField(HTMLInputElement&, KeyboardEvent&) { }
    virtual void textFieldDataListChanged(HTMLInputElement&) { }
    virtual void ajaxSucceeded(LocalFrame*) { }

    // Input method editor related functions.
    virtual void didCancelCompositionOnSelectionChange() { }
    virtual void willSetInputMethodState() { }
    virtual void didUpdateTextOfFocusedElementByNonUserInput() { }
    virtual void showImeIfNeeded() { }

    virtual void registerViewportLayers() const { }

    virtual void showUnhandledTapUIIfNeeded(IntPoint, Node*, bool) { }

    virtual void onMouseDown(Node*) { }

    virtual void didUpdateTopControls() const { }

    virtual void registerPopupOpeningObserver(PopupOpeningObserver*) = 0;
    virtual void unregisterPopupOpeningObserver(PopupOpeningObserver*) = 0;

    virtual CompositorProxyClient* createCompositorProxyClient(LocalFrame*) = 0;

    virtual FloatSize elasticOverscroll() const { return FloatSize(); }

    // Called when observed XHR, fetch, and other fetch request with non-GET
    // method is initiated from javascript. At this time, it is not guaranteed
    // that this is comprehensive.
    virtual void didObserveNonGetFetchFromScript() const {}

    virtual std::unique_ptr<WebFrameScheduler> createFrameScheduler(BlameContext*) = 0;

    // Returns the time of the beginning of the last beginFrame, in seconds, if any, and 0.0 otherwise.
    virtual double lastFrameTimeMonotonic() const { return 0.0; }

protected:
    ~ChromeClient() override { }

    virtual void showMouseOverURL(const HitTestResult&) = 0;
    virtual void setWindowRect(const IntRect&) = 0;
    virtual bool openBeforeUnloadConfirmPanelDelegate(LocalFrame*, bool isReload) = 0;
    virtual bool openJavaScriptAlertDelegate(LocalFrame*, const String&) = 0;
    virtual bool openJavaScriptConfirmDelegate(LocalFrame*, const String&) = 0;
    virtual bool openJavaScriptPromptDelegate(LocalFrame*, const String& message, const String& defaultValue, String& result) = 0;
    virtual void printDelegate(LocalFrame*) = 0;

private:
    bool canOpenModalIfDuringPageDismissal(Frame* mainFrame, DialogType, const String& message);
    void setToolTip(const HitTestResult&);

    LayoutPoint m_lastToolTipPoint;
    String m_lastToolTipText;

    FRIEND_TEST_ALL_PREFIXES(ChromeClientTest, SetToolTipFlood);
};

} // namespace blink

#endif // ChromeClient_h
