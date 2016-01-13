/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef SVGNumberList_h
#define SVGNumberList_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/svg/SVGNumber.h"
#include "core/svg/properties/SVGListPropertyHelper.h"

namespace WebCore {

class SVGNumberListTearOff;

class SVGNumberList FINAL : public SVGListPropertyHelper<SVGNumberList, SVGNumber> {
public:
    typedef SVGNumberListTearOff TearOffType;

    static PassRefPtr<SVGNumberList> create()
    {
        return adoptRef(new SVGNumberList());
    }

    virtual ~SVGNumberList();

    PassRefPtr<SVGNumberList> clone();

    void setValueAsString(const String&, ExceptionState&);

    // SVGPropertyBase:
    virtual PassRefPtr<SVGPropertyBase> cloneForAnimation(const String&) const OVERRIDE;
    virtual String valueAsString() const OVERRIDE;

    virtual void add(PassRefPtrWillBeRawPtr<SVGPropertyBase>, SVGElement*) OVERRIDE;
    virtual void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, PassRefPtr<SVGPropertyBase> fromValue, PassRefPtr<SVGPropertyBase> toValue, PassRefPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement*) OVERRIDE;
    virtual float calculateDistance(PassRefPtr<SVGPropertyBase> to, SVGElement*) OVERRIDE;

    static AnimatedPropertyType classType() { return AnimatedNumberList; }

    Vector<float> toFloatVector() const;

private:
    SVGNumberList();

    bool adjustFromToListValues(PassRefPtr<SVGNumberList> fromList, PassRefPtr<SVGNumberList> toList, float percentage, bool isToAnimation, bool resizeAnimatedListIfNeeded);

    template <typename CharType>
    bool parse(const CharType*& ptr, const CharType* end);
};

} // namespace WebCore

#endif // SVGNumberList_h
