/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2014 Opera Software ASA. All rights reserved.
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

#ifndef WebPluginContainerImpl_h
#define WebPluginContainerImpl_h

#include "core/frame/LocalFrameLifecycleObserver.h"
#include "core/plugins/PluginView.h"
#include "platform/Widget.h"
#include "public/web/WebPluginContainer.h"
#include "web/WebExport.h"
#include "wtf/Compiler.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

struct NPObject;

namespace blink {

class GestureEvent;
class HTMLPlugInElement;
class IntRect;
class KeyboardEvent;
class MouseEvent;
class ResourceError;
class ResourceResponse;
class TouchEvent;
class WebPlugin;
class WheelEvent;
class Widget;
struct WebPrintParams;
struct WebPrintPresetOptions;

class WEB_EXPORT WebPluginContainerImpl final : public PluginView, WTF_NON_EXPORTED_BASE(public WebPluginContainer), public LocalFrameLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(WebPluginContainerImpl);
    USING_PRE_FINALIZER(WebPluginContainerImpl, dispose);
public:
    static WebPluginContainerImpl* create(HTMLPlugInElement* element, WebPlugin* webPlugin)
    {
        return new WebPluginContainerImpl(element, webPlugin);
    }

    // PluginView methods
    WebLayer* platformLayer() const override;
    v8::Local<v8::Object> scriptableObject(v8::Isolate*) override;
    bool supportsKeyboardFocus() const override;
    bool supportsInputMethod() const override;
    bool canProcessDrag() const override;
    bool wantsWheelEvents() override;
    void updateAllLifecyclePhases() override;
    void invalidatePaintIfNeeded() override { issuePaintInvalidations(); }

    // Widget methods
    void setFrameRect(const IntRect&) override;
    void paint(GraphicsContext&, const CullRect&) const override;
    void invalidateRect(const IntRect&) override;
    void setFocus(bool, WebFocusType) override;
    void show() override;
    void hide() override;
    void handleEvent(Event*) override;
    void frameRectsChanged() override;
    void setParentVisible(bool) override;
    void widgetGeometryMayHaveChanged() override;
    bool isPluginContainer() const override { return true; }
    void eventListenersRemoved() override;

    // WebPluginContainer methods
    WebElement element() override;
    WebDocument document() override;
    void dispatchProgressEvent(const WebString& type, bool lengthComputable, unsigned long long loaded, unsigned long long total, const WebString& url) override;
    void enqueueMessageEvent(const WebDOMMessageEvent&) override;
    void invalidate() override;
    void invalidateRect(const WebRect&) override;
    void scrollRect(const WebRect&) override;
    void scheduleAnimation() override;
    void reportGeometry() override;
    v8::Local<v8::Object> v8ObjectForElement() override;
    WebString executeScriptURL(const WebURL&, bool popupsAllowed) override;
    void loadFrameRequest(const WebURLRequest&, const WebString& target) override;
    bool isRectTopmost(const WebRect&) override;
    void requestTouchEventType(TouchEventRequestType) override;
    void setWantsWheelEvents(bool) override;
    WebPoint rootFrameToLocalPoint(const WebPoint&) override;
    WebPoint localToRootFramePoint(const WebPoint&) override;

    // Non-Oilpan, this cannot be null. With Oilpan, it will be
    // null when in a disposed state, pending finalization during the next GC.
    WebPlugin* plugin() override { return m_webPlugin; }
    void setPlugin(WebPlugin*) override;

    float deviceScaleFactor() override;
    float pageScaleFactor() override;
    float pageZoomFactor() override;

    void setWebLayer(WebLayer*) override;

    void requestFullscreen() override;
    bool isFullscreenElement() const override;
    void cancelFullscreen() override;

    // Printing interface. The plugin can support custom printing
    // (which means it controls the layout, number of pages etc).
    // Whether the plugin supports its own paginated print. The other print
    // interface methods are called only if this method returns true.
    bool supportsPaginatedPrint() const;
    // If the plugin content should not be scaled to the printable area of
    // the page, then this method should return true.
    bool isPrintScalingDisabled() const;
    // Returns true on success and sets the out parameter to the print preset options for the document.
    bool getPrintPresetOptionsFromDocument(WebPrintPresetOptions*) const;
    // Sets up printing at the specified WebPrintParams. Returns the number of pages to be printed at these settings.
    int printBegin(const WebPrintParams&) const;
    // Prints the page specified by pageNumber (0-based index) into the supplied canvas.
    void printPage(int pageNumber, GraphicsContext&, const IntRect& paintRect);
    // Ends the print operation.
    void printEnd();

    // Copy the selected text.
    void copy();

    // Pass the edit command to the plugin.
    bool executeEditCommand(const WebString& name);
    bool executeEditCommand(const WebString& name, const WebString& value);

    // Resource load events for the plugin's source data:
    void didReceiveResponse(const ResourceResponse&) override;
    void didReceiveData(const char *data, int dataLength) override;
    void didFinishLoading();
    void didFailLoading(const ResourceError&);

    DECLARE_VIRTUAL_TRACE();
    void dispose() override;

private:
    // Sets |windowRect| to the content rect of the plugin in screen space.
    // Sets |clippedAbsoluteRect| to the visible rect for the plugin, clipped to the visible screen of the root frame, in local space of the plugin.
    // Sets |unclippedAbsoluteRect| to the visible rect for the plugin (but without also clipping to the screen), in local space of the plugin.
    void computeClipRectsForPlugin(
        const HTMLFrameOwnerElement* pluginOwnerElement, IntRect& windowRect, IntRect& clippedLocalRect, IntRect& unclippedIntLocalRect) const;

    WebPluginContainerImpl(HTMLPlugInElement*, WebPlugin*);
    ~WebPluginContainerImpl() override;

    void handleMouseEvent(MouseEvent*);
    void handleDragEvent(MouseEvent*);
    void handleWheelEvent(WheelEvent*);
    void handleKeyboardEvent(KeyboardEvent*);
    void handleTouchEvent(TouchEvent*);
    void handleGestureEvent(GestureEvent*);

    void synthesizeMouseEventIfPossible(TouchEvent*);

    void focusPlugin();

    void issuePaintInvalidations();

    void calculateGeometry(
        IntRect& windowRect,
        IntRect& clipRect,
        IntRect& unobscuredRect,
        Vector<IntRect>& cutOutRects);
    void windowCutOutRects(
        const IntRect& frameRect,
        Vector<IntRect>& cutOutRects);

    friend class WebPluginContainerTest;

    Member<HTMLPlugInElement> m_element;
    WebPlugin* m_webPlugin;

    WebLayer* m_webLayer;

    IntRect m_pendingInvalidationRect;

    TouchEventRequestType m_touchEventRequestType;
    bool m_wantsWheelEvents;

    bool m_isDisposed;
};

DEFINE_TYPE_CASTS(WebPluginContainerImpl, Widget, widget, widget->isPluginContainer(), widget.isPluginContainer());
// Unlike Widget, we need not worry about object type for container.
// WebPluginContainerImpl is the only subclass of WebPluginContainer.
DEFINE_TYPE_CASTS(WebPluginContainerImpl, WebPluginContainer, container, true, true);

} // namespace blink

#endif
