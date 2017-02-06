// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyParserHelpers_h
#define CSSPropertyParserHelpers_h

#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/Length.h" // For ValueRange
#include "platform/heap/Handle.h"

namespace blink {

class CSSStringValue;
class CSSValuePair;

// When these functions are successful, they will consume all the relevant
// tokens from the range and also consume any whitespace which follows. When
// the start of the range doesn't match the type we're looking for, the range
// will not be modified.
namespace CSSPropertyParserHelpers {

// TODO(timloh): These should probably just be consumeComma and consumeSlash.
bool consumeCommaIncludingWhitespace(CSSParserTokenRange&);
bool consumeSlashIncludingWhitespace(CSSParserTokenRange&);
// consumeFunction expects the range starts with a FunctionToken.
CSSParserTokenRange consumeFunction(CSSParserTokenRange&);

enum class UnitlessQuirk {
    Allow,
    Forbid
};

CSSPrimitiveValue* consumeInteger(CSSParserTokenRange&, double minimumValue = -std::numeric_limits<double>::max());
CSSPrimitiveValue* consumePositiveInteger(CSSParserTokenRange&);
bool consumeNumberRaw(CSSParserTokenRange&, double& result);
CSSPrimitiveValue* consumeNumber(CSSParserTokenRange&, ValueRange);
CSSPrimitiveValue* consumeLength(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid);
CSSPrimitiveValue* consumePercent(CSSParserTokenRange&, ValueRange);
CSSPrimitiveValue* consumeLengthOrPercent(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid);
CSSPrimitiveValue* consumeAngle(CSSParserTokenRange&);
CSSPrimitiveValue* consumeTime(CSSParserTokenRange&, ValueRange);

CSSPrimitiveValue* consumeIdent(CSSParserTokenRange&);
CSSPrimitiveValue* consumeIdentRange(CSSParserTokenRange&, CSSValueID lower, CSSValueID upper);
template<CSSValueID, CSSValueID...> inline bool identMatches(CSSValueID id);
template<CSSValueID... allowedIdents> CSSPrimitiveValue* consumeIdent(CSSParserTokenRange&);

CSSCustomIdentValue* consumeCustomIdent(CSSParserTokenRange&);
CSSStringValue* consumeString(CSSParserTokenRange&);
String consumeUrl(CSSParserTokenRange&);

CSSValue* consumeColor(CSSParserTokenRange&, CSSParserMode, bool acceptQuirkyColors = false);

CSSValuePair* consumePosition(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk);
bool consumePosition(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk, CSSValue*& resultX, CSSValue*& resultY);
bool consumeOneOrTwoValuedPosition(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk, CSSValue*& resultX, CSSValue*& resultY);

// TODO(timloh): Move across consumeImage

// Template implementations are at the bottom of the file for readability.

template<typename... emptyBaseCase> inline bool identMatches(CSSValueID id) { return false; }
template<CSSValueID head, CSSValueID... tail> inline bool identMatches(CSSValueID id)
{
    return id == head || identMatches<tail...>(id);
}

template<CSSValueID... names> CSSPrimitiveValue* consumeIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken || !identMatches<names...>(range.peek().id()))
        return nullptr;
    return CSSPrimitiveValue::createIdentifier(range.consumeIncludingWhitespace().id());
}

static inline bool isCSSWideKeyword(const CSSValueID& id)
{
    return id == CSSValueInitial || id == CSSValueInherit || id == CSSValueUnset || id == CSSValueDefault;
}

} // namespace CSSPropertyParserHelpers

} // namespace blink

#endif // CSSPropertyParserHelpers_h
