/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef HTMLScriptElement_h
#define HTMLScriptElement_h

#include "core/dom/ScriptLoaderClient.h"
#include "core/html/HTMLElement.h"

namespace WebCore {

class ScriptLoader;

class HTMLScriptElement FINAL : public HTMLElement, public ScriptLoaderClient {
public:
    static PassRefPtrWillBeRawPtr<HTMLScriptElement> create(Document&, bool wasInsertedByParser, bool alreadyStarted = false);

    String text() { return textFromChildren(); }
    void setText(const String&);

    KURL src() const;

    void setAsync(bool);
    bool async() const;

    ScriptLoader* loader() const { return m_loader.get(); }

private:
    HTMLScriptElement(Document&, bool wasInsertedByParser, bool alreadyStarted);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void didNotifySubtreeInsertionsToDocument() OVERRIDE;
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;

    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;
    virtual bool hasLegalLinkAttribute(const QualifiedName&) const OVERRIDE;
    virtual const QualifiedName& subResourceAttributeName() const OVERRIDE;

    virtual String sourceAttributeValue() const OVERRIDE;
    virtual String charsetAttributeValue() const OVERRIDE;
    virtual String typeAttributeValue() const OVERRIDE;
    virtual String languageAttributeValue() const OVERRIDE;
    virtual String forAttributeValue() const OVERRIDE;
    virtual String eventAttributeValue() const OVERRIDE;
    virtual bool asyncAttributeValue() const OVERRIDE;
    virtual bool deferAttributeValue() const OVERRIDE;
    virtual bool hasSourceAttribute() const OVERRIDE;

    virtual void dispatchLoadEvent() OVERRIDE;

    virtual PassRefPtrWillBeRawPtr<Element> cloneElementWithoutAttributesAndChildren() OVERRIDE;

    OwnPtr<ScriptLoader> m_loader;
};

} //namespace

#endif
