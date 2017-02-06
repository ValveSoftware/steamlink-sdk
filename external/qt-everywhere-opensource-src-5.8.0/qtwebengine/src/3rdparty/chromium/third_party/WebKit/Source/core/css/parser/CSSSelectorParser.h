// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSelectorParser_h
#define CSSSelectorParser_h

#include "core/CoreExport.h"
#include "core/css/parser/CSSParserSelector.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include <memory>

namespace blink {

class CSSSelectorList;
class StyleSheetContents;

// FIXME: We should consider building CSSSelectors directly instead of using
// the intermediate CSSParserSelector.
class CORE_EXPORT CSSSelectorParser {
    STACK_ALLOCATED();
public:
    static CSSSelectorList parseSelector(CSSParserTokenRange, const CSSParserContext&, StyleSheetContents*);

    static bool consumeANPlusB(CSSParserTokenRange&, std::pair<int, int>&);

private:
    CSSSelectorParser(const CSSParserContext&, StyleSheetContents*);

    // These will all consume trailing comments if successful

    CSSSelectorList consumeComplexSelectorList(CSSParserTokenRange&);
    CSSSelectorList consumeCompoundSelectorList(CSSParserTokenRange&);

    std::unique_ptr<CSSParserSelector> consumeComplexSelector(CSSParserTokenRange&);
    std::unique_ptr<CSSParserSelector> consumeCompoundSelector(CSSParserTokenRange&);
    // This doesn't include element names, since they're handled specially
    std::unique_ptr<CSSParserSelector> consumeSimpleSelector(CSSParserTokenRange&);

    bool consumeName(CSSParserTokenRange&, AtomicString& name, AtomicString& namespacePrefix);

    // These will return nullptr when the selector is invalid
    std::unique_ptr<CSSParserSelector> consumeId(CSSParserTokenRange&);
    std::unique_ptr<CSSParserSelector> consumeClass(CSSParserTokenRange&);
    std::unique_ptr<CSSParserSelector> consumePseudo(CSSParserTokenRange&);
    std::unique_ptr<CSSParserSelector> consumeAttribute(CSSParserTokenRange&);

    CSSSelector::RelationType consumeCombinator(CSSParserTokenRange&);
    CSSSelector::MatchType consumeAttributeMatch(CSSParserTokenRange&);
    CSSSelector::AttributeMatchType consumeAttributeFlags(CSSParserTokenRange&);

    const AtomicString& defaultNamespace() const;
    const AtomicString& determineNamespace(const AtomicString& prefix);
    void prependTypeSelectorIfNeeded(const AtomicString& namespacePrefix, const AtomicString& elementName, CSSParserSelector*);
    static std::unique_ptr<CSSParserSelector> addSimpleSelectorToCompound(std::unique_ptr<CSSParserSelector> compoundSelector, std::unique_ptr<CSSParserSelector> simpleSelector);
    static std::unique_ptr<CSSParserSelector> splitCompoundAtImplicitShadowCrossingCombinator(std::unique_ptr<CSSParserSelector> compoundSelector);

    const CSSParserContext& m_context;
    Member<StyleSheetContents> m_styleSheet; // FIXME: Should be const

    bool m_failedParsing = false;
    bool m_disallowPseudoElements = false;

    class DisallowPseudoElementsScope {
        STACK_ALLOCATED();
        WTF_MAKE_NONCOPYABLE(DisallowPseudoElementsScope);
    public:
        DisallowPseudoElementsScope(CSSSelectorParser* parser)
            : m_parser(parser), m_wasDisallowed(m_parser->m_disallowPseudoElements)
        {
            m_parser->m_disallowPseudoElements = true;
        }

        ~DisallowPseudoElementsScope()
        {
            m_parser->m_disallowPseudoElements = m_wasDisallowed;
        }

    private:
        CSSSelectorParser* m_parser;
        bool m_wasDisallowed;
    };
};

} // namespace blink

#endif // CSSSelectorParser_h
