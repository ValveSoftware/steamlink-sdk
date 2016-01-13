/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLElement_h
#define HTMLElement_h

#include "core/dom/Element.h"

namespace WebCore {

class DocumentFragment;
class HTMLCollection;
class HTMLFormElement;
class ExceptionState;

enum TranslateAttributeMode {
    TranslateAttributeYes,
    TranslateAttributeNo,
    TranslateAttributeInherit
};

class HTMLElement : public Element {
public:
    DECLARE_ELEMENT_FACTORY_WITH_TAGNAME(HTMLElement);

    virtual String title() const OVERRIDE FINAL;
    virtual short tabIndex() const OVERRIDE;

    void setInnerText(const String&, ExceptionState&);
    void setOuterText(const String&, ExceptionState&);

    virtual bool hasCustomFocusLogic() const;

    String contentEditable() const;
    void setContentEditable(const String&, ExceptionState&);

    virtual bool draggable() const;
    void setDraggable(bool);

    bool spellcheck() const;
    void setSpellcheck(bool);

    bool translate() const;
    void setTranslate(bool);

    const AtomicString& dir();
    void setDir(const AtomicString&);

    void click();

    virtual void accessKeyAction(bool sendMouseEvents) OVERRIDE;

    bool ieForbidsInsertHTML() const;

    virtual HTMLFormElement* formOwner() const { return 0; }

    HTMLFormElement* findFormAncestor() const;

    bool hasDirectionAuto() const;
    TextDirection directionalityIfhasDirAutoAttribute(bool& isAuto) const;

    virtual bool isHTMLUnknownElement() const { return false; }
    virtual bool isPluginElement() const { return false; }

    virtual bool isLabelable() const { return false; }
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/elements.html#interactive-content
    virtual bool isInteractiveContent() const;
    virtual void defaultEventHandler(Event*) OVERRIDE;

    static const AtomicString& eventNameForAttributeName(const QualifiedName& attrName);

    virtual bool matchesReadOnlyPseudoClass() const OVERRIDE;
    virtual bool matchesReadWritePseudoClass() const OVERRIDE;

    static const AtomicString& eventParameterName();

protected:
    HTMLElement(const QualifiedName& tagName, Document&, ConstructionType);

    void addHTMLLengthToStyle(MutableStylePropertySet*, CSSPropertyID, const String& value);
    void addHTMLColorToStyle(MutableStylePropertySet*, CSSPropertyID, const String& color);

    void applyAlignmentAttributeToStyle(const AtomicString&, MutableStylePropertySet*);
    void applyBorderAttributeToStyle(const AtomicString&, MutableStylePropertySet*);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;
    unsigned parseBorderWidthAttribute(const AtomicString&) const;

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;
    void calculateAndAdjustDirectionality();

private:
    virtual String nodeName() const OVERRIDE FINAL;

    void mapLanguageAttributeToLocale(const AtomicString&, MutableStylePropertySet*);

    PassRefPtrWillBeRawPtr<DocumentFragment> textToFragment(const String&, ExceptionState&);

    void dirAttributeChanged(const AtomicString&);
    void adjustDirectionalityIfNeededAfterChildAttributeChanged(Element* child);
    void adjustDirectionalityIfNeededAfterChildrenChanged(Node* beforeChange, int childCountDelta);
    TextDirection directionality(Node** strongDirectionalityTextNode= 0) const;

    TranslateAttributeMode translateAttributeMode() const;

    void handleKeypressEvent(KeyboardEvent*);
};

DEFINE_ELEMENT_TYPE_CASTS(HTMLElement, isHTMLElement());

template <> inline bool isElementOfType<const HTMLElement>(const Node& node) { return node.isHTMLElement(); }
template <typename T> bool isElementOfType(const HTMLElement&);
template <> inline bool isElementOfType<const HTMLElement>(const HTMLElement&) { return true; }

inline HTMLElement::HTMLElement(const QualifiedName& tagName, Document& document, ConstructionType type = CreateHTMLElement)
    : Element(tagName, &document, type)
{
    ASSERT(!tagName.localName().isNull());
    ScriptWrappable::init(this);
}

// This requires isHTML*Element(const Element&) and isHTML*Element(const HTMLElement&).
// When the input element is an HTMLElement, we don't need to check the namespace URI, just the local name.
#define DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(thisType) \
    inline bool is##thisType(const thisType* element); \
    inline bool is##thisType(const thisType& element); \
    inline bool is##thisType(const HTMLElement* element) { return element && is##thisType(*element); } \
    inline bool is##thisType(const Element* element) { return element && is##thisType(*element); } \
    inline bool is##thisType(const Node& node) { return node.isElementNode() ? is##thisType(toElement(node)) : false; } \
    inline bool is##thisType(const Node* node) { return node && node->isElementNode() ? is##thisType(*toElement(node)) : false; } \
    template<typename T> inline bool is##thisType(const PassRefPtr<T>& node) { return is##thisType(node.get()); } \
    template<typename T> inline bool is##thisType(const RefPtr<T>& node) { return is##thisType(node.get()); } \
    template <> inline bool isElementOfType<const thisType>(const HTMLElement& element) { return is##thisType(element); } \
    DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(thisType)

} // namespace WebCore

#include "core/HTMLElementTypeHelpers.h"

#endif // HTMLElement_h
