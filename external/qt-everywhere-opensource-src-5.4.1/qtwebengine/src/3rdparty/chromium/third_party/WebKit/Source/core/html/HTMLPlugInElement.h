/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights reserved.
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
 *
 */

#ifndef HTMLPlugInElement_h
#define HTMLPlugInElement_h

#include "bindings/v8/SharedPersistent.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include <v8.h>

struct NPObject;

namespace WebCore {

class HTMLImageLoader;
class RenderEmbeddedObject;
class RenderWidget;
class Widget;

enum PreferPlugInsForImagesOption {
    ShouldPreferPlugInsForImages,
    ShouldNotPreferPlugInsForImages
};

class HTMLPlugInElement : public HTMLFrameOwnerElement {
public:
    virtual ~HTMLPlugInElement();
    virtual void trace(Visitor*) OVERRIDE;

    void resetInstance();
    SharedPersistent<v8::Object>* pluginWrapper();
    Widget* pluginWidget() const;
    NPObject* getNPObject();
    bool canProcessDrag() const;
    const String& url() const { return m_url; }

    // Public for FrameView::addWidgetToUpdate()
    bool needsWidgetUpdate() const { return m_needsWidgetUpdate; }
    void setNeedsWidgetUpdate(bool needsWidgetUpdate) { m_needsWidgetUpdate = needsWidgetUpdate; }
    void updateWidget();

    bool shouldAccelerate() const;

    void requestPluginCreationWithoutRendererIfPossible();
    void createPluginWithoutRenderer();

protected:
    HTMLPlugInElement(const QualifiedName& tagName, Document&, bool createdByParser, PreferPlugInsForImagesOption);

    // Node functions:
    virtual void didMoveToNewDocument(Document& oldDocument) OVERRIDE;

    // Element functions:
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;

    virtual bool hasFallbackContent() const;
    virtual bool useFallbackContent() const;
    // Create or update the RenderWidget and return it, triggering layout if
    // necessary.
    virtual RenderWidget* renderWidgetForJSBindings() const;

    bool isImageType();
    bool shouldPreferPlugInsForImages() const { return m_shouldPreferPlugInsForImages; }
    RenderEmbeddedObject* renderEmbeddedObject() const;
    bool allowedToLoadFrameURL(const String& url);
    bool requestObject(const String& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues);
    bool shouldUsePlugin(const KURL&, const String& mimeType, bool hasFallback, bool& useFallback);

    void dispatchErrorEvent();

    String m_serviceType;
    String m_url;
    KURL m_loadedUrl;
    OwnPtrWillBeMember<HTMLImageLoader> m_imageLoader;
    bool m_isDelayingLoadEvent;

private:
    // EventTarget functions:
    virtual void removeAllEventListeners() OVERRIDE FINAL;

    // Node functions:
    virtual bool canContainRangeEndPoint() const OVERRIDE { return false; }
    virtual bool willRespondToMouseClickEvents() OVERRIDE FINAL;
    virtual void defaultEventHandler(Event*) OVERRIDE FINAL;
    virtual void attach(const AttachContext& = AttachContext()) OVERRIDE FINAL;
    virtual void detach(const AttachContext& = AttachContext()) OVERRIDE FINAL;
    virtual void finishParsingChildren() OVERRIDE FINAL;

    // Element functions:
    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    virtual void willRecalcStyle(StyleRecalcChange) OVERRIDE FINAL;
    virtual bool supportsFocus() const OVERRIDE FINAL { return true; }
    virtual bool rendererIsFocusable() const OVERRIDE FINAL;
    virtual bool isKeyboardFocusable() const OVERRIDE FINAL;
    virtual void didAddUserAgentShadowRoot(ShadowRoot&) OVERRIDE FINAL;
    virtual void willAddFirstAuthorShadowRoot() OVERRIDE FINAL;

    // HTMLElement function:
    virtual bool hasCustomFocusLogic() const OVERRIDE;
    virtual bool isPluginElement() const OVERRIDE FINAL;

    // Return any existing RenderWidget without triggering relayout, or 0 if it
    // doesn't yet exist.
    virtual RenderWidget* existingRenderWidget() const = 0;
    virtual void updateWidgetInternal() = 0;

    enum DisplayState {
        Restarting,
        RestartingWithPendingMouseClick,
        Playing
    };
    DisplayState displayState() const { return m_displayState; }
    void setDisplayState(DisplayState state) { m_displayState = state; }
    bool loadPlugin(const KURL&, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback, bool requireRenderer);
    bool pluginIsLoadable(const KURL&, const String& mimeType);
    bool wouldLoadAsNetscapePlugin(const String& url, const String& serviceType);

    mutable RefPtr<SharedPersistent<v8::Object> > m_pluginWrapper;
    NPObject* m_NPObject;
    bool m_isCapturingMouseEvents;
    bool m_needsWidgetUpdate;
    bool m_shouldPreferPlugInsForImages;
    DisplayState m_displayState;

    // Normally the Widget is stored in HTMLFrameOwnerElement::m_widget.
    // However, plugins can persist even when not rendered. In order to
    // prevent confusing code which may assume that widget() != null
    // means the frame is active, we save off m_widget here while
    // the plugin is persisting but not being displayed.
    RefPtr<Widget> m_persistedPluginWidget;
};

inline bool isHTMLPlugInElement(const Element& element)
{
    return element.isHTMLElement() && toHTMLElement(element).isPluginElement();
}

inline bool isHTMLPlugInElement(const HTMLElement& element)
{
    return element.isPluginElement();
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLPlugInElement);

} // namespace WebCore

#endif // HTMLPlugInElement_h
