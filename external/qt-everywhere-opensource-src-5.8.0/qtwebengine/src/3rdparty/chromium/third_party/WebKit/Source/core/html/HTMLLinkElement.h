/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef HTMLLinkElement_h
#define HTMLLinkElement_h

#include "core/CoreExport.h"
#include "core/css/CSSStyleSheet.h"
#include "core/dom/DOMTokenList.h"
#include "core/dom/IconURL.h"
#include "core/dom/StyleEngineContext.h"
#include "core/fetch/ResourceOwner.h"
#include "core/fetch/StyleSheetResource.h"
#include "core/fetch/StyleSheetResourceClient.h"
#include "core/html/HTMLElement.h"
#include "core/html/LinkRelAttribute.h"
#include "core/html/LinkResource.h"
#include "core/html/RelList.h"
#include "core/loader/LinkLoader.h"
#include "core/loader/LinkLoaderClient.h"

namespace blink {

class HTMLLinkElement;
class KURL;
class LinkImport;

template<typename T> class EventSender;
using LinkEventSender = EventSender<HTMLLinkElement>;

//
// LinkStyle handles dynamically change-able link resources, which is
// typically @rel="stylesheet".
//
// It could be @rel="shortcut icon" or something else though. Each of
// types might better be handled by a separate class, but dynamically
// changing @rel makes it harder to move such a design so we are
// sticking current way so far.
//
class LinkStyle final : public LinkResource, ResourceOwner<StyleSheetResource> {
    USING_GARBAGE_COLLECTED_MIXIN(LinkStyle);
public:
    static LinkStyle* create(HTMLLinkElement* owner);

    explicit LinkStyle(HTMLLinkElement* owner);
    ~LinkStyle() override;

    LinkResourceType type() const override { return Style; }
    void process() override;
    void ownerRemoved() override;
    bool hasLoaded() const override { return m_loadedSheet; }
    DECLARE_VIRTUAL_TRACE();

    void startLoadingDynamicSheet();
    void notifyLoadedSheetAndAllCriticalSubresources(Node::LoadedSheetErrorStatus);
    bool sheetLoaded();

    void setDisabledState(bool);
    void setSheetTitle(const String&);

    bool styleSheetIsLoading() const;
    bool hasSheet() const { return m_sheet; }
    bool isDisabled() const { return m_disabledState == Disabled; }
    bool isEnabledViaScript() const { return m_disabledState == EnabledViaScript; }
    bool isUnset() const { return m_disabledState == Unset; }

    CSSStyleSheet* sheet() const { return m_sheet.get(); }

private:
    // From StyleSheetResourceClient
    void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CSSStyleSheetResource*) override;
    String debugName() const override { return "LinkStyle"; }

    enum DisabledState {
        Unset,
        EnabledViaScript,
        Disabled
    };

    enum PendingSheetType {
        None,
        NonBlocking,
        Blocking
    };

    void clearSheet();
    void addPendingSheet(PendingSheetType);
    void removePendingSheet();
    Document& document();

    void setCrossOriginStylesheetStatus(CSSStyleSheet*);
    void setFetchFollowingCORS()
    {
        ASSERT(!m_fetchFollowingCORS);
        m_fetchFollowingCORS = true;
    }
    void clearFetchFollowingCORS()
    {
        m_fetchFollowingCORS = false;
    }

    Member<CSSStyleSheet> m_sheet;
    DisabledState m_disabledState;
    PendingSheetType m_pendingSheetType;
    StyleEngineContext m_styleEngineContext;
    bool m_loading;
    bool m_firedLoad;
    bool m_loadedSheet;
    bool m_fetchFollowingCORS;
};


class CORE_EXPORT HTMLLinkElement final : public HTMLElement, public LinkLoaderClient, private DOMTokenListObserver {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(HTMLLinkElement);
public:
    static HTMLLinkElement* create(Document&, bool createdByParser);
    ~HTMLLinkElement() override;

    KURL href() const;
    const AtomicString& rel() const;
    String media() const { return m_media; }
    String typeValue() const { return m_type; }
    String asValue() const { return m_as; }
    const LinkRelAttribute& relAttribute() const { return m_relAttribute; }
    DOMTokenList& relList() const { return static_cast<DOMTokenList&>(*m_relList); }
    String scope() const { return m_scope; }

    const AtomicString& type() const;

    IconType getIconType() const;

    // the icon sizes as parsed from the HTML attribute
    const Vector<IntSize>& iconSizes() const;

    bool async() const;

    CSSStyleSheet* sheet() const { return linkStyle() ? linkStyle()->sheet() : 0; }
    Document* import() const;

    bool styleSheetIsLoading() const;

    bool isImport() const { return linkImport(); }
    bool isDisabled() const { return linkStyle() && linkStyle()->isDisabled(); }
    bool isEnabledViaScript() const { return linkStyle() && linkStyle()->isEnabledViaScript(); }

    DOMTokenList* sizes() const;

    void dispatchPendingEvent(LinkEventSender*);
    void scheduleEvent();
    void dispatchEventImmediately();
    static void dispatchPendingLoadEvents();

    // From LinkLoaderClient
    bool shouldLoadLink() override;

    // For LinkStyle
    bool loadLink(const String& type, const String& as, const String& media, const KURL&);
    bool isAlternate() const { return linkStyle()->isUnset() && m_relAttribute.isAlternate(); }
    bool shouldProcessStyle() { return linkResourceToProcess() && linkStyle(); }
    bool isCreatedByParser() const { return m_createdByParser; }

    DECLARE_VIRTUAL_TRACE();

    DECLARE_VIRTUAL_TRACE_WRAPPERS();

private:
    HTMLLinkElement(Document&, bool createdByParser);

    LinkStyle* linkStyle() const;
    LinkImport* linkImport() const;
    LinkResource* linkResourceToProcess();

    void process();
    static void processCallback(Node*);

    // From Node and subclassses
    void parseAttribute(const QualifiedName&, const AtomicString&, const AtomicString&) override;
    InsertionNotificationRequest insertedInto(ContainerNode*) override;
    void removedFrom(ContainerNode*) override;
    bool isURLAttribute(const Attribute&) const override;
    bool hasLegalLinkAttribute(const QualifiedName&) const override;
    const QualifiedName& subResourceAttributeName() const override;
    bool sheetLoaded() override;
    void notifyLoadedSheetAndAllCriticalSubresources(LoadedSheetErrorStatus) override;
    void startLoadingDynamicSheet() override;
    void finishParsingChildren() override;

    // From LinkLoaderClient
    void linkLoaded() override;
    void linkLoadingErrored() override;
    void didStartLinkPrerender() override;
    void didStopLinkPrerender() override;
    void didSendLoadForLinkPrerender() override;
    void didSendDOMContentLoadedForLinkPrerender() override;

    // From DOMTokenListObserver
    void valueWasSet() final;

    Member<LinkResource> m_link;
    Member<LinkLoader> m_linkLoader;

    String m_type;
    String m_as;
    String m_media;
    Member<DOMTokenList> m_sizes;
    Vector<IntSize> m_iconSizes;
    Member<RelList> m_relList;
    LinkRelAttribute m_relAttribute;
    String m_scope;

    bool m_createdByParser;
    bool m_isInShadowTree;
};

} // namespace blink

#endif // HTMLLinkElement_h
