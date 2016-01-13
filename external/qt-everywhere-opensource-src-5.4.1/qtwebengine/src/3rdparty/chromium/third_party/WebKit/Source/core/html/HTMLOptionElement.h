/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef HTMLOptionElement_h
#define HTMLOptionElement_h

#include "core/html/HTMLElement.h"

namespace WebCore {

class ExceptionState;
class HTMLDataListElement;
class HTMLSelectElement;

class HTMLOptionElement FINAL : public HTMLElement {
public:
    static PassRefPtrWillBeRawPtr<HTMLOptionElement> create(Document&);
    static PassRefPtrWillBeRawPtr<HTMLOptionElement> createForJSConstructor(Document&, const String& data, const AtomicString& value,
        bool defaultSelected, bool selected, ExceptionState&);

    String text() const;
    void setText(const String&, ExceptionState&);

    int index() const;

    String value() const;
    void setValue(const AtomicString&);

    bool selected() const;
    void setSelected(bool);

    HTMLDataListElement* ownerDataListElement() const;
    HTMLSelectElement* ownerSelectElement() const;

    String label() const;
    void setLabel(const AtomicString&);

    bool ownElementDisabled() const { return m_disabled; }

    virtual bool isDisabledFormControl() const OVERRIDE;

    String textIndentedToRespectGroupLabel() const;

    void setSelectedState(bool);

    HTMLFormElement* form() const;

    bool isDisplayNone() const;

    virtual void removedFrom(ContainerNode* insertionPoint) OVERRIDE;

private:
    explicit HTMLOptionElement(Document&);

    virtual bool rendererIsFocusable() const OVERRIDE;
    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE { return false; }
    virtual void attach(const AttachContext& = AttachContext()) OVERRIDE;
    virtual void detach(const AttachContext& = AttachContext()) OVERRIDE;

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void accessKeyAction(bool) OVERRIDE;

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;

    // <option> never has a renderer so we manually manage a cached style.
    void updateNonRenderStyle();
    virtual RenderStyle* nonRendererStyle() const OVERRIDE;
    virtual PassRefPtr<RenderStyle> customStyleForRenderer() OVERRIDE;
    virtual void didRecalcStyle(StyleRecalcChange) OVERRIDE;

    String collectOptionInnerText() const;

    bool m_disabled;
    bool m_isSelected;
    RefPtr<RenderStyle> m_style;
};

} // namespace WebCore

#endif
