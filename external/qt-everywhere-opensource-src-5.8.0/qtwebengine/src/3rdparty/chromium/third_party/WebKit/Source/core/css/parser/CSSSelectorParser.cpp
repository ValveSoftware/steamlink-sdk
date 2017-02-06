// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSSelectorParser.h"

#include "core/css/CSSSelectorList.h"
#include "core/css/StyleSheetContents.h"
#include "core/frame/UseCounter.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

static void recordSelectorStats(const CSSParserContext& context, const CSSSelectorList& selectorList)
{
    if (!context.useCounter())
        return;

    for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(*selector)) {
        for (const CSSSelector* current = selector; current ; current = current->tagHistory()) {
            UseCounter::Feature feature = UseCounter::NumberOfFeatures;
            switch (current->getPseudoType()) {
            case CSSSelector::PseudoAny:
                feature = UseCounter::CSSSelectorPseudoAny;
                break;
            case CSSSelector::PseudoUnresolved:
                feature = UseCounter::CSSSelectorPseudoUnresolved;
                break;
            case CSSSelector::PseudoDefined:
                feature = UseCounter::CSSSelectorPseudoDefined;
                break;
            case CSSSelector::PseudoSlotted:
                feature = UseCounter::CSSSelectorPseudoSlotted;
                break;
            case CSSSelector::PseudoContent:
                feature = UseCounter::CSSSelectorPseudoContent;
                break;
            case CSSSelector::PseudoHost:
                feature = UseCounter::CSSSelectorPseudoHost;
                break;
            case CSSSelector::PseudoHostContext:
                feature = UseCounter::CSSSelectorPseudoHostContext;
                break;
            case CSSSelector::PseudoFullScreenAncestor:
                feature = UseCounter::CSSSelectorPseudoFullScreenAncestor;
                break;
            case CSSSelector::PseudoFullScreen:
                feature = UseCounter::CSSSelectorPseudoFullScreen;
                break;
            case CSSSelector::PseudoListBox:
                if (context.mode() != UASheetMode)
                    feature = UseCounter::CSSSelectorInternalPseudoListBox;
                break;
            case CSSSelector::PseudoWebKitCustomElement:
                if (context.mode() != UASheetMode) {
                    if (current->value() == "-internal-media-controls-cast-button")
                        feature = UseCounter::CSSSelectorInternalMediaControlsCastButton;
                    else if (current->value() == "-internal-media-controls-overlay-cast-button")
                        feature = UseCounter::CSSSelectorInternalMediaControlsOverlayCastButton;
                }
                break;
            case CSSSelector::PseudoSpatialNavigationFocus:
                if (context.mode() != UASheetMode)
                    feature = UseCounter::CSSSelectorInternalPseudoSpatialNavigationFocus;
                break;
            case CSSSelector::PseudoReadOnly:
                if (context.mode() != UASheetMode)
                    feature = UseCounter::CSSSelectorPseudoReadOnly;
                break;
            case CSSSelector::PseudoReadWrite:
                if (context.mode() != UASheetMode)
                    feature = UseCounter::CSSSelectorPseudoReadWrite;
                break;
            default:
                break;
            }
            if (feature != UseCounter::NumberOfFeatures)
                context.useCounter()->count(feature);
            if (current->relation() == CSSSelector::IndirectAdjacent)
                context.useCounter()->count(UseCounter::CSSSelectorIndirectAdjacent);
            if (current->selectorList())
                recordSelectorStats(context, *current->selectorList());
        }
    }
}

CSSSelectorList CSSSelectorParser::parseSelector(CSSParserTokenRange range, const CSSParserContext& context, StyleSheetContents* styleSheet)
{
    CSSSelectorParser parser(context, styleSheet);
    range.consumeWhitespace();
    CSSSelectorList result = parser.consumeComplexSelectorList(range);
    if (!range.atEnd())
        return CSSSelectorList();

    recordSelectorStats(context, result);
    return result;
}

CSSSelectorParser::CSSSelectorParser(const CSSParserContext& context, StyleSheetContents* styleSheet)
    : m_context(context)
    , m_styleSheet(styleSheet)
{
}

CSSSelectorList CSSSelectorParser::consumeComplexSelectorList(CSSParserTokenRange& range)
{
    Vector<std::unique_ptr<CSSParserSelector>> selectorList;
    std::unique_ptr<CSSParserSelector> selector = consumeComplexSelector(range);
    if (!selector)
        return CSSSelectorList();
    selectorList.append(std::move(selector));
    while (!range.atEnd() && range.peek().type() == CommaToken) {
        range.consumeIncludingWhitespace();
        selector = consumeComplexSelector(range);
        if (!selector)
            return CSSSelectorList();
        selectorList.append(std::move(selector));
    }

    if (m_failedParsing)
        return CSSSelectorList();

    return CSSSelectorList::adoptSelectorVector(selectorList);
}

CSSSelectorList CSSSelectorParser::consumeCompoundSelectorList(CSSParserTokenRange& range)
{
    Vector<std::unique_ptr<CSSParserSelector>> selectorList;
    std::unique_ptr<CSSParserSelector> selector = consumeCompoundSelector(range);
    range.consumeWhitespace();
    if (!selector)
        return CSSSelectorList();
    selectorList.append(std::move(selector));
    while (!range.atEnd() && range.peek().type() == CommaToken) {
        range.consumeIncludingWhitespace();
        selector = consumeCompoundSelector(range);
        range.consumeWhitespace();
        if (!selector)
            return CSSSelectorList();
        selectorList.append(std::move(selector));
    }

    if (m_failedParsing)
        return CSSSelectorList();

    return CSSSelectorList::adoptSelectorVector(selectorList);
}

namespace {

enum CompoundSelectorFlags {
    HasPseudoElementForRightmostCompound = 1 << 0,
    HasContentPseudoElement = 1 << 1
};

unsigned extractCompoundFlags(const CSSParserSelector& simpleSelector, CSSParserMode parserMode)
{
    if (simpleSelector.match() != CSSSelector::PseudoElement)
        return 0;
    if (simpleSelector.pseudoType() == CSSSelector::PseudoContent)
        return HasContentPseudoElement;
    if (simpleSelector.pseudoType() == CSSSelector::PseudoShadow)
        return 0;
    // TODO(rune@opera.com): crbug.com/578131
    // The UASheetMode check is a work-around to allow this selector in mediaControls(New).css:
    // input[type="range" i]::-webkit-media-slider-container > div {
    if (parserMode == UASheetMode && simpleSelector.pseudoType() == CSSSelector::PseudoWebKitCustomElement)
        return 0;
    return HasPseudoElementForRightmostCompound;
}

} // namespace

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeComplexSelector(CSSParserTokenRange& range)
{
    std::unique_ptr<CSSParserSelector> selector = consumeCompoundSelector(range);
    if (!selector)
        return nullptr;


    unsigned previousCompoundFlags = 0;

    for (CSSParserSelector* simple = selector.get(); simple && !previousCompoundFlags; simple = simple->tagHistory())
        previousCompoundFlags |= extractCompoundFlags(*simple, m_context.mode());

    while (CSSSelector::RelationType combinator = consumeCombinator(range)) {
        std::unique_ptr<CSSParserSelector> nextSelector = consumeCompoundSelector(range);
        if (!nextSelector)
            return combinator == CSSSelector::Descendant ? std::move(selector) : nullptr;
        if (previousCompoundFlags & HasPseudoElementForRightmostCompound)
            return nullptr;
        CSSParserSelector* end = nextSelector.get();
        unsigned compoundFlags = extractCompoundFlags(*end, m_context.mode());
        while (end->tagHistory()) {
            end = end->tagHistory();
            compoundFlags |= extractCompoundFlags(*end, m_context.mode());
        }
        end->setRelation(combinator);
        if (previousCompoundFlags & HasContentPseudoElement)
            end->setRelationIsAffectedByPseudoContent();
        previousCompoundFlags = compoundFlags;
        end->setTagHistory(std::move(selector));

        selector = std::move(nextSelector);
    }

    return selector;
}

namespace {

bool isScrollbarPseudoClass(CSSSelector::PseudoType pseudo)
{
    switch (pseudo) {
    case CSSSelector::PseudoEnabled:
    case CSSSelector::PseudoDisabled:
    case CSSSelector::PseudoHover:
    case CSSSelector::PseudoActive:
    case CSSSelector::PseudoHorizontal:
    case CSSSelector::PseudoVertical:
    case CSSSelector::PseudoDecrement:
    case CSSSelector::PseudoIncrement:
    case CSSSelector::PseudoStart:
    case CSSSelector::PseudoEnd:
    case CSSSelector::PseudoDoubleButton:
    case CSSSelector::PseudoSingleButton:
    case CSSSelector::PseudoNoButton:
    case CSSSelector::PseudoCornerPresent:
    case CSSSelector::PseudoWindowInactive:
        return true;
    default:
        return false;
    }
}

bool isUserActionPseudoClass(CSSSelector::PseudoType pseudo)
{
    switch (pseudo) {
    case CSSSelector::PseudoHover:
    case CSSSelector::PseudoFocus:
    case CSSSelector::PseudoActive:
        return true;
    default:
        return false;
    }
}

bool isPseudoClassValidAfterPseudoElement(CSSSelector::PseudoType pseudoClass, CSSSelector::PseudoType compoundPseudoElement)
{
    switch (compoundPseudoElement) {
    case CSSSelector::PseudoResizer:
    case CSSSelector::PseudoScrollbar:
    case CSSSelector::PseudoScrollbarCorner:
    case CSSSelector::PseudoScrollbarButton:
    case CSSSelector::PseudoScrollbarThumb:
    case CSSSelector::PseudoScrollbarTrack:
    case CSSSelector::PseudoScrollbarTrackPiece:
        return isScrollbarPseudoClass(pseudoClass);
    case CSSSelector::PseudoSelection:
        return pseudoClass == CSSSelector::PseudoWindowInactive;
    case CSSSelector::PseudoWebKitCustomElement:
    case CSSSelector::PseudoBlinkInternalElement:
        return isUserActionPseudoClass(pseudoClass);
    default:
        return false;
    }
}

bool isSimpleSelectorValidAfterPseudoElement(const CSSParserSelector& simpleSelector, CSSSelector::PseudoType compoundPseudoElement)
{
    if (compoundPseudoElement == CSSSelector::PseudoUnknown)
        return true;
    if (compoundPseudoElement == CSSSelector::PseudoContent)
        return simpleSelector.match() != CSSSelector::PseudoElement;
    if (simpleSelector.match() != CSSSelector::PseudoClass)
        return false;
    CSSSelector::PseudoType pseudo = simpleSelector.pseudoType();
    if (pseudo == CSSSelector::PseudoNot) {
        ASSERT(simpleSelector.selectorList());
        ASSERT(simpleSelector.selectorList()->first());
        ASSERT(!simpleSelector.selectorList()->first()->tagHistory());
        pseudo = simpleSelector.selectorList()->first()->getPseudoType();
    }
    return isPseudoClassValidAfterPseudoElement(pseudo, compoundPseudoElement);
}

} // namespace

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeCompoundSelector(CSSParserTokenRange& range)
{
    std::unique_ptr<CSSParserSelector> compoundSelector;

    AtomicString namespacePrefix;
    AtomicString elementName;
    CSSSelector::PseudoType compoundPseudoElement = CSSSelector::PseudoUnknown;
    if (!consumeName(range, elementName, namespacePrefix)) {
        compoundSelector = consumeSimpleSelector(range);
        if (!compoundSelector)
            return nullptr;
        if (compoundSelector->match() == CSSSelector::PseudoElement)
            compoundPseudoElement = compoundSelector->pseudoType();
    }
    if (m_context.isHTMLDocument())
        elementName = elementName.lower();

    while (std::unique_ptr<CSSParserSelector> simpleSelector = consumeSimpleSelector(range)) {
        // TODO(rune@opera.com): crbug.com/578131
        // The UASheetMode check is a work-around to allow this selector in mediaControls(New).css:
        // video::-webkit-media-text-track-region-container.scrolling
        if (m_context.mode() != UASheetMode && !isSimpleSelectorValidAfterPseudoElement(*simpleSelector.get(), compoundPseudoElement)) {
            m_failedParsing = true;
            return nullptr;
        }
        if (simpleSelector->match() == CSSSelector::PseudoElement)
            compoundPseudoElement = simpleSelector->pseudoType();

        if (compoundSelector)
            compoundSelector = addSimpleSelectorToCompound(std::move(compoundSelector), std::move(simpleSelector));
        else
            compoundSelector = std::move(simpleSelector);
    }

    if (!compoundSelector) {
        AtomicString namespaceURI = determineNamespace(namespacePrefix);
        if (namespaceURI.isNull()) {
            m_failedParsing = true;
            return nullptr;
        }
        if (namespaceURI == defaultNamespace())
            namespacePrefix = nullAtom;
        return CSSParserSelector::create(QualifiedName(namespacePrefix, elementName, namespaceURI));
    }
    prependTypeSelectorIfNeeded(namespacePrefix, elementName, compoundSelector.get());
    return splitCompoundAtImplicitShadowCrossingCombinator(std::move(compoundSelector));
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeSimpleSelector(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    std::unique_ptr<CSSParserSelector> selector;
    if (token.type() == HashToken)
        selector = consumeId(range);
    else if (token.type() == DelimiterToken && token.delimiter() == '.')
        selector = consumeClass(range);
    else if (token.type() == LeftBracketToken)
        selector = consumeAttribute(range);
    else if (token.type() == ColonToken)
        selector = consumePseudo(range);
    else
        return nullptr;
    if (!selector)
        m_failedParsing = true;
    return selector;
}

bool CSSSelectorParser::consumeName(CSSParserTokenRange& range, AtomicString& name, AtomicString& namespacePrefix)
{
    name = nullAtom;
    namespacePrefix = nullAtom;

    const CSSParserToken& firstToken = range.peek();
    if (firstToken.type() == IdentToken) {
        name = firstToken.value().toAtomicString();
        range.consume();
    } else if (firstToken.type() == DelimiterToken && firstToken.delimiter() == '*') {
        name = starAtom;
        range.consume();
    } else if (firstToken.type() == DelimiterToken && firstToken.delimiter() == '|') {
        // This is an empty namespace, which'll get assigned this value below
        name = emptyAtom;
    } else {
        return false;
    }

    if (range.peek().type() != DelimiterToken || range.peek().delimiter() != '|')
        return true;
    range.consume();

    namespacePrefix = name;
    const CSSParserToken& nameToken = range.consume();
    if (nameToken.type() == IdentToken) {
        name = nameToken.value().toAtomicString();
    } else if (nameToken.type() == DelimiterToken && nameToken.delimiter() == '*') {
        name = starAtom;
    } else {
        name = nullAtom;
        namespacePrefix = nullAtom;
        return false;
    }

    return true;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeId(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == HashToken);
    if (range.peek().getHashTokenType() != HashTokenId)
        return nullptr;
    std::unique_ptr<CSSParserSelector> selector = CSSParserSelector::create();
    selector->setMatch(CSSSelector::Id);
    AtomicString value = range.consume().value().toAtomicString();
    selector->setValue(value, isQuirksModeBehavior(m_context.matchMode()));
    return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeClass(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == DelimiterToken);
    ASSERT(range.peek().delimiter() == '.');
    range.consume();
    if (range.peek().type() != IdentToken)
        return nullptr;
    std::unique_ptr<CSSParserSelector> selector = CSSParserSelector::create();
    selector->setMatch(CSSSelector::Class);
    AtomicString value = range.consume().value().toAtomicString();
    selector->setValue(value, isQuirksModeBehavior(m_context.matchMode()));
    return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeAttribute(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == LeftBracketToken);
    CSSParserTokenRange block = range.consumeBlock();
    block.consumeWhitespace();

    AtomicString namespacePrefix;
    AtomicString attributeName;
    if (!consumeName(block, attributeName, namespacePrefix))
        return nullptr;
    block.consumeWhitespace();

    if (m_context.isHTMLDocument())
        attributeName = attributeName.lower();

    AtomicString namespaceURI = determineNamespace(namespacePrefix);
    if (namespaceURI.isNull())
        return nullptr;

    QualifiedName qualifiedName = namespacePrefix.isNull()
        ? QualifiedName(nullAtom, attributeName, nullAtom)
        : QualifiedName(namespacePrefix, attributeName, namespaceURI);

    std::unique_ptr<CSSParserSelector> selector = CSSParserSelector::create();

    if (block.atEnd()) {
        selector->setAttribute(qualifiedName, CSSSelector::CaseSensitive);
        selector->setMatch(CSSSelector::AttributeSet);
        return selector;
    }

    selector->setMatch(consumeAttributeMatch(block));

    const CSSParserToken& attributeValue = block.consumeIncludingWhitespace();
    if (attributeValue.type() != IdentToken && attributeValue.type() != StringToken)
        return nullptr;
    selector->setValue(attributeValue.value().toAtomicString());
    selector->setAttribute(qualifiedName, consumeAttributeFlags(block));

    if (!block.atEnd())
        return nullptr;
    return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumePseudo(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == ColonToken);
    range.consume();

    int colons = 1;
    if (range.peek().type() == ColonToken) {
        range.consume();
        colons++;
    }

    const CSSParserToken& token = range.peek();
    if (token.type() != IdentToken && token.type() != FunctionToken)
        return nullptr;

    std::unique_ptr<CSSParserSelector> selector = CSSParserSelector::create();
    selector->setMatch(colons == 1 ? CSSSelector::PseudoClass : CSSSelector::PseudoElement);

    String value = token.value().toString();
    bool hasArguments = token.type() == FunctionToken;
    selector->updatePseudoType(AtomicString(value.is8Bit() ? value.lower() : value), hasArguments);

    if (selector->match() == CSSSelector::PseudoElement && m_disallowPseudoElements)
        return nullptr;

    if (token.type() == IdentToken) {
        range.consume();
        if (selector->pseudoType() == CSSSelector::PseudoUnknown)
            return nullptr;
        return selector;
    }

    CSSParserTokenRange block = range.consumeBlock();
    block.consumeWhitespace();
    if (selector->pseudoType() == CSSSelector::PseudoUnknown)
        return nullptr;

    switch (selector->pseudoType()) {
    case CSSSelector::PseudoHost:
    case CSSSelector::PseudoHostContext:
    case CSSSelector::PseudoAny:
    case CSSSelector::PseudoCue:
        {
            DisallowPseudoElementsScope scope(this);

            std::unique_ptr<CSSSelectorList> selectorList = wrapUnique(new CSSSelectorList());
            *selectorList = consumeCompoundSelectorList(block);
            if (!selectorList->isValid() || !block.atEnd())
                return nullptr;
            selector->setSelectorList(std::move(selectorList));
            return selector;
        }
    case CSSSelector::PseudoNot:
        {
            std::unique_ptr<CSSParserSelector> innerSelector = consumeCompoundSelector(block);
            block.consumeWhitespace();
            if (!innerSelector || !innerSelector->isSimple() || !block.atEnd())
                return nullptr;
            Vector<std::unique_ptr<CSSParserSelector>> selectorVector;
            selectorVector.append(std::move(innerSelector));
            selector->adoptSelectorVector(selectorVector);
            return selector;
        }
    case CSSSelector::PseudoSlotted:
        {
            DisallowPseudoElementsScope scope(this);

            std::unique_ptr<CSSParserSelector> innerSelector = consumeCompoundSelector(block);
            block.consumeWhitespace();
            if (!innerSelector || !block.atEnd() || !RuntimeEnabledFeatures::shadowDOMV1Enabled())
                return nullptr;
            Vector<std::unique_ptr<CSSParserSelector>> selectorVector;
            selectorVector.append(std::move(innerSelector));
            selector->adoptSelectorVector(selectorVector);
            return selector;
        }
    case CSSSelector::PseudoLang:
        {
            // FIXME: CSS Selectors Level 4 allows :lang(*-foo)
            const CSSParserToken& ident = block.consumeIncludingWhitespace();
            if (ident.type() != IdentToken || !block.atEnd())
                return nullptr;
            selector->setArgument(ident.value().toAtomicString());
            return selector;
        }
    case CSSSelector::PseudoNthChild:
    case CSSSelector::PseudoNthLastChild:
    case CSSSelector::PseudoNthOfType:
    case CSSSelector::PseudoNthLastOfType:
        {
            std::pair<int, int> ab;
            if (!consumeANPlusB(block, ab))
                return nullptr;
            block.consumeWhitespace();
            if (!block.atEnd())
                return nullptr;
            selector->setNth(ab.first, ab.second);
            return selector;
        }
    default:
        break;
    }

    return nullptr;
}

CSSSelector::RelationType CSSSelectorParser::consumeCombinator(CSSParserTokenRange& range)
{
    CSSSelector::RelationType fallbackResult = CSSSelector::SubSelector;
    while (range.peek().type() == WhitespaceToken) {
        range.consume();
        fallbackResult = CSSSelector::Descendant;
    }

    if (range.peek().type() != DelimiterToken)
        return fallbackResult;

    UChar delimiter = range.peek().delimiter();

    if (delimiter == '+' || delimiter == '~' || delimiter == '>') {
        range.consumeIncludingWhitespace();
        if (delimiter == '+')
            return CSSSelector::DirectAdjacent;
        if (delimiter == '~')
            return CSSSelector::IndirectAdjacent;
        return CSSSelector::Child;
    }

    // Match /deep/
    if (delimiter != '/')
        return fallbackResult;
    range.consume();
    const CSSParserToken& ident = range.consume();
    if (ident.type() != IdentToken || !equalIgnoringASCIICase(ident.value(), "deep"))
        m_failedParsing = true;
    const CSSParserToken& slash = range.consumeIncludingWhitespace();
    if (slash.type() != DelimiterToken || slash.delimiter() != '/')
        m_failedParsing = true;
    return CSSSelector::ShadowDeep;
}

CSSSelector::MatchType CSSSelectorParser::consumeAttributeMatch(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.consumeIncludingWhitespace();
    switch (token.type()) {
    case IncludeMatchToken:
        return CSSSelector::AttributeList;
    case DashMatchToken:
        return CSSSelector::AttributeHyphen;
    case PrefixMatchToken:
        return CSSSelector::AttributeBegin;
    case SuffixMatchToken:
        return CSSSelector::AttributeEnd;
    case SubstringMatchToken:
        return CSSSelector::AttributeContain;
    case DelimiterToken:
        if (token.delimiter() == '=')
            return CSSSelector::AttributeExact;
    default:
        m_failedParsing = true;
        return CSSSelector::AttributeExact;
    }
}

CSSSelector::AttributeMatchType CSSSelectorParser::consumeAttributeFlags(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return CSSSelector::CaseSensitive;
    const CSSParserToken& flag = range.consumeIncludingWhitespace();
    if (equalIgnoringASCIICase(flag.value(), "i"))
        return CSSSelector::CaseInsensitive;
    m_failedParsing = true;
    return CSSSelector::CaseSensitive;
}

bool CSSSelectorParser::consumeANPlusB(CSSParserTokenRange& range, std::pair<int, int>& result)
{
    const CSSParserToken& token = range.consume();
    if (token.type() == NumberToken && token.numericValueType() == IntegerValueType) {
        result = std::make_pair(0, static_cast<int>(token.numericValue()));
        return true;
    }
    if (token.type() == IdentToken) {
        if (equalIgnoringASCIICase(token.value(), "odd")) {
            result = std::make_pair(2, 1);
            return true;
        }
        if (equalIgnoringASCIICase(token.value(), "even")) {
            result = std::make_pair(2, 0);
            return true;
        }
    }

    // The 'n' will end up as part of an ident or dimension. For a valid <an+b>,
    // this will store a string of the form 'n', 'n-', or 'n-123'.
    String nString;

    if (token.type() == DelimiterToken && token.delimiter() == '+' && range.peek().type() == IdentToken) {
        result.first = 1;
        nString = range.consume().value().toString();
    } else if (token.type() == DimensionToken && token.numericValueType() == IntegerValueType) {
        result.first = token.numericValue();
        nString = token.value().toString();
    } else if (token.type() == IdentToken) {
        if (token.value()[0] == '-') {
            result.first = -1;
            nString = token.value().toString().substring(1);
        } else {
            result.first = 1;
            nString = token.value().toString();
        }
    }

    range.consumeWhitespace();

    if (nString.isEmpty() || !isASCIIAlphaCaselessEqual(nString[0], 'n'))
        return false;
    if (nString.length() > 1 && nString[1] != '-')
        return false;

    if (nString.length() > 2) {
        bool valid;
        result.second = nString.substring(1).toIntStrict(&valid);
        return valid;
    }

    NumericSign sign = nString.length() == 1 ? NoSign : MinusSign;
    if (sign == NoSign && range.peek().type() == DelimiterToken) {
        char delimiterSign = range.consumeIncludingWhitespace().delimiter();
        if (delimiterSign == '+')
            sign = PlusSign;
        else if (delimiterSign == '-')
            sign = MinusSign;
        else
            return false;
    }

    if (sign == NoSign && range.peek().type() != NumberToken) {
        result.second = 0;
        return true;
    }

    const CSSParserToken& b = range.consume();
    if (b.type() != NumberToken || b.numericValueType() != IntegerValueType)
        return false;
    if ((b.numericSign() == NoSign) == (sign == NoSign))
        return false;
    result.second = b.numericValue();
    if (sign == MinusSign)
        result.second = -result.second;
    return true;
}

const AtomicString& CSSSelectorParser::defaultNamespace() const
{
    if (!m_styleSheet)
        return starAtom;
    return m_styleSheet->defaultNamespace();
}

const AtomicString& CSSSelectorParser::determineNamespace(const AtomicString& prefix)
{
    if (prefix.isNull())
        return defaultNamespace();
    if (prefix.isEmpty())
        return emptyAtom; // No namespace. If an element/attribute has a namespace, we won't match it.
    if (prefix == starAtom)
        return starAtom; // We'll match any namespace.
    if (!m_styleSheet)
        return nullAtom; // Cannot resolve prefix to namespace without a stylesheet, syntax error.
    return m_styleSheet->namespaceURIFromPrefix(prefix);
}

void CSSSelectorParser::prependTypeSelectorIfNeeded(const AtomicString& namespacePrefix, const AtomicString& elementName, CSSParserSelector* compoundSelector)
{
    if (elementName.isNull() && defaultNamespace() == starAtom && !compoundSelector->needsImplicitShadowCombinatorForMatching())
        return;

    AtomicString determinedElementName = elementName.isNull() ? starAtom : elementName;
    AtomicString namespaceURI = determineNamespace(namespacePrefix);
    if (namespaceURI.isNull()) {
        m_failedParsing = true;
        return;
    }
    AtomicString determinedPrefix = namespacePrefix;
    if (namespaceURI == defaultNamespace())
        determinedPrefix = nullAtom;
    QualifiedName tag = QualifiedName(determinedPrefix, determinedElementName, namespaceURI);

    // *:host/*:host-context never matches, so we can't discard the *,
    // otherwise we can't tell the difference between *:host and just :host.
    //
    // Also, selectors where we use a ShadowPseudo combinator between the
    // element and the pseudo element for matching (custom pseudo elements,
    // ::cue, ::shadow), we need a universal selector to set the combinator
    // (relation) on in the cases where there are no simple selectors preceding
    // the pseudo element.
    bool explicitForHost = compoundSelector->isHostPseudoSelector() && !elementName.isNull();
    if (tag != anyQName() || explicitForHost || compoundSelector->needsImplicitShadowCombinatorForMatching())
        compoundSelector->prependTagSelector(tag, determinedPrefix == nullAtom && determinedElementName == starAtom && !explicitForHost);
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::addSimpleSelectorToCompound(std::unique_ptr<CSSParserSelector> compoundSelector, std::unique_ptr<CSSParserSelector> simpleSelector)
{
    compoundSelector->appendTagHistory(CSSSelector::SubSelector, std::move(simpleSelector));
    return compoundSelector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::splitCompoundAtImplicitShadowCrossingCombinator(std::unique_ptr<CSSParserSelector> compoundSelector)
{
    // The tagHistory is a linked list that stores combinator separated compound selectors
    // from right-to-left. Yet, within a single compound selector, stores the simple selectors
    // from left-to-right.
    //
    // ".a.b > div#id" is stored in a tagHistory as [div, #id, .a, .b], each element in the
    // list stored with an associated relation (combinator or SubSelector).
    //
    // ::cue, ::shadow, and custom pseudo elements have an implicit ShadowPseudo combinator
    // to their left, which really makes for a new compound selector, yet it's consumed by
    // the selector parser as a single compound selector.
    //
    // Example: input#x::-webkit-clear-button -> [ ::-webkit-clear-button, input, #x ]
    //
    // Likewise, ::slotted() pseudo element has an implicit ShadowSlot combinator to its left
    // for finding matching slot element in other TreeScope.
    //
    // Example: slot[name=foo]::slotted(div) -> [ ::slotted(div), slot, [name=foo] ]
    CSSParserSelector* splitAfter = compoundSelector.get();

    while (splitAfter->tagHistory() && !splitAfter->tagHistory()->needsImplicitShadowCombinatorForMatching())
        splitAfter = splitAfter->tagHistory();

    if (!splitAfter || !splitAfter->tagHistory())
        return compoundSelector;

    std::unique_ptr<CSSParserSelector> secondCompound = splitAfter->releaseTagHistory();
    secondCompound->appendTagHistory(secondCompound->pseudoType() == CSSSelector::PseudoSlotted ? CSSSelector::ShadowSlot : CSSSelector::ShadowPseudo, std::move(compoundSelector));
    return secondCompound;
}

} // namespace blink
