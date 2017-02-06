/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef CSSPropertySourceData_h
#define CSSPropertySourceData_h

#include "core/css/StyleRule.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class SourceRange {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    SourceRange();
    SourceRange(unsigned start, unsigned end);
    unsigned length() const;

    unsigned start;
    unsigned end;
};

} // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::SourceRange);

namespace blink {

class CSSPropertySourceData {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    CSSPropertySourceData(const String& name, const String& value, bool important, bool disabled, bool parsedOk, const SourceRange& range);
    CSSPropertySourceData(const CSSPropertySourceData& other);

    String name;
    String value;
    bool important;
    bool disabled;
    bool parsedOk;
    SourceRange range;
};

} // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::CSSPropertySourceData);

namespace blink {

class CSSStyleSourceData {
    USING_FAST_MALLOC(CSSStyleSourceData);
public:
    static std::unique_ptr<CSSStyleSourceData> create()
    {
        return wrapUnique(new CSSStyleSourceData);
    }

    Vector<CSSPropertySourceData> propertyData;
};

class CSSMediaQueryExpSourceData {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    CSSMediaQueryExpSourceData(const SourceRange& valueRange)
        : valueRange(valueRange) { }

    SourceRange valueRange;
};

} // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::CSSMediaQueryExpSourceData);

namespace blink {

class CSSMediaQuerySourceData {
public:
    static std::unique_ptr<CSSMediaQuerySourceData> create()
    {
        return wrapUnique(new CSSMediaQuerySourceData);
    }

    Vector<CSSMediaQueryExpSourceData> expData;
};

class CSSMediaSourceData {
    USING_FAST_MALLOC(CSSMediaSourceData);
public:
    static std::unique_ptr<CSSMediaSourceData> create()
    {
        return wrapUnique(new CSSMediaSourceData);
    }

    Vector<std::unique_ptr<CSSMediaQuerySourceData>> queryData;
};

class CSSRuleSourceData;
using RuleSourceDataList = Vector<RefPtr<CSSRuleSourceData>>;
using SelectorRangeList = Vector<SourceRange>;

class CSSRuleSourceData : public RefCounted<CSSRuleSourceData> {
public:
    static PassRefPtr<CSSRuleSourceData> create(StyleRule::RuleType type)
    {
        return adoptRef(new CSSRuleSourceData(type));
    }

    StyleRule::RuleType type;

    // Range of the selector list in the enclosing source.
    SourceRange ruleHeaderRange;

    // Range of the rule body (e.g. style text for style rules) in the enclosing source.
    SourceRange ruleBodyRange;

    // Only for CSSStyleRules.
    SelectorRangeList selectorRanges;

    // Only for CSSStyleRules, CSSFontFaceRules, and CSSPageRules.
    std::unique_ptr<CSSStyleSourceData> styleSourceData;

    // Only for CSSMediaRules.
    RuleSourceDataList childRules;

    // Only for CSSMediaRules and CSSImportRules.
    std::unique_ptr<CSSMediaSourceData> mediaSourceData;

private:
    CSSRuleSourceData(StyleRule::RuleType type)
        : type(type)
    {
        if (type == StyleRule::Style || type == StyleRule::FontFace || type == StyleRule::Page || type == StyleRule::Keyframe)
            styleSourceData = CSSStyleSourceData::create();
        if (type == StyleRule::Media || type == StyleRule::Import)
            mediaSourceData = CSSMediaSourceData::create();
    }

};

} // namespace blink

#endif // CSSPropertySourceData_h
