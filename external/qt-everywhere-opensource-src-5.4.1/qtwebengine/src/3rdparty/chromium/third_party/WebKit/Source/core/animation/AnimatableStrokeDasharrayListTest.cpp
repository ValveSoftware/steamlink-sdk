/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "config.h"
#include "core/animation/AnimatableStrokeDasharrayList.h"

#include "core/svg/SVGLength.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

PassRefPtr<SVGLengthList> createSVGLengthList(size_t length)
{
    RefPtr<SVGLengthList> list = SVGLengthList::create();
    for (size_t i = 0; i < length; ++i)
        list->append(SVGLength::create());
    return list.release();
}

TEST(AnimationAnimatableStrokeDasharrayListTest, EqualTo)
{
    RefPtr<SVGLengthList> svgListA = createSVGLengthList(4);
    RefPtr<SVGLengthList> svgListB = createSVGLengthList(4);
    RefPtrWillBeRawPtr<AnimatableStrokeDasharrayList> listA = AnimatableStrokeDasharrayList::create(svgListA);
    RefPtrWillBeRawPtr<AnimatableStrokeDasharrayList> listB = AnimatableStrokeDasharrayList::create(svgListB);
    EXPECT_TRUE(listA->equals(listB.get()));

    TrackExceptionState exceptionState;
    svgListB->at(3)->newValueSpecifiedUnits(LengthTypePX, 50);
    listB = AnimatableStrokeDasharrayList::create(svgListB);
    EXPECT_FALSE(listA->equals(listB.get()));

    svgListB = createSVGLengthList(5);
    listB = AnimatableStrokeDasharrayList::create(svgListB);
    EXPECT_FALSE(listA->equals(listB.get()));
}

} // namespace
