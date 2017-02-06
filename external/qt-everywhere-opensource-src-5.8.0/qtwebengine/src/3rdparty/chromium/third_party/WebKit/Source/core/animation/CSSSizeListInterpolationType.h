// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSizeListInterpolationType_h
#define CSSSizeListInterpolationType_h

#include "core/animation/CSSLengthInterpolationType.h"
#include "core/animation/CSSLengthListInterpolationType.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"

namespace blink {

class CSSSizeListInterpolationType : public CSSLengthListInterpolationType {
public:
    CSSSizeListInterpolationType(CSSPropertyID property)
        : CSSLengthListInterpolationType(property)
    { }

private:
    InterpolationValue maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const final
    {
        CSSValueList* tempList = nullptr;
        if (!value.isBaseValueList()) {
            tempList = CSSValueList::createCommaSeparated();
            tempList->append(value);
        }
        const CSSValueList& list = value.isBaseValueList() ? toCSSValueList(value) : *tempList;

        // Only size lists without keywords may interpolate smoothly:
        // https://drafts.csswg.org/css-backgrounds-3/#the-background-size
        return ListInterpolationFunctions::createList(list.length() * 2, [&list](size_t index) -> InterpolationValue {
            const CSSValue& item = list.item(index / 2);
            if (!item.isValuePair())
                return nullptr;
            const CSSValuePair& pair = toCSSValuePair(item);
            return CSSLengthInterpolationType::maybeConvertCSSValue(index % 2 == 0 ? pair.first() : pair.second());
        });
    }
};

} // namespace blink

#endif // CSSSizeListInterpolationType_h
