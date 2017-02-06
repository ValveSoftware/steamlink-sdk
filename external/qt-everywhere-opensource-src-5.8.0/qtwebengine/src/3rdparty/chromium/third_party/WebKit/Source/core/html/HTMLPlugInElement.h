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

#include "bindings/core/v8/SharedPersistent.h"
#include "core/CoreExport.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/api/LayoutEmbeddedItem.h"

#include <v8.h>

namespace blink {

class HTMLImageLoader;
class LayoutPart;
class Widget;

enum PreferPlugInsForImagesOption {
    ShouldPreferPlugInsForImages,
    ShouldNotPreferPlugInsForImages
};

class CORE_EXPORT HTMLPlugInElement : public HTMLFrameOwnerElement {
public:
    ~HTMLPlugInElement() override;
    DECLARE_VIRTUAL_TRACE();

    void resetInstance();
    // TODO(dcheng): Consider removing this, since HTMLEmbedElementLegacyCall
    // and HTMLObjectElementLegacyCall usage is extremely low.
    SharedPersistent<v8::Object>* pluginWrapper();
    Widget* pluginWidget() const;
    bool canProcessDrag() const;
    const String& url() const { return m_url; }

    // Public for FrameView::addPartToUpdate()
    bool needsWidgetUpdate() const { return m_needsWidgetUpdate; }
    void setNeedsWidgetUpdate(bool needsWidgetUpdate) { m_needsWidgetUpdate = needsWidgetUpdate; }
    void updateWidget();

    bool shouldAccelerate() const;

    void requestPluginCreationWithoutLayoutObjectIfPossible();
    void createPluginWithoutLayoutObject();

    void removedFrom(ContainerNode* insertionPoint) override;

protected:
    HTMLPlugInElement(const QualifiedName& tagName, Document&, bool createdByParser, PreferPlugInsForImagesOption);

    // Node functions:
    void didMoveToNewDocument(Document& oldDocument) override;

    // Element functions:
    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) override;

    virtual bool hasFallbackContent() const;
    virtual bool useFallbackContent() const;
    // Create or update the LayoutPart and return it, triggering layout if
    // necessary.
    virtual LayoutPart* layoutPartForJSBindings() const;

    bool isImageType();
    bool shouldPreferPlugInsForImages() const { return m_shouldPreferPlugInsForImages; }
    LayoutEmbeddedItem layoutEmbeddedItem() const;
    bool allowedToLoadFrameURL(const String& url);
    bool requestObject(const String& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues);
    bool shouldUsePlugin(const KURL&, const String& mimeType, bool hasFallback, bool& useFallback);

    void dispatchErrorEvent();
    void lazyReattachIfNeeded();

    String m_serviceType;
    String m_url;
    KURL m_loadedUrl;
    Member<HTMLImageLoader> m_imageLoader;
    bool m_isDelayingLoadEvent;

private:
    // EventTarget functions:
    void removeAllEventListeners() final;

    // Node functions:
    bool canContainRangeEndPoint() const override { return false; }
    bool canStartSelection() const override;
    bool willRespondToMouseClickEvents() final;
    void defaultEventHandler(Event*) final;
    void attach(const AttachContext& = AttachContext()) final;
    void detach(const AttachContext& = AttachContext()) final;
    void finishParsingChildren() final;

    // Element functions:
    LayoutObject* createLayoutObject(const ComputedStyle&) override;
    bool supportsFocus() const final { return true; }
    bool layoutObjectIsFocusable() const final;
    bool isKeyboardFocusable() const final;
    void didAddUserAgentShadowRoot(ShadowRoot&) final;

    // HTMLElement function:
    bool hasCustomFocusLogic() const override;
    bool isPluginElement() const final;

    // Return any existing LayoutPart without triggering relayout, or 0 if it
    // doesn't yet exist.
    virtual LayoutPart* existingLayoutPart() const = 0;
    virtual void updateWidgetInternal() = 0;

    bool loadPlugin(const KURL&, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback, bool requireLayoutObject);
    // Perform checks after we have determined that a plugin will be used to
    // show the object (i.e after allowedToLoadObject).
    bool allowedToLoadPlugin(const KURL&, const String& mimeType);
    // Perform checks based on the URL and MIME-type of the object to load.
    bool allowedToLoadObject(const KURL&, const String& mimeType);
    bool wouldLoadAsNetscapePlugin(const String& url, const String& serviceType);

    void setPersistedPluginWidget(Widget*);

    mutable RefPtr<SharedPersistent<v8::Object>> m_pluginWrapper;
    bool m_needsWidgetUpdate;
    bool m_shouldPreferPlugInsForImages;

    // Normally the Widget is stored in HTMLFrameOwnerElement::m_widget.
    // However, plugins can persist even when not rendered. In order to
    // prevent confusing code which may assume that widget() != null
    // means the frame is active, we save off m_widget here while
    // the plugin is persisting but not being displayed.
    Member<Widget> m_persistedPluginWidget;
};

inline bool isHTMLPlugInElement(const HTMLElement& element)
{
    return element.isPluginElement();
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLPlugInElement);

} // namespace blink

#endif // HTMLPlugInElement_h
