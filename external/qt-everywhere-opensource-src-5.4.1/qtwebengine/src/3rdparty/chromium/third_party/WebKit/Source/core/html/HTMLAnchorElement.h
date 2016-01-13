/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLAnchorElement_h
#define HTMLAnchorElement_h

#include "core/HTMLNames.h"
#include "core/dom/DOMURLUtils.h"
#include "core/dom/Document.h"
#include "core/html/HTMLElement.h"
#include "platform/LinkHash.h"

namespace WebCore {

// Link relation bitmask values.
// FIXME: Uncomment as the various link relations are implemented.
enum {
//     RelationAlternate   = 0x00000001,
//     RelationArchives    = 0x00000002,
//     RelationAuthor      = 0x00000004,
//     RelationBoomark     = 0x00000008,
//     RelationExternal    = 0x00000010,
//     RelationFirst       = 0x00000020,
//     RelationHelp        = 0x00000040,
//     RelationIndex       = 0x00000080,
//     RelationLast        = 0x00000100,
//     RelationLicense     = 0x00000200,
//     RelationNext        = 0x00000400,
//     RelationNoFolow    = 0x00000800,
    RelationNoReferrer     = 0x00001000,
//     RelationPrev        = 0x00002000,
//     RelationSearch      = 0x00004000,
//     RelationSidebar     = 0x00008000,
//     RelationTag         = 0x00010000,
//     RelationUp          = 0x00020000,
};

class HTMLAnchorElement : public HTMLElement, public DOMURLUtils {
public:
    static PassRefPtrWillBeRawPtr<HTMLAnchorElement> create(Document&);

    virtual ~HTMLAnchorElement();

    KURL href() const;
    void setHref(const AtomicString&);

    const AtomicString& name() const;

    virtual KURL url() const OVERRIDE FINAL;
    virtual void setURL(const KURL&) OVERRIDE FINAL;

    virtual String input() const OVERRIDE FINAL;
    virtual void setInput(const String&) OVERRIDE FINAL;

    bool isLiveLink() const;

    virtual bool willRespondToMouseClickEvents() OVERRIDE FINAL;

    bool hasRel(uint32_t relation) const;
    void setRel(const AtomicString&);

    LinkHash visitedLinkHash() const;
    void invalidateCachedVisitedLinkHash() { m_cachedVisitedLinkHash = 0; }

    virtual void trace(Visitor*) OVERRIDE;

protected:
    HTMLAnchorElement(const QualifiedName&, Document&);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool supportsFocus() const OVERRIDE;

private:
    virtual bool isMouseFocusable() const OVERRIDE;
    virtual bool isKeyboardFocusable() const OVERRIDE;
    virtual void defaultEventHandler(Event*) OVERRIDE FINAL;
    virtual void setActive(bool = true) OVERRIDE FINAL;
    virtual void accessKeyAction(bool sendMouseEvents) OVERRIDE FINAL;
    virtual bool isURLAttribute(const Attribute&) const OVERRIDE FINAL;
    virtual bool hasLegalLinkAttribute(const QualifiedName&) const OVERRIDE FINAL;
    virtual bool canStartSelection() const OVERRIDE FINAL;
    virtual short tabIndex() const OVERRIDE FINAL;
    virtual bool draggable() const OVERRIDE FINAL;
    virtual bool isInteractiveContent() const OVERRIDE FINAL;

    void sendPings(const KURL& destinationURL);
    AtomicString target() const;
    void handleClick(Event*);

    class PrefetchEventHandler;
    PrefetchEventHandler* prefetchEventHandler();

    uint32_t m_linkRelations;
    OwnPtrWillBeMember<PrefetchEventHandler> m_prefetchEventHandler;
    mutable LinkHash m_cachedVisitedLinkHash;
};

inline LinkHash HTMLAnchorElement::visitedLinkHash() const
{
    if (!m_cachedVisitedLinkHash)
        m_cachedVisitedLinkHash = WebCore::visitedLinkHash(document().baseURL(), fastGetAttribute(HTMLNames::hrefAttr));
    return m_cachedVisitedLinkHash;
}

// Functions shared with the other anchor elements (i.e., SVG).

bool isEnterKeyKeydownEvent(Event*);
bool isLinkClick(Event*);

} // namespace WebCore

#endif // HTMLAnchorElement_h
