/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PropertySetCSSStyleDeclaration_h
#define PropertySetCSSStyleDeclaration_h

#include "core/css/CSSStyleDeclaration.h"
#include "wtf/HashMap.h"

namespace blink {

class CSSRule;
class CSSValue;
class Element;
class ExceptionState;
class MutableStylePropertySet;
class StyleSheetContents;

class AbstractPropertySetCSSStyleDeclaration : public CSSStyleDeclaration {
public:
    virtual Element* parentElement() const { return 0; }
    StyleSheetContents* contextStyleSheet() const;

    DECLARE_VIRTUAL_TRACE();

private:
    CSSRule* parentRule() const override { return 0; }
    unsigned length() const final;
    String item(unsigned index) const final;
    String getPropertyValue(const String& propertyName) final;
    String getPropertyPriority(const String& propertyName) final;
    String getPropertyShorthand(const String& propertyName) final;
    bool isPropertyImplicit(const String& propertyName) final;
    void setProperty(const String& propertyName, const String& value, const String& priority, ExceptionState&) final;
    String removeProperty(const String& propertyName, ExceptionState&) final;
    String cssFloat() const;
    void setCSSFloat(const String&, ExceptionState&);
    String cssText() const final;
    void setCSSText(const String&, ExceptionState&) final;
    const CSSValue* getPropertyCSSValueInternal(CSSPropertyID) final;
    const CSSValue* getPropertyCSSValueInternal(AtomicString customPropertyName) final;
    String getPropertyValueInternal(CSSPropertyID) final;
    void setPropertyInternal(CSSPropertyID, const String& customPropertyName, const String& value, bool important, ExceptionState&) final;

    bool cssPropertyMatches(CSSPropertyID, const CSSValue*) const final;

protected:
    enum MutationType { NoChanges, PropertyChanged };
    virtual void willMutate() { }
    virtual void didMutate(MutationType) { }
    virtual MutableStylePropertySet& propertySet() const = 0;
};

class PropertySetCSSStyleDeclaration : public AbstractPropertySetCSSStyleDeclaration {
public:
    PropertySetCSSStyleDeclaration(MutableStylePropertySet& propertySet) : m_propertySet(&propertySet) { }

    DECLARE_VIRTUAL_TRACE();

protected:
    MutableStylePropertySet& propertySet() const final { ASSERT(m_propertySet); return *m_propertySet; }

    Member<MutableStylePropertySet> m_propertySet; // Cannot be null
};

class StyleRuleCSSStyleDeclaration : public PropertySetCSSStyleDeclaration {
public:
    static StyleRuleCSSStyleDeclaration* create(MutableStylePropertySet& propertySet, CSSRule* parentRule)
    {
        return new StyleRuleCSSStyleDeclaration(propertySet, parentRule);
    }

    void reattach(MutableStylePropertySet&);

    DECLARE_VIRTUAL_TRACE();

protected:
    StyleRuleCSSStyleDeclaration(MutableStylePropertySet&, CSSRule*);
    ~StyleRuleCSSStyleDeclaration() override;

    CSSStyleSheet* parentStyleSheet() const override;

    CSSRule* parentRule() const override { return m_parentRule;  }

    void willMutate() override;
    void didMutate(MutationType) override;

    Member<CSSRule> m_parentRule;
};

class InlineCSSStyleDeclaration final : public AbstractPropertySetCSSStyleDeclaration {
public:
    explicit InlineCSSStyleDeclaration(Element* parentElement)
        : m_parentElement(parentElement)
    {
    }

    DECLARE_VIRTUAL_TRACE();

private:
    MutableStylePropertySet& propertySet() const override;
    CSSStyleSheet* parentStyleSheet() const override;
    Element* parentElement() const override { return m_parentElement; }

    void didMutate(MutationType) override;

    Member<Element> m_parentElement;
};

} // namespace blink

#endif
