/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorStyleSheet_h
#define InspectorStyleSheet_h

#include "core/InspectorTypeBuilder.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/inspector/InspectorStyleTextEditor.h"
#include "platform/JSONValues.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

class ParsedStyleSheet;

namespace WebCore {

class CSSRuleList;
class CSSStyleDeclaration;
class CSSStyleRule;
class CSSStyleSheet;
class Document;
class Element;
class ExceptionState;
class InspectorPageAgent;
class InspectorResourceAgent;
class InspectorStyleSheetBase;

typedef WillBePersistentHeapVector<RefPtrWillBeMember<CSSRule> > CSSRuleVector;
typedef String ErrorString;

class InspectorCSSId {
public:
    InspectorCSSId()
        : m_ordinal(0)
    {
    }

    InspectorCSSId(const String& styleSheetId, unsigned ordinal)
        : m_styleSheetId(styleSheetId)
        , m_ordinal(ordinal)
    {
    }

    bool isEmpty() const { return m_styleSheetId.isEmpty(); }

    const String& styleSheetId() const { return m_styleSheetId; }
    unsigned ordinal() const { return m_ordinal; }

private:
    String m_styleSheetId;
    unsigned m_ordinal;
};

struct InspectorStyleProperty {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    explicit InspectorStyleProperty(CSSPropertySourceData sourceData)
        : sourceData(sourceData)
        , hasSource(true)
    {
    }

    InspectorStyleProperty(CSSPropertySourceData sourceData, bool hasSource)
        : sourceData(sourceData)
        , hasSource(hasSource)
    {
    }

    void setRawTextFromStyleDeclaration(const String& styleDeclaration)
    {
        unsigned start = sourceData.range.start;
        unsigned end = sourceData.range.end;
        ASSERT(start < end);
        ASSERT(end <= styleDeclaration.length());
        rawText = styleDeclaration.substring(start, end - start);
    }

    bool hasRawText() const { return !rawText.isEmpty(); }

    void trace(Visitor* visitor) { visitor->trace(sourceData); }

    CSSPropertySourceData sourceData;
    bool hasSource;
    String rawText;
};

class InspectorStyle FINAL : public RefCounted<InspectorStyle> {
public:
    static PassRefPtr<InspectorStyle> create(const InspectorCSSId&, PassRefPtrWillBeRawPtr<CSSStyleDeclaration>, InspectorStyleSheetBase* parentStyleSheet);

    CSSStyleDeclaration* cssStyle() const { return m_style.get(); }
    PassRefPtr<TypeBuilder::CSS::CSSStyle> buildObjectForStyle() const;
    PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty> > buildArrayForComputedStyle() const;
    bool setPropertyText(unsigned index, const String& text, bool overwrite, ExceptionState&);
    bool styleText(String* result) const;

private:
    InspectorStyle(const InspectorCSSId&, PassRefPtrWillBeRawPtr<CSSStyleDeclaration>, InspectorStyleSheetBase* parentStyleSheet);

    bool verifyPropertyText(const String& propertyText, bool canOmitSemicolon);
    void populateAllProperties(WillBeHeapVector<InspectorStyleProperty>& result) const;
    PassRefPtr<TypeBuilder::CSS::CSSStyle> styleWithProperties() const;
    PassRefPtrWillBeRawPtr<CSSRuleSourceData> extractSourceData() const;
    bool applyStyleText(const String&);
    String shorthandValue(const String& shorthandProperty) const;
    NewLineAndWhitespace& newLineAndWhitespaceDelimiters() const;
    inline Document* ownerDocument() const;

    InspectorCSSId m_styleId;
    RefPtrWillBePersistent<CSSStyleDeclaration> m_style;
    InspectorStyleSheetBase* m_parentStyleSheet;
    mutable std::pair<String, String> m_format;
    mutable bool m_formatAcquired;
};

class InspectorStyleSheetBase : public RefCounted<InspectorStyleSheetBase> {
public:
    class Listener {
    public:
        Listener() { }
        virtual ~Listener() { }
        virtual void styleSheetChanged(InspectorStyleSheetBase*) = 0;
        virtual void willReparseStyleSheet() = 0;
        virtual void didReparseStyleSheet() = 0;
    };
    virtual ~InspectorStyleSheetBase() { }

    String id() const { return m_id; }

    virtual Document* ownerDocument() const = 0;
    virtual bool setText(const String&, ExceptionState&) = 0;
    virtual bool getText(String* result) const = 0;
    bool setPropertyText(const InspectorCSSId&, unsigned propertyIndex, const String& text, bool overwrite, ExceptionState&);

    virtual bool setStyleText(const InspectorCSSId&, const String&) = 0;
    bool getStyleText(const InspectorCSSId&, String*);

    virtual CSSStyleDeclaration* styleForId(const InspectorCSSId&) const = 0;
    virtual InspectorCSSId styleId(CSSStyleDeclaration*) const = 0;

    PassRefPtr<TypeBuilder::CSS::CSSStyle> buildObjectForStyle(CSSStyleDeclaration*);
    bool findPropertyByRange(const SourceRange&, InspectorCSSId*, unsigned* propertyIndex, bool* overwrite);
    bool lineNumberAndColumnToOffset(unsigned lineNumber, unsigned columnNumber, unsigned* offset);

protected:
    InspectorStyleSheetBase(const String& id, Listener*);

    Listener* listener() const { return m_listener; }
    void fireStyleSheetChanged();
    PassOwnPtr<Vector<unsigned> > lineEndings();

    virtual PassRefPtr<InspectorStyle> inspectorStyleForId(const InspectorCSSId&) = 0;
    virtual unsigned ruleCount() = 0;

    // Also accessed by friend class InspectorStyle.
    virtual PassRefPtrWillBeRawPtr<CSSRuleSourceData> ruleSourceDataAt(unsigned) const = 0;
    virtual bool ensureParsedDataReady() = 0;

private:
    friend class InspectorStyle;

    String m_id;
    Listener* m_listener;
};

class InspectorStyleSheet : public InspectorStyleSheetBase {
public:
    static PassRefPtr<InspectorStyleSheet> create(InspectorPageAgent*, InspectorResourceAgent*, const String& id, PassRefPtrWillBeRawPtr<CSSStyleSheet> pageStyleSheet, TypeBuilder::CSS::StyleSheetOrigin::Enum, const String& documentURL, Listener*);

    virtual ~InspectorStyleSheet();

    String finalURL() const;
    virtual Document* ownerDocument() const OVERRIDE;
    virtual bool setText(const String&, ExceptionState&) OVERRIDE;
    virtual bool getText(String* result) const OVERRIDE;
    String ruleSelector(const InspectorCSSId&, ExceptionState&);
    bool setRuleSelector(const InspectorCSSId&, const String& selector, ExceptionState&);
    CSSStyleRule* addRule(const String& selector, ExceptionState&);
    bool deleteRule(const InspectorCSSId&, ExceptionState&);

    CSSStyleSheet* pageStyleSheet() const { return m_pageStyleSheet.get(); }

    PassRefPtr<TypeBuilder::CSS::CSSStyleSheetHeader> buildObjectForStyleSheetInfo() const;
    PassRefPtr<TypeBuilder::CSS::CSSRule> buildObjectForRule(CSSStyleRule*, PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> >);

    PassRefPtr<TypeBuilder::CSS::SourceRange> ruleHeaderSourceRange(const CSSRule*);

    InspectorCSSId ruleId(CSSStyleRule*) const;
    CSSStyleRule* ruleForId(const InspectorCSSId&) const;

    virtual InspectorCSSId styleId(CSSStyleDeclaration*) const OVERRIDE;
    virtual CSSStyleDeclaration* styleForId(const InspectorCSSId&) const OVERRIDE;
    virtual bool setStyleText(const InspectorCSSId&, const String&) OVERRIDE;

    bool findRuleBySelectorRange(const SourceRange&, InspectorCSSId*);

    const CSSRuleVector& flatRules();

protected:
    virtual PassRefPtr<InspectorStyle> inspectorStyleForId(const InspectorCSSId&) OVERRIDE;
    virtual unsigned ruleCount() OVERRIDE;

    // Also accessed by friend class InspectorStyle.
    virtual PassRefPtrWillBeRawPtr<CSSRuleSourceData> ruleSourceDataAt(unsigned) const OVERRIDE;
    virtual bool ensureParsedDataReady() OVERRIDE;

private:
    InspectorStyleSheet(InspectorPageAgent*, InspectorResourceAgent*, const String& id, PassRefPtrWillBeRawPtr<CSSStyleSheet> pageStyleSheet, TypeBuilder::CSS::StyleSheetOrigin::Enum, const String& documentURL, Listener*);

    unsigned ruleIndexByStyle(CSSStyleDeclaration*) const;
    String sourceMapURL() const;
    String sourceURL() const;
    bool ensureText() const;
    void ensureFlatRules() const;
    bool styleSheetTextWithChangedStyle(CSSStyleDeclaration*, const String& newStyleText, String* result);
    bool originalStyleSheetText(String* result) const;
    bool resourceStyleSheetText(String* result) const;
    bool inlineStyleSheetText(String* result) const;
    PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::Selector> > selectorsFromSource(const CSSRuleSourceData*, const String&);
    PassRefPtr<TypeBuilder::CSS::SelectorList> buildObjectForSelectorList(CSSStyleRule*);
    String url() const;
    bool hasSourceURL() const;
    bool startsAtZero() const;

    InspectorPageAgent* m_pageAgent;
    InspectorResourceAgent* m_resourceAgent;
    RefPtrWillBePersistent<CSSStyleSheet> m_pageStyleSheet;
    TypeBuilder::CSS::StyleSheetOrigin::Enum m_origin;
    String m_documentURL;
    OwnPtr<ParsedStyleSheet> m_parsedStyleSheet;
    mutable CSSRuleVector m_flatRules;
    mutable String m_sourceURL;
};

class InspectorStyleSheetForInlineStyle FINAL : public InspectorStyleSheetBase {
public:
    static PassRefPtr<InspectorStyleSheetForInlineStyle> create(const String& id, PassRefPtrWillBeRawPtr<Element>, Listener*);

    void didModifyElementAttribute();
    virtual Document* ownerDocument() const OVERRIDE;
    virtual bool setText(const String&, ExceptionState&) OVERRIDE;
    virtual bool getText(String* result) const OVERRIDE;

    virtual CSSStyleDeclaration* styleForId(const InspectorCSSId& id) const OVERRIDE { ASSERT_UNUSED(id, !id.ordinal()); return inlineStyle(); }
    virtual InspectorCSSId styleId(CSSStyleDeclaration* style) const OVERRIDE { return InspectorCSSId(id(), 0); }
    virtual bool setStyleText(const InspectorCSSId&, const String&) OVERRIDE;

protected:
    virtual PassRefPtr<InspectorStyle> inspectorStyleForId(const InspectorCSSId&) OVERRIDE;
    virtual unsigned ruleCount() OVERRIDE { return 1; }

    // Also accessed by friend class InspectorStyle.
    virtual bool ensureParsedDataReady() OVERRIDE;
    virtual PassRefPtrWillBeRawPtr<CSSRuleSourceData> ruleSourceDataAt(unsigned ruleIndex) const OVERRIDE { ASSERT_UNUSED(ruleIndex, !ruleIndex); return m_ruleSourceData; }

private:
    InspectorStyleSheetForInlineStyle(const String& id, PassRefPtrWillBeRawPtr<Element>, Listener*);
    CSSStyleDeclaration* inlineStyle() const;
    const String& elementStyleText() const;
    PassRefPtrWillBeRawPtr<CSSRuleSourceData> getStyleAttributeData() const;

    RefPtrWillBePersistent<Element> m_element;
    RefPtrWillBePersistent<CSSRuleSourceData> m_ruleSourceData;
    RefPtr<InspectorStyle> m_inspectorStyle;

    // Contains "style" attribute value.
    mutable String m_styleText;
    mutable bool m_isStyleTextValid;
};


} // namespace WebCore

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(WebCore::InspectorStyleProperty);

#endif // !defined(InspectorStyleSheet_h)
