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

#ifndef SVGTransformList_h
#define SVGTransformList_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/svg/SVGTransform.h"
#include "core/svg/properties/SVGListPropertyHelper.h"

namespace WebCore {

class SVGTransformListTearOff;

class SVGTransformList FINAL : public SVGListPropertyHelper<SVGTransformList, SVGTransform> {
public:
    typedef SVGTransformListTearOff TearOffType;

    static PassRefPtr<SVGTransformList> create()
    {
        return adoptRef(new SVGTransformList());
    }

    static PassRefPtr<SVGTransformList> create(SVGTransformType, const String&);

    virtual ~SVGTransformList();

    PassRefPtr<SVGTransform> consolidate();

    bool concatenate(AffineTransform& result) const;

    // SVGPropertyBase:
    virtual PassRefPtr<SVGPropertyBase> cloneForAnimation(const String&) const OVERRIDE;
    PassRefPtr<SVGTransformList> clone();

    virtual String valueAsString() const OVERRIDE;
    void setValueAsString(const String&, ExceptionState&);
    bool parse(const UChar*& ptr, const UChar* end);
    bool parse(const LChar*& ptr, const LChar* end);

    virtual void add(PassRefPtrWillBeRawPtr<SVGPropertyBase>, SVGElement*) OVERRIDE;
    virtual void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, PassRefPtr<SVGPropertyBase> fromValue, PassRefPtr<SVGPropertyBase> toValue, PassRefPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement*) OVERRIDE;
    virtual float calculateDistance(PassRefPtr<SVGPropertyBase> to, SVGElement*) OVERRIDE;

    static AnimatedPropertyType classType() { return AnimatedTransformList; }

private:
    SVGTransformList();

    template <typename CharType>
    bool parseInternal(const CharType*& ptr, const CharType* end);
};

} // namespace WebCore

#endif // SVGTransformList_h
