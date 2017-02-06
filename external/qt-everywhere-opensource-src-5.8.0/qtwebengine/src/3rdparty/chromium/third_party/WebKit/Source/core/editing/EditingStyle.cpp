/*
 * Copyright (C) 2007, 2008, 2009 Apple Computer, Inc.
 * Copyright (C) 2010, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include "core/editing/EditingStyle.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/HTMLNames.h"
#include "core/css/CSSColorValue.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSValueList.h"
#include "core/css/FontSize.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/QualifiedName.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/editing/commands/ApplyStyleCommand.h"
#include "core/editing/serializers/HTMLInterchange.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFontElement.h"
#include "core/html/HTMLSpanElement.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"
#include "wtf/StdLibExtras.h"

namespace blink {

static const CSSPropertyID& textDecorationPropertyForEditing()
{
    static const CSSPropertyID property = RuntimeEnabledFeatures::css3TextDecorationsEnabled() ? CSSPropertyTextDecorationLine : CSSPropertyTextDecoration;
    return property;
}

// Editing style properties must be preserved during editing operation.
// e.g. when a user inserts a new paragraph, all properties listed here must be copied to the new paragraph.
// NOTE: Use either allEditingProperties() or inheritableEditingProperties() to
// respect runtime enabling of properties.
static const CSSPropertyID staticEditingProperties[] = {
    CSSPropertyBackgroundColor,
    CSSPropertyColor,
    CSSPropertyFontFamily,
    CSSPropertyFontSize,
    CSSPropertyFontStyle,
    CSSPropertyFontVariantLigatures,
    CSSPropertyFontVariantCaps,
    CSSPropertyFontWeight,
    CSSPropertyLetterSpacing,
    CSSPropertyOrphans,
    CSSPropertyTextAlign,
    // FIXME: CSSPropertyTextDecoration needs to be removed when CSS3 Text
    // Decoration feature is no longer experimental.
    CSSPropertyTextDecoration,
    CSSPropertyTextDecorationLine,
    CSSPropertyTextIndent,
    CSSPropertyTextTransform,
    CSSPropertyWhiteSpace,
    CSSPropertyWidows,
    CSSPropertyWordSpacing,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyWebkitTextFillColor,
    CSSPropertyWebkitTextStrokeColor,
    CSSPropertyWebkitTextStrokeWidth,
};

enum EditingPropertiesType { OnlyInheritableEditingProperties, AllEditingProperties };

static const Vector<CSSPropertyID>& allEditingProperties()
{
    DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, properties, ());
    if (properties.isEmpty()) {
        CSSPropertyMetadata::filterEnabledCSSPropertiesIntoVector(staticEditingProperties, WTF_ARRAY_LENGTH(staticEditingProperties), properties);
        if (RuntimeEnabledFeatures::css3TextDecorationsEnabled())
            properties.remove(properties.find(CSSPropertyTextDecoration));
    }
    return properties;
}

static const Vector<CSSPropertyID>& inheritableEditingProperties()
{
    DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, properties, ());
    if (properties.isEmpty()) {
        CSSPropertyMetadata::filterEnabledCSSPropertiesIntoVector(staticEditingProperties, WTF_ARRAY_LENGTH(staticEditingProperties), properties);
        for (size_t index = 0; index < properties.size();) {
            if (!CSSPropertyMetadata::isInheritedProperty(properties[index])) {
                properties.remove(index);
                continue;
            }
            ++index;
        }
    }
    return properties;
}

template <class StyleDeclarationType>
static MutableStylePropertySet* copyEditingProperties(StyleDeclarationType* style, EditingPropertiesType type = OnlyInheritableEditingProperties)
{
    if (type == AllEditingProperties)
        return style->copyPropertiesInSet(allEditingProperties());
    return style->copyPropertiesInSet(inheritableEditingProperties());
}

static inline bool isEditingProperty(int id)
{
    return allEditingProperties().contains(static_cast<CSSPropertyID>(id));
}

static MutableStylePropertySet* editingStyleFromComputedStyle(CSSComputedStyleDeclaration* style, EditingPropertiesType type = OnlyInheritableEditingProperties)
{
    if (!style)
        return MutableStylePropertySet::create(HTMLQuirksMode);
    return copyEditingProperties(style, type);
}

static CSSComputedStyleDeclaration* ensureComputedStyle(const Position& position)
{
    Element* elem = associatedElementOf(position);
    if (!elem)
        return nullptr;
    return CSSComputedStyleDeclaration::create(elem);
}

static MutableStylePropertySet* getPropertiesNotIn(StylePropertySet* styleWithRedundantProperties, CSSStyleDeclaration* baseStyle);
enum LegacyFontSizeMode { AlwaysUseLegacyFontSize, UseLegacyFontSizeOnlyIfPixelValuesMatch };
static int legacyFontSizeFromCSSValue(Document*, const CSSPrimitiveValue*, bool, LegacyFontSizeMode);
static bool isTransparentColorValue(const CSSValue*);
static bool hasTransparentBackgroundColor(CSSStyleDeclaration*);
static bool hasTransparentBackgroundColor(StylePropertySet*);
static const CSSValue* backgroundColorValueInEffect(Node*);
static bool hasAncestorVerticalAlignStyle(Node&, CSSValueID);

class HTMLElementEquivalent : public GarbageCollected<HTMLElementEquivalent> {
public:
    static HTMLElementEquivalent* create(CSSPropertyID propertyID, CSSValueID primitiveValue, const HTMLQualifiedName& tagName)
    {
        return new HTMLElementEquivalent(propertyID, primitiveValue, tagName);
    }

    virtual bool matches(const Element* element) const { return !m_tagName || element->hasTagName(*m_tagName); }
    virtual bool hasAttribute() const { return false; }
    virtual bool propertyExistsInStyle(const StylePropertySet* style) const { return style->getPropertyCSSValue(m_propertyID); }
    virtual bool valueIsPresentInStyle(HTMLElement*, StylePropertySet*) const;
    virtual void addToStyle(Element*, EditingStyle*) const;

    DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(m_primitiveValue); }

protected:
    HTMLElementEquivalent(CSSPropertyID);
    HTMLElementEquivalent(CSSPropertyID, const HTMLQualifiedName& tagName);
    HTMLElementEquivalent(CSSPropertyID, CSSValueID primitiveValue, const HTMLQualifiedName& tagName);
    const CSSPropertyID m_propertyID;
    const Member<CSSPrimitiveValue> m_primitiveValue;
    const HTMLQualifiedName* m_tagName; // We can store a pointer because HTML tag names are const global.
};

HTMLElementEquivalent::HTMLElementEquivalent(CSSPropertyID id)
    : m_propertyID(id)
    , m_tagName(0)
{
}

HTMLElementEquivalent::HTMLElementEquivalent(CSSPropertyID id, const HTMLQualifiedName& tagName)
    : m_propertyID(id)
    , m_tagName(&tagName)
{
}

HTMLElementEquivalent::HTMLElementEquivalent(CSSPropertyID id, CSSValueID primitiveValue, const HTMLQualifiedName& tagName)
    : m_propertyID(id)
    , m_primitiveValue(CSSPrimitiveValue::createIdentifier(primitiveValue))
    , m_tagName(&tagName)
{
    DCHECK_NE(primitiveValue, CSSValueInvalid);
}

bool HTMLElementEquivalent::valueIsPresentInStyle(HTMLElement* element, StylePropertySet* style) const
{
    const CSSValue* value = style->getPropertyCSSValue(m_propertyID);
    return matches(element) && value && value->isPrimitiveValue() && toCSSPrimitiveValue(value)->getValueID() == m_primitiveValue->getValueID();
}

void HTMLElementEquivalent::addToStyle(Element*, EditingStyle* style) const
{
    style->setProperty(m_propertyID, m_primitiveValue->cssText());
}

class HTMLTextDecorationEquivalent final : public HTMLElementEquivalent {
public:
    static HTMLElementEquivalent* create(CSSValueID primitiveValue, const HTMLQualifiedName& tagName)
    {
        return new HTMLTextDecorationEquivalent(primitiveValue, tagName);
    }
    bool propertyExistsInStyle(const StylePropertySet*) const override;
    bool valueIsPresentInStyle(HTMLElement*, StylePropertySet*) const override;

    DEFINE_INLINE_VIRTUAL_TRACE() { HTMLElementEquivalent::trace(visitor); }

private:
    HTMLTextDecorationEquivalent(CSSValueID primitiveValue, const HTMLQualifiedName& tagName);
};

HTMLTextDecorationEquivalent::HTMLTextDecorationEquivalent(CSSValueID primitiveValue, const HTMLQualifiedName& tagName)
    : HTMLElementEquivalent(textDecorationPropertyForEditing(), primitiveValue, tagName)
    // m_propertyID is used in HTMLElementEquivalent::addToStyle
{
}

bool HTMLTextDecorationEquivalent::propertyExistsInStyle(const StylePropertySet* style) const
{
    return style->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect)
        || style->getPropertyCSSValue(textDecorationPropertyForEditing());
}

bool HTMLTextDecorationEquivalent::valueIsPresentInStyle(HTMLElement* element, StylePropertySet* style) const
{
    const CSSValue* styleValue = style->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
    if (!styleValue)
        styleValue = style->getPropertyCSSValue(textDecorationPropertyForEditing());
    return matches(element) && styleValue && styleValue->isValueList() && toCSSValueList(styleValue)->hasValue(*m_primitiveValue);
}

class HTMLAttributeEquivalent : public HTMLElementEquivalent {
public:
    static HTMLAttributeEquivalent* create(CSSPropertyID propertyID, const HTMLQualifiedName& tagName, const QualifiedName& attrName)
    {
        return new HTMLAttributeEquivalent(propertyID, tagName, attrName);
    }
    static HTMLAttributeEquivalent* create(CSSPropertyID propertyID, const QualifiedName& attrName)
    {
        return new HTMLAttributeEquivalent(propertyID, attrName);
    }

    bool matches(const Element* element) const override { return HTMLElementEquivalent::matches(element) && element->hasAttribute(m_attrName); }
    bool hasAttribute() const override { return true; }
    bool valueIsPresentInStyle(HTMLElement*, StylePropertySet*) const override;
    void addToStyle(Element*, EditingStyle*) const override;
    virtual const CSSValue* attributeValueAsCSSValue(Element*) const;
    inline const QualifiedName& attributeName() const { return m_attrName; }

    DEFINE_INLINE_VIRTUAL_TRACE() { HTMLElementEquivalent::trace(visitor); }

protected:
    HTMLAttributeEquivalent(CSSPropertyID, const HTMLQualifiedName& tagName, const QualifiedName& attrName);
    HTMLAttributeEquivalent(CSSPropertyID, const QualifiedName& attrName);
    const QualifiedName& m_attrName; // We can store a reference because HTML attribute names are const global.
};

HTMLAttributeEquivalent::HTMLAttributeEquivalent(CSSPropertyID id, const HTMLQualifiedName& tagName, const QualifiedName& attrName)
    : HTMLElementEquivalent(id, tagName)
    , m_attrName(attrName)
{
}

HTMLAttributeEquivalent::HTMLAttributeEquivalent(CSSPropertyID id, const QualifiedName& attrName)
    : HTMLElementEquivalent(id)
    , m_attrName(attrName)
{
}

bool HTMLAttributeEquivalent::valueIsPresentInStyle(HTMLElement* element, StylePropertySet* style) const
{
    const CSSValue* value = attributeValueAsCSSValue(element);
    const CSSValue* styleValue = style->getPropertyCSSValue(m_propertyID);

    return compareCSSValuePtr(value, styleValue);
}

void HTMLAttributeEquivalent::addToStyle(Element* element, EditingStyle* style) const
{
    if (const CSSValue* value = attributeValueAsCSSValue(element))
        style->setProperty(m_propertyID, value->cssText());
}

const CSSValue* HTMLAttributeEquivalent::attributeValueAsCSSValue(Element* element) const
{
    DCHECK(element);
    const AtomicString& value = element->getAttribute(m_attrName);
    if (value.isNull())
        return nullptr;

    MutableStylePropertySet* dummyStyle = nullptr;
    dummyStyle = MutableStylePropertySet::create(HTMLQuirksMode);
    dummyStyle->setProperty(m_propertyID, value);
    return dummyStyle->getPropertyCSSValue(m_propertyID);
}

class HTMLFontSizeEquivalent final : public HTMLAttributeEquivalent {
public:
    static HTMLFontSizeEquivalent* create()
    {
        return new HTMLFontSizeEquivalent();
    }
    const CSSValue* attributeValueAsCSSValue(Element*) const override;

    DEFINE_INLINE_VIRTUAL_TRACE() { HTMLAttributeEquivalent::trace(visitor); }

private:
    HTMLFontSizeEquivalent();
};

HTMLFontSizeEquivalent::HTMLFontSizeEquivalent()
    : HTMLAttributeEquivalent(CSSPropertyFontSize, HTMLNames::fontTag, HTMLNames::sizeAttr)
{
}

const CSSValue* HTMLFontSizeEquivalent::attributeValueAsCSSValue(Element* element) const
{
    DCHECK(element);
    const AtomicString& value = element->getAttribute(m_attrName);
    if (value.isNull())
        return nullptr;
    CSSValueID size;
    if (!HTMLFontElement::cssValueFromFontSizeNumber(value, size))
        return nullptr;
    return CSSPrimitiveValue::createIdentifier(size);
}

float EditingStyle::NoFontDelta = 0.0f;

EditingStyle::EditingStyle(ContainerNode* node, PropertiesToInclude propertiesToInclude)
{
    init(node, propertiesToInclude);
}

EditingStyle::EditingStyle(const Position& position, PropertiesToInclude propertiesToInclude)
{
    init(position.anchorNode(), propertiesToInclude);
}

EditingStyle::EditingStyle(const StylePropertySet* style)
    : m_mutableStyle(style ? style->mutableCopy() : nullptr)
{
    extractFontSizeDelta();
}

EditingStyle::EditingStyle(CSSPropertyID propertyID, const String& value)
    : m_mutableStyle(nullptr)
{
    setProperty(propertyID, value);
    m_isVerticalAlign = propertyID == CSSPropertyVerticalAlign && (value == "sub" || value == "super");
}

static Color cssValueToColor(const CSSValue* colorValue)
{
    if (!colorValue || (!colorValue->isColorValue() && !colorValue->isPrimitiveValue()))
        return Color::transparent;

    if (colorValue->isColorValue())
        return toCSSColorValue(colorValue)->value();

    Color color = 0;
    // FIXME: Why ignore the return value?
    CSSParser::parseColor(color, colorValue->cssText());
    return color;
}

static inline Color getFontColor(CSSStyleDeclaration* style)
{
    return cssValueToColor(style->getPropertyCSSValueInternal(CSSPropertyColor));
}

static inline Color getFontColor(StylePropertySet* style)
{
    return cssValueToColor(style->getPropertyCSSValue(CSSPropertyColor));
}

static inline Color getBackgroundColor(CSSStyleDeclaration* style)
{
    return cssValueToColor(style->getPropertyCSSValueInternal(CSSPropertyBackgroundColor));
}

static inline Color getBackgroundColor(StylePropertySet* style)
{
    return cssValueToColor(style->getPropertyCSSValue(CSSPropertyBackgroundColor));
}

static inline Color backgroundColorInEffect(Node* node)
{
    return cssValueToColor(backgroundColorValueInEffect(node));
}

static int textAlignResolvingStartAndEnd(int textAlign, int direction)
{
    switch (textAlign) {
    case CSSValueCenter:
    case CSSValueWebkitCenter:
        return CSSValueCenter;
    case CSSValueJustify:
        return CSSValueJustify;
    case CSSValueLeft:
    case CSSValueWebkitLeft:
        return CSSValueLeft;
    case CSSValueRight:
    case CSSValueWebkitRight:
        return CSSValueRight;
    case CSSValueStart:
        return direction != CSSValueRtl ? CSSValueLeft : CSSValueRight;
    case CSSValueEnd:
        return direction == CSSValueRtl ? CSSValueRight : CSSValueLeft;
    }
    return CSSValueInvalid;
}

template<typename T>
static int textAlignResolvingStartAndEnd(T* style)
{
    return textAlignResolvingStartAndEnd(getIdentifierValue(style, CSSPropertyTextAlign), getIdentifierValue(style, CSSPropertyDirection));
}

void EditingStyle::init(Node* node, PropertiesToInclude propertiesToInclude)
{
    if (isTabHTMLSpanElementTextNode(node))
        node = tabSpanElement(node)->parentNode();
    else if (isTabHTMLSpanElement(node))
        node = node->parentNode();

    CSSComputedStyleDeclaration* computedStyleAtPosition = CSSComputedStyleDeclaration::create(node);
    m_mutableStyle = propertiesToInclude == AllProperties && computedStyleAtPosition ? computedStyleAtPosition->copyProperties() : editingStyleFromComputedStyle(computedStyleAtPosition);

    if (propertiesToInclude == EditingPropertiesInEffect) {
        if (const CSSValue* value = backgroundColorValueInEffect(node))
            m_mutableStyle->setProperty(CSSPropertyBackgroundColor, value->cssText());
        if (const CSSValue* value = computedStyleAtPosition->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect))
            m_mutableStyle->setProperty(CSSPropertyTextDecoration, value->cssText());
    }

    if (node && node->ensureComputedStyle()) {
        const ComputedStyle* computedStyle = node->ensureComputedStyle();
        removeTextFillAndStrokeColorsIfNeeded(computedStyle);
        replaceFontSizeByKeywordIfPossible(computedStyle, computedStyleAtPosition);
    }

    m_isMonospaceFont = computedStyleAtPosition->isMonospaceFont();
    extractFontSizeDelta();
}

void EditingStyle::removeTextFillAndStrokeColorsIfNeeded(const ComputedStyle* computedStyle)
{
    // If a node's text fill color is currentColor, then its children use
    // their font-color as their text fill color (they don't
    // inherit it).  Likewise for stroke color.
    if (computedStyle->textFillColor().isCurrentColor())
        m_mutableStyle->removeProperty(CSSPropertyWebkitTextFillColor);
    if (computedStyle->textStrokeColor().isCurrentColor())
        m_mutableStyle->removeProperty(CSSPropertyWebkitTextStrokeColor);
}

void EditingStyle::setProperty(CSSPropertyID propertyID, const String& value, bool important)
{
    if (!m_mutableStyle)
        m_mutableStyle = MutableStylePropertySet::create(HTMLQuirksMode);

    m_mutableStyle->setProperty(propertyID, value, important);
}

void EditingStyle::replaceFontSizeByKeywordIfPossible(const ComputedStyle* computedStyle, CSSComputedStyleDeclaration* cssComputedStyle)
{
    DCHECK(computedStyle);
    if (computedStyle->getFontDescription().keywordSize())
        m_mutableStyle->setProperty(CSSPropertyFontSize, cssComputedStyle->getFontSizeCSSValuePreferringKeyword()->cssText());
}

void EditingStyle::extractFontSizeDelta()
{
    if (!m_mutableStyle)
        return;

    if (m_mutableStyle->getPropertyCSSValue(CSSPropertyFontSize)) {
        // Explicit font size overrides any delta.
        m_mutableStyle->removeProperty(CSSPropertyWebkitFontSizeDelta);
        return;
    }

    // Get the adjustment amount out of the style.
    const CSSValue* value = m_mutableStyle->getPropertyCSSValue(CSSPropertyWebkitFontSizeDelta);
    if (!value || !value->isPrimitiveValue())
        return;

    const CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    // Only PX handled now. If we handle more types in the future, perhaps
    // a switch statement here would be more appropriate.
    if (!primitiveValue->isPx())
        return;

    m_fontSizeDelta = primitiveValue->getFloatValue();
    m_mutableStyle->removeProperty(CSSPropertyWebkitFontSizeDelta);
}

bool EditingStyle::isEmpty() const
{
    return (!m_mutableStyle || m_mutableStyle->isEmpty()) && m_fontSizeDelta == NoFontDelta;
}

bool EditingStyle::textDirection(WritingDirection& writingDirection) const
{
    if (!m_mutableStyle)
        return false;

    const CSSValue* unicodeBidi = m_mutableStyle->getPropertyCSSValue(CSSPropertyUnicodeBidi);
    if (!unicodeBidi || !unicodeBidi->isPrimitiveValue())
        return false;

    CSSValueID unicodeBidiValue = toCSSPrimitiveValue(unicodeBidi)->getValueID();
    if (isEmbedOrIsolate(unicodeBidiValue)) {
        const CSSValue* direction = m_mutableStyle->getPropertyCSSValue(CSSPropertyDirection);
        if (!direction || !direction->isPrimitiveValue())
            return false;

        writingDirection = toCSSPrimitiveValue(direction)->getValueID() == CSSValueLtr ? LeftToRightWritingDirection : RightToLeftWritingDirection;

        return true;
    }

    if (unicodeBidiValue == CSSValueNormal) {
        writingDirection = NaturalWritingDirection;
        return true;
    }

    return false;
}

void EditingStyle::overrideWithStyle(const StylePropertySet* style)
{
    if (!style || style->isEmpty())
        return;
    if (!m_mutableStyle)
        m_mutableStyle = MutableStylePropertySet::create(HTMLQuirksMode);
    m_mutableStyle->mergeAndOverrideOnConflict(style);
    extractFontSizeDelta();
}

void EditingStyle::clear()
{
    m_mutableStyle.clear();
    m_isMonospaceFont = false;
    m_fontSizeDelta = NoFontDelta;
}

EditingStyle* EditingStyle::copy() const
{
    EditingStyle* copy = EditingStyle::create();
    if (m_mutableStyle)
        copy->m_mutableStyle = m_mutableStyle->mutableCopy();
    copy->m_isMonospaceFont = m_isMonospaceFont;
    copy->m_fontSizeDelta = m_fontSizeDelta;
    return copy;
}

// This is the list of CSS properties that apply specially to block-level elements.
static const CSSPropertyID staticBlockProperties[] = {
    CSSPropertyBreakAfter,
    CSSPropertyBreakBefore,
    CSSPropertyBreakInside,
    CSSPropertyOrphans,
    CSSPropertyOverflow, // This can be also be applied to replaced elements
    CSSPropertyColumnCount,
    CSSPropertyColumnGap,
    CSSPropertyColumnRuleColor,
    CSSPropertyColumnRuleStyle,
    CSSPropertyColumnRuleWidth,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakInside,
    CSSPropertyColumnWidth,
    CSSPropertyPageBreakAfter,
    CSSPropertyPageBreakBefore,
    CSSPropertyPageBreakInside,
    CSSPropertyTextAlign,
    CSSPropertyTextAlignLast,
    CSSPropertyTextIndent,
    CSSPropertyTextJustify,
    CSSPropertyWidows
};

static const Vector<CSSPropertyID>& blockPropertiesVector()
{
    DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, properties, ());
    if (properties.isEmpty())
        CSSPropertyMetadata::filterEnabledCSSPropertiesIntoVector(staticBlockProperties, WTF_ARRAY_LENGTH(staticBlockProperties), properties);
    return properties;
}

EditingStyle* EditingStyle::extractAndRemoveBlockProperties()
{
    EditingStyle* blockProperties = EditingStyle::create();
    if (!m_mutableStyle)
        return blockProperties;

    blockProperties->m_mutableStyle = m_mutableStyle->copyPropertiesInSet(blockPropertiesVector());
    removeBlockProperties();

    return blockProperties;
}

EditingStyle* EditingStyle::extractAndRemoveTextDirection()
{
    EditingStyle* textDirection = EditingStyle::create();
    textDirection->m_mutableStyle = MutableStylePropertySet::create(HTMLQuirksMode);
    textDirection->m_mutableStyle->setProperty(CSSPropertyUnicodeBidi, CSSValueIsolate, m_mutableStyle->propertyIsImportant(CSSPropertyUnicodeBidi));
    textDirection->m_mutableStyle->setProperty(CSSPropertyDirection, m_mutableStyle->getPropertyValue(CSSPropertyDirection),
        m_mutableStyle->propertyIsImportant(CSSPropertyDirection));

    m_mutableStyle->removeProperty(CSSPropertyUnicodeBidi);
    m_mutableStyle->removeProperty(CSSPropertyDirection);

    return textDirection;
}

void EditingStyle::removeBlockProperties()
{
    if (!m_mutableStyle)
        return;

    m_mutableStyle->removePropertiesInSet(blockPropertiesVector().data(), blockPropertiesVector().size());
}

void EditingStyle::removeStyleAddedByElement(Element* element)
{
    if (!element || !element->parentNode())
        return;
    MutableStylePropertySet* parentStyle = editingStyleFromComputedStyle(CSSComputedStyleDeclaration::create(element->parentNode()), AllEditingProperties);
    MutableStylePropertySet* nodeStyle = editingStyleFromComputedStyle(CSSComputedStyleDeclaration::create(element), AllEditingProperties);
    nodeStyle->removeEquivalentProperties(parentStyle);
    m_mutableStyle->removeEquivalentProperties(nodeStyle);
}

void EditingStyle::removeStyleConflictingWithStyleOfElement(Element* element)
{
    if (!element || !element->parentNode() || !m_mutableStyle)
        return;

    MutableStylePropertySet* parentStyle = editingStyleFromComputedStyle(CSSComputedStyleDeclaration::create(element->parentNode()), AllEditingProperties);
    MutableStylePropertySet* nodeStyle = editingStyleFromComputedStyle(CSSComputedStyleDeclaration::create(element), AllEditingProperties);
    nodeStyle->removeEquivalentProperties(parentStyle);

    unsigned propertyCount = nodeStyle->propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i)
        m_mutableStyle->removeProperty(nodeStyle->propertyAt(i).id());
}

void EditingStyle::collapseTextDecorationProperties()
{
    if (!m_mutableStyle)
        return;

    const CSSValue* textDecorationsInEffect = m_mutableStyle->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
    if (!textDecorationsInEffect)
        return;

    if (textDecorationsInEffect->isValueList())
        m_mutableStyle->setProperty(textDecorationPropertyForEditing(), textDecorationsInEffect->cssText(), m_mutableStyle->propertyIsImportant(textDecorationPropertyForEditing()));
    else
        m_mutableStyle->removeProperty(textDecorationPropertyForEditing());
    m_mutableStyle->removeProperty(CSSPropertyWebkitTextDecorationsInEffect);
}

// CSS properties that create a visual difference only when applied to text.
static const CSSPropertyID textOnlyProperties[] = {
    // FIXME: CSSPropertyTextDecoration needs to be removed when CSS3 Text
    // Decoration feature is no longer experimental.
    CSSPropertyTextDecoration,
    CSSPropertyTextDecorationLine,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyFontStyle,
    CSSPropertyFontWeight,
    CSSPropertyColor,
};

TriState EditingStyle::triStateOfStyle(EditingStyle* style) const
{
    if (!style || !style->m_mutableStyle)
        return FalseTriState;
    return triStateOfStyle(style->m_mutableStyle->ensureCSSStyleDeclaration(), DoNotIgnoreTextOnlyProperties);
}

TriState EditingStyle::triStateOfStyle(CSSStyleDeclaration* styleToCompare, ShouldIgnoreTextOnlyProperties shouldIgnoreTextOnlyProperties) const
{
    MutableStylePropertySet* difference = getPropertiesNotIn(m_mutableStyle.get(), styleToCompare);

    if (shouldIgnoreTextOnlyProperties == IgnoreTextOnlyProperties)
        difference->removePropertiesInSet(textOnlyProperties, WTF_ARRAY_LENGTH(textOnlyProperties));

    if (difference->isEmpty())
        return TrueTriState;
    if (difference->propertyCount() == m_mutableStyle->propertyCount())
        return FalseTriState;

    return MixedTriState;
}

static bool hasAncestorVerticalAlignStyle(Node& node, CSSValueID value)
{
    for (Node& runner : NodeTraversal::inclusiveAncestorsOf(node)) {
        CSSComputedStyleDeclaration* ancestorStyle = CSSComputedStyleDeclaration::create(&runner);
        if (getIdentifierValue(ancestorStyle, CSSPropertyVerticalAlign) == value)
            return true;
    }
    return false;
}

TriState EditingStyle::triStateOfStyle(const VisibleSelection& selection) const
{
    if (selection.isNone())
        return FalseTriState;

    if (selection.isCaret())
        return triStateOfStyle(EditingStyle::styleAtSelectionStart(selection));

    TriState state = FalseTriState;
    bool nodeIsStart = true;
    for (Node& node : NodeTraversal::startsAt(*selection.start().anchorNode())) {
        if (node.layoutObject() && node.hasEditableStyle()) {
            CSSComputedStyleDeclaration* nodeStyle = CSSComputedStyleDeclaration::create(&node);
            if (nodeStyle) {
                // If the selected element has <sub> or <sup> ancestor element, apply the corresponding
                // style(vertical-align) to it so that document.queryCommandState() works with the style.
                // See bug http://crbug.com/582225.
                if (m_isVerticalAlign && getIdentifierValue(nodeStyle, CSSPropertyVerticalAlign) == CSSValueBaseline) {
                    const CSSPrimitiveValue* verticalAlign = toCSSPrimitiveValue(m_mutableStyle->getPropertyCSSValue(CSSPropertyVerticalAlign));
                    if (hasAncestorVerticalAlignStyle(node, verticalAlign->getValueID()))
                        node.mutableComputedStyle()->setVerticalAlign(verticalAlign->convertTo<EVerticalAlign>());
                }

                // Pass EditingStyle::DoNotIgnoreTextOnlyProperties without checking if node.isTextNode()
                // because the node can be an element node. See bug http://crbug.com/584939.
                TriState nodeState = triStateOfStyle(nodeStyle, EditingStyle::DoNotIgnoreTextOnlyProperties);
                if (nodeIsStart) {
                    state = nodeState;
                    nodeIsStart = false;
                } else if (state != nodeState && node.isTextNode()) {
                    state = MixedTriState;
                    break;
                }
            }
        }
        if (&node == selection.end().anchorNode())
            break;
    }

    return state;
}

bool EditingStyle::conflictsWithInlineStyleOfElement(HTMLElement* element, EditingStyle* extractedStyle, Vector<CSSPropertyID>* conflictingProperties) const
{
    DCHECK(element);
    DCHECK(!conflictingProperties || conflictingProperties->isEmpty());

    const StylePropertySet* inlineStyle = element->inlineStyle();
    if (!m_mutableStyle || !inlineStyle)
        return false;

    unsigned propertyCount = m_mutableStyle->propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        CSSPropertyID propertyID = m_mutableStyle->propertyAt(i).id();

        // We don't override whitespace property of a tab span because that would collapse the tab into a space.
        if (propertyID == CSSPropertyWhiteSpace && isTabHTMLSpanElement(element))
            continue;

        if (propertyID == CSSPropertyWebkitTextDecorationsInEffect && inlineStyle->getPropertyCSSValue(textDecorationPropertyForEditing())) {
            if (!conflictingProperties)
                return true;
            conflictingProperties->append(CSSPropertyTextDecoration);
            // Because text-decoration expands to text-decoration-line when CSS3
            // Text Decoration is enabled, we also state it as conflicting.
            if (RuntimeEnabledFeatures::css3TextDecorationsEnabled())
                conflictingProperties->append(CSSPropertyTextDecorationLine);
            if (extractedStyle)
                extractedStyle->setProperty(textDecorationPropertyForEditing(), inlineStyle->getPropertyValue(textDecorationPropertyForEditing()), inlineStyle->propertyIsImportant(textDecorationPropertyForEditing()));
            continue;
        }

        if (!inlineStyle->getPropertyCSSValue(propertyID))
            continue;

        if (propertyID == CSSPropertyUnicodeBidi && inlineStyle->getPropertyCSSValue(CSSPropertyDirection)) {
            if (!conflictingProperties)
                return true;
            conflictingProperties->append(CSSPropertyDirection);
            if (extractedStyle)
                extractedStyle->setProperty(propertyID, inlineStyle->getPropertyValue(propertyID), inlineStyle->propertyIsImportant(propertyID));
        }

        if (!conflictingProperties)
            return true;

        conflictingProperties->append(propertyID);

        if (extractedStyle)
            extractedStyle->setProperty(propertyID, inlineStyle->getPropertyValue(propertyID), inlineStyle->propertyIsImportant(propertyID));
    }

    return conflictingProperties && !conflictingProperties->isEmpty();
}

static const HeapVector<Member<HTMLElementEquivalent>>& htmlElementEquivalents()
{
    DEFINE_STATIC_LOCAL(HeapVector<Member<HTMLElementEquivalent>>, HTMLElementEquivalents, (new HeapVector<Member<HTMLElementEquivalent>>));
    if (!HTMLElementEquivalents.size()) {
        HTMLElementEquivalents.append(HTMLElementEquivalent::create(CSSPropertyFontWeight, CSSValueBold, HTMLNames::bTag));
        HTMLElementEquivalents.append(HTMLElementEquivalent::create(CSSPropertyFontWeight, CSSValueBold, HTMLNames::strongTag));
        HTMLElementEquivalents.append(HTMLElementEquivalent::create(CSSPropertyVerticalAlign, CSSValueSub, HTMLNames::subTag));
        HTMLElementEquivalents.append(HTMLElementEquivalent::create(CSSPropertyVerticalAlign, CSSValueSuper, HTMLNames::supTag));
        HTMLElementEquivalents.append(HTMLElementEquivalent::create(CSSPropertyFontStyle, CSSValueItalic, HTMLNames::iTag));
        HTMLElementEquivalents.append(HTMLElementEquivalent::create(CSSPropertyFontStyle, CSSValueItalic, HTMLNames::emTag));

        HTMLElementEquivalents.append(HTMLTextDecorationEquivalent::create(CSSValueUnderline, HTMLNames::uTag));
        HTMLElementEquivalents.append(HTMLTextDecorationEquivalent::create(CSSValueLineThrough, HTMLNames::sTag));
        HTMLElementEquivalents.append(HTMLTextDecorationEquivalent::create(CSSValueLineThrough, HTMLNames::strikeTag));
    }

    return HTMLElementEquivalents;
}


bool EditingStyle::conflictsWithImplicitStyleOfElement(HTMLElement* element, EditingStyle* extractedStyle, ShouldExtractMatchingStyle shouldExtractMatchingStyle) const
{
    if (!m_mutableStyle)
        return false;

    const HeapVector<Member<HTMLElementEquivalent>>& HTMLElementEquivalents = htmlElementEquivalents();
    for (size_t i = 0; i < HTMLElementEquivalents.size(); ++i) {
        const HTMLElementEquivalent* equivalent = HTMLElementEquivalents[i].get();
        if (equivalent->matches(element) && equivalent->propertyExistsInStyle(m_mutableStyle.get())
            && (shouldExtractMatchingStyle == ExtractMatchingStyle || !equivalent->valueIsPresentInStyle(element, m_mutableStyle.get()))) {
            if (extractedStyle)
                equivalent->addToStyle(element, extractedStyle);
            return true;
        }
    }
    return false;
}

static const HeapVector<Member<HTMLAttributeEquivalent>>& htmlAttributeEquivalents()
{
    DEFINE_STATIC_LOCAL(HeapVector<Member<HTMLAttributeEquivalent>>, HTMLAttributeEquivalents, (new HeapVector<Member<HTMLAttributeEquivalent>>));
    if (!HTMLAttributeEquivalents.size()) {
        // elementIsStyledSpanOrHTMLEquivalent depends on the fact each HTMLAttriuteEquivalent matches exactly one attribute
        // of exactly one element except dirAttr.
        HTMLAttributeEquivalents.append(HTMLAttributeEquivalent::create(CSSPropertyColor, HTMLNames::fontTag, HTMLNames::colorAttr));
        HTMLAttributeEquivalents.append(HTMLAttributeEquivalent::create(CSSPropertyFontFamily, HTMLNames::fontTag, HTMLNames::faceAttr));
        HTMLAttributeEquivalents.append(HTMLFontSizeEquivalent::create());

        HTMLAttributeEquivalents.append(HTMLAttributeEquivalent::create(CSSPropertyDirection, HTMLNames::dirAttr));
        HTMLAttributeEquivalents.append(HTMLAttributeEquivalent::create(CSSPropertyUnicodeBidi, HTMLNames::dirAttr));
    }

    return HTMLAttributeEquivalents;
}

bool EditingStyle::conflictsWithImplicitStyleOfAttributes(HTMLElement* element) const
{
    DCHECK(element);
    if (!m_mutableStyle)
        return false;

    const HeapVector<Member<HTMLAttributeEquivalent>>& HTMLAttributeEquivalents = htmlAttributeEquivalents();
    for (const auto& equivalent : HTMLAttributeEquivalents) {
        if (equivalent->matches(element) && equivalent->propertyExistsInStyle(m_mutableStyle.get())
            && !equivalent->valueIsPresentInStyle(element, m_mutableStyle.get()))
            return true;
    }

    return false;
}

bool EditingStyle::extractConflictingImplicitStyleOfAttributes(HTMLElement* element, ShouldPreserveWritingDirection shouldPreserveWritingDirection,
    EditingStyle* extractedStyle, Vector<QualifiedName>& conflictingAttributes, ShouldExtractMatchingStyle shouldExtractMatchingStyle) const
{
    DCHECK(element);
    // HTMLAttributeEquivalent::addToStyle doesn't support unicode-bidi and direction properties
    if (extractedStyle)
        DCHECK_EQ(shouldPreserveWritingDirection, PreserveWritingDirection);
    if (!m_mutableStyle)
        return false;

    const HeapVector<Member<HTMLAttributeEquivalent>>& HTMLAttributeEquivalents = htmlAttributeEquivalents();
    bool removed = false;
    for (const auto& attribute : HTMLAttributeEquivalents) {
        const HTMLAttributeEquivalent* equivalent = attribute.get();

        // unicode-bidi and direction are pushed down separately so don't push down with other styles.
        if (shouldPreserveWritingDirection == PreserveWritingDirection && equivalent->attributeName() == HTMLNames::dirAttr)
            continue;

        if (!equivalent->matches(element) || !equivalent->propertyExistsInStyle(m_mutableStyle.get())
            || (shouldExtractMatchingStyle == DoNotExtractMatchingStyle && equivalent->valueIsPresentInStyle(element, m_mutableStyle.get())))
            continue;

        if (extractedStyle)
            equivalent->addToStyle(element, extractedStyle);
        conflictingAttributes.append(equivalent->attributeName());
        removed = true;
    }

    return removed;
}

bool EditingStyle::styleIsPresentInComputedStyleOfNode(Node* node) const
{
    return !m_mutableStyle || getPropertiesNotIn(m_mutableStyle.get(), CSSComputedStyleDeclaration::create(node))->isEmpty();
}

bool EditingStyle::elementIsStyledSpanOrHTMLEquivalent(const HTMLElement* element)
{
    DCHECK(element);
    bool elementIsSpanOrElementEquivalent = false;
    if (isHTMLSpanElement(*element)) {
        elementIsSpanOrElementEquivalent = true;
    } else {
        const HeapVector<Member<HTMLElementEquivalent>>& HTMLElementEquivalents = htmlElementEquivalents();
        size_t i;
        for (i = 0; i < HTMLElementEquivalents.size(); ++i) {
            if (HTMLElementEquivalents[i]->matches(element)) {
                elementIsSpanOrElementEquivalent = true;
                break;
            }
        }
    }

    AttributeCollection attributes = element->attributes();
    if (attributes.isEmpty())
        return elementIsSpanOrElementEquivalent; // span, b, etc... without any attributes

    unsigned matchedAttributes = 0;
    const HeapVector<Member<HTMLAttributeEquivalent>>& HTMLAttributeEquivalents = htmlAttributeEquivalents();
    for (const auto& equivalent : HTMLAttributeEquivalents) {
        if (equivalent->matches(element) && equivalent->attributeName() != HTMLNames::dirAttr)
            matchedAttributes++;
    }

    if (!elementIsSpanOrElementEquivalent && !matchedAttributes)
        return false; // element is not a span, a html element equivalent, or font element.

    if (element->getAttribute(HTMLNames::classAttr) == AppleStyleSpanClass)
        matchedAttributes++;

    if (element->hasAttribute(HTMLNames::styleAttr)) {
        if (const StylePropertySet* style = element->inlineStyle()) {
            unsigned propertyCount = style->propertyCount();
            for (unsigned i = 0; i < propertyCount; ++i) {
                if (!isEditingProperty(style->propertyAt(i).id()))
                    return false;
            }
        }
        matchedAttributes++;
    }

    // font with color attribute, span with style attribute, etc...
    DCHECK_LE(matchedAttributes, attributes.size());
    return matchedAttributes >= attributes.size();
}

void EditingStyle::prepareToApplyAt(const Position& position, ShouldPreserveWritingDirection shouldPreserveWritingDirection)
{
    if (!m_mutableStyle)
        return;

    // ReplaceSelectionCommand::handleStyleSpans() requires that this function only removes the editing style.
    // If this function was modified in the future to delete all redundant properties, then add a boolean value to indicate
    // which one of editingStyleAtPosition or computedStyle is called.
    EditingStyle* editingStyleAtPosition = EditingStyle::create(position, EditingPropertiesInEffect);
    StylePropertySet* styleAtPosition = editingStyleAtPosition->m_mutableStyle.get();

    const CSSValue* unicodeBidi = nullptr;
    const CSSValue* direction = nullptr;
    if (shouldPreserveWritingDirection == PreserveWritingDirection) {
        unicodeBidi = m_mutableStyle->getPropertyCSSValue(CSSPropertyUnicodeBidi);
        direction = m_mutableStyle->getPropertyCSSValue(CSSPropertyDirection);
    }

    m_mutableStyle->removeEquivalentProperties(styleAtPosition);

    if (textAlignResolvingStartAndEnd(m_mutableStyle.get()) == textAlignResolvingStartAndEnd(styleAtPosition))
        m_mutableStyle->removeProperty(CSSPropertyTextAlign);

    if (getFontColor(m_mutableStyle.get()) == getFontColor(styleAtPosition))
        m_mutableStyle->removeProperty(CSSPropertyColor);

    if (hasTransparentBackgroundColor(m_mutableStyle.get())
        || cssValueToColor(m_mutableStyle->getPropertyCSSValue(CSSPropertyBackgroundColor)) == backgroundColorInEffect(position.computeContainerNode()))
        m_mutableStyle->removeProperty(CSSPropertyBackgroundColor);

    if (unicodeBidi && unicodeBidi->isPrimitiveValue()) {
        m_mutableStyle->setProperty(CSSPropertyUnicodeBidi, toCSSPrimitiveValue(unicodeBidi)->getValueID());
        if (direction && direction->isPrimitiveValue())
            m_mutableStyle->setProperty(CSSPropertyDirection, toCSSPrimitiveValue(direction)->getValueID());
    }
}

void EditingStyle::mergeTypingStyle(Document* document)
{
    DCHECK(document);

    EditingStyle* typingStyle = document->frame()->selection().typingStyle();
    if (!typingStyle || typingStyle == this)
        return;

    mergeStyle(typingStyle->style(), OverrideValues);
}

void EditingStyle::mergeInlineStyleOfElement(HTMLElement* element, CSSPropertyOverrideMode mode, PropertiesToInclude propertiesToInclude)
{
    DCHECK(element);
    if (!element->inlineStyle())
        return;

    switch (propertiesToInclude) {
    case AllProperties:
        mergeStyle(element->inlineStyle(), mode);
        return;
    case OnlyEditingInheritableProperties:
        mergeStyle(copyEditingProperties(element->inlineStyle(), OnlyInheritableEditingProperties), mode);
        return;
    case EditingPropertiesInEffect:
        mergeStyle(copyEditingProperties(element->inlineStyle(), AllEditingProperties), mode);
        return;
    }
}

static inline bool elementMatchesAndPropertyIsNotInInlineStyleDecl(const HTMLElementEquivalent* equivalent, const Element* element,
    EditingStyle::CSSPropertyOverrideMode mode, StylePropertySet* style)
{
    return equivalent->matches(element) && (!element->inlineStyle() || !equivalent->propertyExistsInStyle(element->inlineStyle()))
        && (mode == EditingStyle::OverrideValues || !equivalent->propertyExistsInStyle(style));
}

static MutableStylePropertySet* extractEditingProperties(const StylePropertySet* style, EditingStyle::PropertiesToInclude propertiesToInclude)
{
    if (!style)
        return nullptr;

    switch (propertiesToInclude) {
    case EditingStyle::AllProperties:
    case EditingStyle::EditingPropertiesInEffect:
        return copyEditingProperties(style, AllEditingProperties);
    case EditingStyle::OnlyEditingInheritableProperties:
        return copyEditingProperties(style, OnlyInheritableEditingProperties);
    }

    NOTREACHED();
    return nullptr;
}

void EditingStyle::mergeInlineAndImplicitStyleOfElement(Element* element, CSSPropertyOverrideMode mode, PropertiesToInclude propertiesToInclude)
{
    EditingStyle* styleFromRules = EditingStyle::create();
    styleFromRules->mergeStyleFromRulesForSerialization(element);

    if (element->inlineStyle())
        styleFromRules->m_mutableStyle->mergeAndOverrideOnConflict(element->inlineStyle());

    styleFromRules->m_mutableStyle = extractEditingProperties(styleFromRules->m_mutableStyle.get(), propertiesToInclude);
    mergeStyle(styleFromRules->m_mutableStyle.get(), mode);

    const HeapVector<Member<HTMLElementEquivalent>>& elementEquivalents = htmlElementEquivalents();
    for (const auto& equivalent : elementEquivalents) {
        if (elementMatchesAndPropertyIsNotInInlineStyleDecl(equivalent.get(), element, mode, m_mutableStyle.get()))
            equivalent->addToStyle(element, this);
    }

    const HeapVector<Member<HTMLAttributeEquivalent>>& attributeEquivalents = htmlAttributeEquivalents();
    for (const auto& attribute : attributeEquivalents) {
        if (attribute->attributeName() == HTMLNames::dirAttr)
            continue; // We don't want to include directionality
        if (elementMatchesAndPropertyIsNotInInlineStyleDecl(attribute.get(), element, mode, m_mutableStyle.get()))
            attribute->addToStyle(element, this);
    }
}

EditingStyle* EditingStyle::wrappingStyleForAnnotatedSerialization(ContainerNode* context)
{
    EditingStyle* wrappingStyle = EditingStyle::create(context, EditingStyle::EditingPropertiesInEffect);

    // Styles that Mail blockquotes contribute should only be placed on the Mail
    // blockquote, to help us differentiate those styles from ones that the user
    // has applied. This helps us get the color of content pasted into
    // blockquotes right.
    wrappingStyle->removeStyleAddedByElement(toHTMLElement(enclosingNodeOfType(firstPositionInOrBeforeNode(context), isMailHTMLBlockquoteElement, CanCrossEditingBoundary)));

    // Call collapseTextDecorationProperties first or otherwise it'll copy the value over from in-effect to text-decorations.
    wrappingStyle->collapseTextDecorationProperties();

    return wrappingStyle;
}

EditingStyle* EditingStyle::wrappingStyleForSerialization(ContainerNode* context)
{
    DCHECK(context);
    EditingStyle* wrappingStyle = EditingStyle::create();

    // When not annotating for interchange, we only preserve inline style declarations.
    for (Node& node : NodeTraversal::inclusiveAncestorsOf(*context)) {
        if (node.isDocumentNode())
            break;
        if (node.isStyledElement() && !isMailHTMLBlockquoteElement(&node)) {
            wrappingStyle->mergeInlineAndImplicitStyleOfElement(toElement(&node), EditingStyle::DoNotOverrideValues,
                EditingStyle::EditingPropertiesInEffect);
        }
    }

    return wrappingStyle;
}

static CSSValueList* mergeTextDecorationValues(const CSSValueList* mergedValue, const CSSValueList* valueToMerge)
{
    DEFINE_STATIC_LOCAL(CSSPrimitiveValue, underline, (CSSPrimitiveValue::createIdentifier(CSSValueUnderline)));
    DEFINE_STATIC_LOCAL(CSSPrimitiveValue, lineThrough, (CSSPrimitiveValue::createIdentifier(CSSValueLineThrough)));
    CSSValueList* result = mergedValue->copy();
    if (valueToMerge->hasValue(underline) && !mergedValue->hasValue(underline))
        result->append(underline);

    if (valueToMerge->hasValue(lineThrough) && !mergedValue->hasValue(lineThrough))
        result->append(lineThrough);

    return result;
}

void EditingStyle::mergeStyle(const StylePropertySet* style, CSSPropertyOverrideMode mode)
{
    if (!style)
        return;

    if (!m_mutableStyle) {
        m_mutableStyle = style->mutableCopy();
        return;
    }

    unsigned propertyCount = style->propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        StylePropertySet::PropertyReference property = style->propertyAt(i);
        const CSSValue* value = m_mutableStyle->getPropertyCSSValue(property.id());

        // text decorations never override values
        if ((property.id() == textDecorationPropertyForEditing() || property.id() == CSSPropertyWebkitTextDecorationsInEffect) && property.value()->isValueList() && value) {
            if (value->isValueList()) {
                CSSValueList* result = mergeTextDecorationValues(toCSSValueList(value), toCSSValueList(property.value()));
                m_mutableStyle->setProperty(property.id(), result, property.isImportant());
                continue;
            }
            value = nullptr; // text-decoration: none is equivalent to not having the property
        }

        if (mode == OverrideValues || (mode == DoNotOverrideValues && !value))
            m_mutableStyle->setProperty(property.toCSSProperty());
    }
}

static MutableStylePropertySet* styleFromMatchedRulesForElement(Element* element, unsigned rulesToInclude)
{
    MutableStylePropertySet* style = MutableStylePropertySet::create(HTMLQuirksMode);
    StyleRuleList* matchedRules = element->document().ensureStyleResolver().styleRulesForElement(element, rulesToInclude);
    if (matchedRules) {
        for (unsigned i = 0; i < matchedRules->size(); ++i)
            style->mergeAndOverrideOnConflict(&matchedRules->at(i)->properties());
    }
    return style;
}

void EditingStyle::mergeStyleFromRules(Element* element)
{
    MutableStylePropertySet* styleFromMatchedRules = styleFromMatchedRulesForElement(element,
        StyleResolver::AuthorCSSRules | StyleResolver::CrossOriginCSSRules);
    // Styles from the inline style declaration, held in the variable "style", take precedence
    // over those from matched rules.
    if (m_mutableStyle)
        styleFromMatchedRules->mergeAndOverrideOnConflict(m_mutableStyle.get());

    clear();
    m_mutableStyle = styleFromMatchedRules;
}

void EditingStyle::mergeStyleFromRulesForSerialization(Element* element)
{
    mergeStyleFromRules(element);

    // The property value, if it's a percentage, may not reflect the actual computed value.
    // For example: style="height: 1%; overflow: visible;" in quirksmode
    // FIXME: There are others like this, see <rdar://problem/5195123> Slashdot copy/paste fidelity problem
    CSSComputedStyleDeclaration* computedStyleForElement = CSSComputedStyleDeclaration::create(element);
    MutableStylePropertySet* fromComputedStyle = MutableStylePropertySet::create(HTMLQuirksMode);
    {
        unsigned propertyCount = m_mutableStyle->propertyCount();
        for (unsigned i = 0; i < propertyCount; ++i) {
            StylePropertySet::PropertyReference property = m_mutableStyle->propertyAt(i);
            CSSValue* value = property.value();
            if (!value->isPrimitiveValue())
                continue;
            if (toCSSPrimitiveValue(value)->isPercentage()) {
                if (const CSSValue* computedPropertyValue = computedStyleForElement->getPropertyCSSValue(property.id()))
                    fromComputedStyle->addRespectingCascade(CSSProperty(property.id(), *computedPropertyValue));
            }
        }
    }
    m_mutableStyle->mergeAndOverrideOnConflict(fromComputedStyle);
}

static void removePropertiesInStyle(MutableStylePropertySet* styleToRemovePropertiesFrom, StylePropertySet* style)
{
    unsigned propertyCount = style->propertyCount();
    Vector<CSSPropertyID> propertiesToRemove(propertyCount);
    for (unsigned i = 0; i < propertyCount; ++i)
        propertiesToRemove[i] = style->propertyAt(i).id();

    styleToRemovePropertiesFrom->removePropertiesInSet(propertiesToRemove.data(), propertiesToRemove.size());
}

void EditingStyle::removeStyleFromRulesAndContext(Element* element, ContainerNode* context)
{
    DCHECK(element);
    if (!m_mutableStyle)
        return;

    // 1. Remove style from matched rules because style remain without repeating it in inline style declaration
    MutableStylePropertySet* styleFromMatchedRules = styleFromMatchedRulesForElement(element, StyleResolver::AllButEmptyCSSRules);
    if (styleFromMatchedRules && !styleFromMatchedRules->isEmpty())
        m_mutableStyle = getPropertiesNotIn(m_mutableStyle.get(), styleFromMatchedRules->ensureCSSStyleDeclaration());

    // 2. Remove style present in context and not overriden by matched rules.
    EditingStyle* computedStyle = EditingStyle::create(context, EditingPropertiesInEffect);
    if (computedStyle->m_mutableStyle) {
        if (!computedStyle->m_mutableStyle->getPropertyCSSValue(CSSPropertyBackgroundColor))
            computedStyle->m_mutableStyle->setProperty(CSSPropertyBackgroundColor, CSSValueTransparent);

        removePropertiesInStyle(computedStyle->m_mutableStyle.get(), styleFromMatchedRules);
        m_mutableStyle = getPropertiesNotIn(m_mutableStyle.get(), computedStyle->m_mutableStyle->ensureCSSStyleDeclaration());
    }

    // 3. If this element is a span and has display: inline or float: none, remove them unless they are overriden by rules.
    // These rules are added by serialization code to wrap text nodes.
    if (isStyleSpanOrSpanWithOnlyStyleAttribute(element)) {
        if (!styleFromMatchedRules->getPropertyCSSValue(CSSPropertyDisplay) && getIdentifierValue(m_mutableStyle.get(), CSSPropertyDisplay) == CSSValueInline)
            m_mutableStyle->removeProperty(CSSPropertyDisplay);
        if (!styleFromMatchedRules->getPropertyCSSValue(CSSPropertyFloat) && getIdentifierValue(m_mutableStyle.get(), CSSPropertyFloat) == CSSValueNone)
            m_mutableStyle->removeProperty(CSSPropertyFloat);
    }
}

void EditingStyle::removePropertiesInElementDefaultStyle(Element* element)
{
    if (!m_mutableStyle || m_mutableStyle->isEmpty())
        return;

    StylePropertySet* defaultStyle = styleFromMatchedRulesForElement(element, StyleResolver::UAAndUserCSSRules);

    removePropertiesInStyle(m_mutableStyle.get(), defaultStyle);
}

void EditingStyle::addAbsolutePositioningFromElement(const Element& element)
{
    LayoutRect rect = element.boundingBox();
    LayoutObject* layoutObject = element.layoutObject();

    LayoutUnit x = rect.x();
    LayoutUnit y = rect.y();
    LayoutUnit width = rect.width();
    LayoutUnit height = rect.height();
    if (layoutObject && layoutObject->isBox()) {
        LayoutBox* layoutBox = toLayoutBox(layoutObject);

        x -= layoutBox->marginLeft();
        y -= layoutBox->marginTop();

        m_mutableStyle->setProperty(CSSPropertyBoxSizing, CSSValueBorderBox);
    }

    m_mutableStyle->setProperty(CSSPropertyPosition, CSSValueAbsolute);
    m_mutableStyle->setProperty(CSSPropertyLeft, CSSPrimitiveValue::create(x, CSSPrimitiveValue::UnitType::Pixels));
    m_mutableStyle->setProperty(CSSPropertyTop, CSSPrimitiveValue::create(y, CSSPrimitiveValue::UnitType::Pixels));
    m_mutableStyle->setProperty(CSSPropertyWidth, CSSPrimitiveValue::create(width, CSSPrimitiveValue::UnitType::Pixels));
    m_mutableStyle->setProperty(CSSPropertyHeight, CSSPrimitiveValue::create(height, CSSPrimitiveValue::UnitType::Pixels));
}

void EditingStyle::forceInline()
{
    if (!m_mutableStyle)
        m_mutableStyle = MutableStylePropertySet::create(HTMLQuirksMode);
    const bool propertyIsImportant = true;
    m_mutableStyle->setProperty(CSSPropertyDisplay, CSSValueInline, propertyIsImportant);
}

int EditingStyle::legacyFontSize(Document* document) const
{
    const CSSValue* cssValue = m_mutableStyle->getPropertyCSSValue(CSSPropertyFontSize);
    if (!cssValue || !cssValue->isPrimitiveValue())
        return 0;
    return legacyFontSizeFromCSSValue(document, toCSSPrimitiveValue(cssValue),
        m_isMonospaceFont, AlwaysUseLegacyFontSize);
}

EditingStyle* EditingStyle::styleAtSelectionStart(const VisibleSelection& selection, bool shouldUseBackgroundColorInEffect, MutableStylePropertySet* styleToCheck)
{
    if (selection.isNone())
        return nullptr;

    Position position = adjustedSelectionStartForStyleComputation(selection);

    // If the pos is at the end of a text node, then this node is not fully selected.
    // Move it to the next deep equivalent position to avoid removing the style from this node.
    // e.g. if pos was at Position("hello", 5) in <b>hello<div>world</div></b>, we want Position("world", 0) instead.
    // We only do this for range because caret at Position("hello", 5) in <b>hello</b>world should give you font-weight: bold.
    Node* positionNode = position.computeContainerNode();
    if (selection.isRange() && positionNode && positionNode->isTextNode() && position.computeOffsetInContainerNode() == positionNode->maxCharacterOffset())
        position = nextVisuallyDistinctCandidate(position);

    Element* element = associatedElementOf(position);
    if (!element)
        return nullptr;

    EditingStyle* style = EditingStyle::create(element, EditingStyle::AllProperties);
    style->mergeTypingStyle(&element->document());

    // If |element| has <sub> or <sup> ancestor element, apply the corresponding
    // style(vertical-align) to it so that document.queryCommandState() works with the style.
    // See bug http://crbug.com/582225.
    CSSValueID valueID = getIdentifierValue(styleToCheck, CSSPropertyVerticalAlign);
    if (valueID == CSSValueSub || valueID == CSSValueSuper) {
        CSSComputedStyleDeclaration* elementStyle = CSSComputedStyleDeclaration::create(element);
        // Find the ancestor that has CSSValueSub or CSSValueSuper as the value of CSS vertical-align property.
        if (getIdentifierValue(elementStyle, CSSPropertyVerticalAlign) == CSSValueBaseline && hasAncestorVerticalAlignStyle(*element, valueID))
            style->m_mutableStyle->setProperty(CSSPropertyVerticalAlign, valueID);
    }

    // If background color is transparent, traverse parent nodes until we hit a different value or document root
    // Also, if the selection is a range, ignore the background color at the start of selection,
    // and find the background color of the common ancestor.
    if (shouldUseBackgroundColorInEffect && (selection.isRange() || hasTransparentBackgroundColor(style->m_mutableStyle.get()))) {
        const EphemeralRange range(selection.toNormalizedEphemeralRange());
        if (const CSSValue* value = backgroundColorValueInEffect(Range::commonAncestorContainer(range.startPosition().computeContainerNode(), range.endPosition().computeContainerNode())))
            style->setProperty(CSSPropertyBackgroundColor, value->cssText());
    }

    return style;
}

static bool isUnicodeBidiNestedOrMultipleEmbeddings(CSSValueID valueID)
{
    return valueID == CSSValueEmbed || valueID == CSSValueBidiOverride
        || valueID == CSSValueWebkitIsolate || valueID == CSSValueWebkitIsolateOverride || valueID == CSSValueWebkitPlaintext
        || valueID == CSSValueIsolate || valueID == CSSValueIsolateOverride || valueID == CSSValuePlaintext;
}

WritingDirection EditingStyle::textDirectionForSelection(const VisibleSelection& selection, EditingStyle* typingStyle, bool& hasNestedOrMultipleEmbeddings)
{
    hasNestedOrMultipleEmbeddings = true;

    if (selection.isNone())
        return NaturalWritingDirection;

    Position position = mostForwardCaretPosition(selection.start());

    Node* node = position.anchorNode();
    if (!node)
        return NaturalWritingDirection;

    Position end;
    if (selection.isRange()) {
        end = mostBackwardCaretPosition(selection.end());

        DCHECK(end.document());
        Node* pastLast = Range::create(*end.document(), position.parentAnchoredEquivalent(), end.parentAnchoredEquivalent())->pastLastNode();
        for (Node* n = node; n && n != pastLast; n = NodeTraversal::next(*n)) {
            if (!n->isStyledElement())
                continue;

            CSSComputedStyleDeclaration* style = CSSComputedStyleDeclaration::create(n);
            const CSSValue* unicodeBidi = style->getPropertyCSSValue(CSSPropertyUnicodeBidi);
            if (!unicodeBidi || !unicodeBidi->isPrimitiveValue())
                continue;

            CSSValueID unicodeBidiValue = toCSSPrimitiveValue(unicodeBidi)->getValueID();
            if (isUnicodeBidiNestedOrMultipleEmbeddings(unicodeBidiValue))
                return NaturalWritingDirection;
        }
    }

    if (selection.isCaret()) {
        WritingDirection direction;
        if (typingStyle && typingStyle->textDirection(direction)) {
            hasNestedOrMultipleEmbeddings = false;
            return direction;
        }
        node = selection.visibleStart().deepEquivalent().anchorNode();
    }
    DCHECK(node);

    // The selection is either a caret with no typing attributes or a range in which no embedding is added, so just use the start position
    // to decide.
    Node* block = enclosingBlock(node);
    WritingDirection foundDirection = NaturalWritingDirection;

    for (Node& runner : NodeTraversal::inclusiveAncestorsOf(*node)) {
        if (runner == block)
            break;
        if (!runner.isStyledElement())
            continue;

        Element* element = &toElement(runner);
        CSSComputedStyleDeclaration* style = CSSComputedStyleDeclaration::create(element);
        const CSSValue* unicodeBidi = style->getPropertyCSSValue(CSSPropertyUnicodeBidi);
        if (!unicodeBidi || !unicodeBidi->isPrimitiveValue())
            continue;

        CSSValueID unicodeBidiValue = toCSSPrimitiveValue(unicodeBidi)->getValueID();
        if (unicodeBidiValue == CSSValueNormal)
            continue;

        if (unicodeBidiValue == CSSValueBidiOverride)
            return NaturalWritingDirection;

        DCHECK(isEmbedOrIsolate(unicodeBidiValue)) << unicodeBidiValue;
        const CSSValue* direction = style->getPropertyCSSValue(CSSPropertyDirection);
        if (!direction || !direction->isPrimitiveValue())
            continue;

        int directionValue = toCSSPrimitiveValue(direction)->getValueID();
        if (directionValue != CSSValueLtr && directionValue != CSSValueRtl)
            continue;

        if (foundDirection != NaturalWritingDirection)
            return NaturalWritingDirection;

        // In the range case, make sure that the embedding element persists until the end of the range.
        if (selection.isRange() && !end.anchorNode()->isDescendantOf(element))
            return NaturalWritingDirection;

        foundDirection = directionValue == CSSValueLtr ? LeftToRightWritingDirection : RightToLeftWritingDirection;
    }
    hasNestedOrMultipleEmbeddings = false;
    return foundDirection;
}

DEFINE_TRACE(EditingStyle)
{
    visitor->trace(m_mutableStyle);
}

static void reconcileTextDecorationProperties(MutableStylePropertySet* style)
{
    const CSSValue* textDecorationsInEffect = style->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
    const CSSValue* textDecoration = style->getPropertyCSSValue(textDecorationPropertyForEditing());
    // We shouldn't have both text-decoration and -webkit-text-decorations-in-effect because that wouldn't make sense.
    DCHECK(!textDecorationsInEffect || !textDecoration);
    if (textDecorationsInEffect) {
        style->setProperty(textDecorationPropertyForEditing(), textDecorationsInEffect->cssText());
        style->removeProperty(CSSPropertyWebkitTextDecorationsInEffect);
        textDecoration = textDecorationsInEffect;
    }

    // If text-decoration is set to "none", remove the property because we don't want to add redundant "text-decoration: none".
    if (textDecoration && !textDecoration->isValueList())
        style->removeProperty(textDecorationPropertyForEditing());
}

StyleChange::StyleChange(EditingStyle* style, const Position& position)
    : m_applyBold(false)
    , m_applyItalic(false)
    , m_applyUnderline(false)
    , m_applyLineThrough(false)
    , m_applySubscript(false)
    , m_applySuperscript(false)
{
    Document* document = position.document();
    if (!style || !style->style() || !document || !document->frame() || !associatedElementOf(position))
        return;

    CSSComputedStyleDeclaration* computedStyle = ensureComputedStyle(position);
    // FIXME: take care of background-color in effect
    MutableStylePropertySet* mutableStyle = getPropertiesNotIn(style->style(), computedStyle);
    DCHECK(mutableStyle);

    reconcileTextDecorationProperties(mutableStyle);
    if (!document->frame()->editor().shouldStyleWithCSS())
        extractTextStyles(document, mutableStyle, computedStyle->isMonospaceFont());

    // Changing the whitespace style in a tab span would collapse the tab into a space.
    if (isTabHTMLSpanElementTextNode(position.anchorNode()) || isTabHTMLSpanElement((position.anchorNode())))
        mutableStyle->removeProperty(CSSPropertyWhiteSpace);

    // If unicode-bidi is present in mutableStyle and direction is not, then add direction to mutableStyle.
    // FIXME: Shouldn't this be done in getPropertiesNotIn?
    if (mutableStyle->getPropertyCSSValue(CSSPropertyUnicodeBidi) && !style->style()->getPropertyCSSValue(CSSPropertyDirection))
        mutableStyle->setProperty(CSSPropertyDirection, style->style()->getPropertyValue(CSSPropertyDirection));

    // Save the result for later
    m_cssStyle = mutableStyle->asText().stripWhiteSpace();
}

static void setTextDecorationProperty(MutableStylePropertySet* style, const CSSValueList* newTextDecoration, CSSPropertyID propertyID)
{
    if (newTextDecoration->length()) {
        style->setProperty(propertyID, newTextDecoration->cssText(), style->propertyIsImportant(propertyID));
    } else {
        // text-decoration: none is redundant since it does not remove any text
        // decorations.
        style->removeProperty(propertyID);
    }
}

void StyleChange::extractTextStyles(Document* document, MutableStylePropertySet* style, bool isMonospaceFont)
{
    DCHECK(style);

    if (getIdentifierValue(style, CSSPropertyFontWeight) == CSSValueBold) {
        style->removeProperty(CSSPropertyFontWeight);
        m_applyBold = true;
    }

    int fontStyle = getIdentifierValue(style, CSSPropertyFontStyle);
    if (fontStyle == CSSValueItalic || fontStyle == CSSValueOblique) {
        style->removeProperty(CSSPropertyFontStyle);
        m_applyItalic = true;
    }

    // Assuming reconcileTextDecorationProperties has been called, there should not be -webkit-text-decorations-in-effect
    // Furthermore, text-decoration: none has been trimmed so that text-decoration property is always a CSSValueList.
    const CSSValue* textDecoration = style->getPropertyCSSValue(textDecorationPropertyForEditing());
    if (textDecoration && textDecoration->isValueList()) {
        DEFINE_STATIC_LOCAL(CSSPrimitiveValue, underline, (CSSPrimitiveValue::createIdentifier(CSSValueUnderline)));
        DEFINE_STATIC_LOCAL(CSSPrimitiveValue, lineThrough, (CSSPrimitiveValue::createIdentifier(CSSValueLineThrough)));
        CSSValueList* newTextDecoration = toCSSValueList(textDecoration)->copy();
        if (newTextDecoration->removeAll(underline))
            m_applyUnderline = true;
        if (newTextDecoration->removeAll(lineThrough))
            m_applyLineThrough = true;

        // If trimTextDecorations, delete underline and line-through
        setTextDecorationProperty(style, newTextDecoration, textDecorationPropertyForEditing());
    }

    int verticalAlign = getIdentifierValue(style, CSSPropertyVerticalAlign);
    switch (verticalAlign) {
    case CSSValueSub:
        style->removeProperty(CSSPropertyVerticalAlign);
        m_applySubscript = true;
        break;
    case CSSValueSuper:
        style->removeProperty(CSSPropertyVerticalAlign);
        m_applySuperscript = true;
        break;
    }

    if (style->getPropertyCSSValue(CSSPropertyColor)) {
        m_applyFontColor = getFontColor(style).serialized();
        style->removeProperty(CSSPropertyColor);
    }

    m_applyFontFace = style->getPropertyValue(CSSPropertyFontFamily);
    // Remove double quotes for Outlook 2007 compatibility. See https://bugs.webkit.org/show_bug.cgi?id=79448
    m_applyFontFace.replace('"', "");
    style->removeProperty(CSSPropertyFontFamily);

    if (const CSSValue* fontSize = style->getPropertyCSSValue(CSSPropertyFontSize)) {
        if (!fontSize->isPrimitiveValue()) {
            style->removeProperty(CSSPropertyFontSize); // Can't make sense of the number. Put no font size.
        } else if (int legacyFontSize = legacyFontSizeFromCSSValue(document, toCSSPrimitiveValue(fontSize), isMonospaceFont, UseLegacyFontSizeOnlyIfPixelValuesMatch)) {
            m_applyFontSize = String::number(legacyFontSize);
            style->removeProperty(CSSPropertyFontSize);
        }
    }
}

static void diffTextDecorations(MutableStylePropertySet* style, CSSPropertyID propertyID, const CSSValue* refTextDecoration)
{
    const CSSValue* textDecoration = style->getPropertyCSSValue(propertyID);
    if (!textDecoration || !textDecoration->isValueList() || !refTextDecoration || !refTextDecoration->isValueList())
        return;

    CSSValueList* newTextDecoration = toCSSValueList(textDecoration)->copy();
    const CSSValueList* valuesInRefTextDecoration = toCSSValueList(refTextDecoration);

    for (size_t i = 0; i < valuesInRefTextDecoration->length(); i++)
        newTextDecoration->removeAll(valuesInRefTextDecoration->item(i));

    setTextDecorationProperty(style, newTextDecoration, propertyID);
}

static bool fontWeightIsBold(const CSSValue* fontWeight)
{
    if (!fontWeight->isPrimitiveValue())
        return false;

    // Because b tag can only bold text, there are only two states in plain html: bold and not bold.
    // Collapse all other values to either one of these two states for editing purposes.
    switch (toCSSPrimitiveValue(fontWeight)->getValueID()) {
    case CSSValue100:
    case CSSValue200:
    case CSSValue300:
    case CSSValue400:
    case CSSValue500:
    case CSSValueNormal:
        return false;
    case CSSValueBold:
    case CSSValue600:
    case CSSValue700:
    case CSSValue800:
    case CSSValue900:
        return true;
    default:
        break;
    }

    NOTREACHED(); // For CSSValueBolder and CSSValueLighter
    return false;
}

static bool fontWeightNeedsResolving(const CSSValue* fontWeight)
{
    if (!fontWeight->isPrimitiveValue())
        return true;

    const CSSValueID value = toCSSPrimitiveValue(fontWeight)->getValueID();
    return value == CSSValueLighter || value == CSSValueBolder;
}

MutableStylePropertySet* getPropertiesNotIn(StylePropertySet* styleWithRedundantProperties, CSSStyleDeclaration* baseStyle)
{
    DCHECK(styleWithRedundantProperties);
    DCHECK(baseStyle);
    MutableStylePropertySet* result = styleWithRedundantProperties->mutableCopy();

    result->removeEquivalentProperties(baseStyle);

    const CSSValue* baseTextDecorationsInEffect = baseStyle->getPropertyCSSValueInternal(CSSPropertyWebkitTextDecorationsInEffect);
    diffTextDecorations(result, textDecorationPropertyForEditing(), baseTextDecorationsInEffect);
    diffTextDecorations(result, CSSPropertyWebkitTextDecorationsInEffect, baseTextDecorationsInEffect);

    if (const CSSValue* baseFontWeight = baseStyle->getPropertyCSSValueInternal(CSSPropertyFontWeight)) {
        if (CSSValue* fontWeight = result->getPropertyCSSValue(CSSPropertyFontWeight)) {
            if (!fontWeightNeedsResolving(fontWeight) && !fontWeightNeedsResolving(baseFontWeight)
                && (fontWeightIsBold(fontWeight) == fontWeightIsBold(baseFontWeight)))
                result->removeProperty(CSSPropertyFontWeight);
        }
    }

    if (baseStyle->getPropertyCSSValueInternal(CSSPropertyColor) && getFontColor(result) == getFontColor(baseStyle))
        result->removeProperty(CSSPropertyColor);

    if (baseStyle->getPropertyCSSValueInternal(CSSPropertyTextAlign)
        && textAlignResolvingStartAndEnd(result) == textAlignResolvingStartAndEnd(baseStyle))
        result->removeProperty(CSSPropertyTextAlign);

    if (baseStyle->getPropertyCSSValueInternal(CSSPropertyBackgroundColor) && getBackgroundColor(result) == getBackgroundColor(baseStyle))
        result->removeProperty(CSSPropertyBackgroundColor);

    return result;
}

CSSValueID getIdentifierValue(StylePropertySet* style, CSSPropertyID propertyID)
{
    if (!style)
        return CSSValueInvalid;
    const CSSValue* value = style->getPropertyCSSValue(propertyID);
    if (!value || !value->isPrimitiveValue())
        return CSSValueInvalid;
    return toCSSPrimitiveValue(value)->getValueID();
}

CSSValueID getIdentifierValue(CSSStyleDeclaration* style, CSSPropertyID propertyID)
{
    if (!style)
        return CSSValueInvalid;
    const CSSValue* value = style->getPropertyCSSValueInternal(propertyID);
    if (!value || !value->isPrimitiveValue())
        return CSSValueInvalid;
    return toCSSPrimitiveValue(value)->getValueID();
}

int legacyFontSizeFromCSSValue(Document* document, const CSSPrimitiveValue* value, bool isMonospaceFont, LegacyFontSizeMode mode)
{
    CSSPrimitiveValue::LengthUnitType lengthType;
    if (CSSPrimitiveValue::unitTypeToLengthUnitType(value->typeWithCalcResolved(), lengthType)
        && lengthType == CSSPrimitiveValue::UnitTypePixels) {
        double conversion = CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(value->typeWithCalcResolved());
        int pixelFontSize = clampTo<int>(value->getDoubleValue() * conversion);
        int legacyFontSize = FontSize::legacyFontSize(document, pixelFontSize, isMonospaceFont);
        // Use legacy font size only if pixel value matches exactly to that of legacy font size.
        if (mode == AlwaysUseLegacyFontSize || FontSize::fontSizeForKeyword(document, legacyFontSize, isMonospaceFont) == pixelFontSize)
            return legacyFontSize;

        return 0;
    }

    if (CSSValueXSmall <= value->getValueID() && value->getValueID() <= CSSValueWebkitXxxLarge)
        return value->getValueID() - CSSValueXSmall + 1;

    return 0;
}

bool isTransparentColorValue(const CSSValue* cssValue)
{
    if (!cssValue)
        return true;
    if (cssValue->isColorValue())
        return !toCSSColorValue(cssValue)->value().alpha();
    if (!cssValue->isPrimitiveValue())
        return false;
    const CSSPrimitiveValue* value = toCSSPrimitiveValue(cssValue);
    return value->getValueID() == CSSValueTransparent;
}

bool hasTransparentBackgroundColor(CSSStyleDeclaration* style)
{
    const CSSValue* cssValue = style->getPropertyCSSValueInternal(CSSPropertyBackgroundColor);
    return isTransparentColorValue(cssValue);
}

bool hasTransparentBackgroundColor(StylePropertySet* style)
{
    const CSSValue* cssValue = style->getPropertyCSSValue(CSSPropertyBackgroundColor);
    return isTransparentColorValue(cssValue);
}

const CSSValue* backgroundColorValueInEffect(Node* node)
{
    for (Node* ancestor = node; ancestor; ancestor = ancestor->parentNode()) {
        CSSComputedStyleDeclaration* ancestorStyle = CSSComputedStyleDeclaration::create(ancestor);
        if (!hasTransparentBackgroundColor(ancestorStyle))
            return ancestorStyle->getPropertyCSSValue(CSSPropertyBackgroundColor);
    }
    return nullptr;
}

} // namespace blink
