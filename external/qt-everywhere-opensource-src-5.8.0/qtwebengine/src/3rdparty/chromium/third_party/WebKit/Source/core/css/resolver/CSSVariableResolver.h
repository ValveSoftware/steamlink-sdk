// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSVariableResolver_h
#define CSSVariableResolver_h

#include "core/CSSPropertyNames.h"
#include "core/css/parser/CSSParserToken.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class CSSParserTokenRange;
class CSSPendingSubstitutionValue;
class CSSVariableData;
class CSSVariableReferenceValue;
class StyleResolverState;
class StyleVariableData;

class CSSVariableResolver {
public:
    static void resolveVariableDefinitions(StyleVariableData*);
    static const CSSValue* resolveVariableReferences(StyleResolverState&, CSSPropertyID, const CSSVariableReferenceValue&);
    static const CSSValue* resolvePendingSubstitutions(StyleResolverState&, CSSPropertyID, const CSSPendingSubstitutionValue&);

    // Shorthand properties are not supported.
    static CSSValue* resolveVariableReferences(StyleVariableData*, CSSPropertyID, const CSSVariableReferenceValue&);

private:
    CSSVariableResolver(StyleVariableData*);

    // These return false if we encounter a reference to an invalid variable with no fallback

    // Resolves a range which may contain var() references or @apply rules
    bool resolveTokenRange(CSSParserTokenRange, Vector<CSSParserToken>& result);
    // Resolves the fallback (if present) of a var() reference, starting from the comma
    bool resolveFallback(CSSParserTokenRange, Vector<CSSParserToken>& result);
    // Resolves the contents of a var() reference
    bool resolveVariableReference(CSSParserTokenRange, Vector<CSSParserToken>& result);
    // Consumes and resolves an @apply rule
    void resolveApplyAtRule(CSSParserTokenRange&, Vector<CSSParserToken>& result);

    // These return null if the custom property is invalid

    // Returns the CSSVariableData for a custom property, resolving and storing it if necessary
    CSSVariableData* valueForCustomProperty(AtomicString name);
    // Resolves the CSSVariableData from a custom property declaration
    PassRefPtr<CSSVariableData> resolveCustomProperty(AtomicString name, const CSSVariableData&);

    StyleVariableData* m_styleVariableData;
    HashSet<AtomicString> m_variablesSeen;
    // Resolution doesn't finish when a cycle is detected. Fallbacks still
    // need to be tracked for additional cycles, and invalidation only
    // applies back to cycle starts.
    HashSet<AtomicString> m_cycleStartPoints;
};

} // namespace blink

#endif // CSSVariableResolver
