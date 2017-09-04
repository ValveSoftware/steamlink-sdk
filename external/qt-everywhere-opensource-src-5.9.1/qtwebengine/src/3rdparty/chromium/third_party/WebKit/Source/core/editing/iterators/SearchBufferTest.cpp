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

#include "core/editing/iterators/SearchBuffer.h"

#include "core/dom/Range.h"
#include "core/editing/EditingTestBase.h"
#include "core/frame/FrameView.h"

namespace blink {

class SearchBufferTest : public EditingTestBase {
 protected:
  Range* getBodyRange() const;
};

Range* SearchBufferTest::getBodyRange() const {
  Range* range(Range::create(document()));
  range->selectNode(document().body());
  return range;
}

TEST_F(SearchBufferTest, FindPlainTextInvalidTarget) {
  static const char* bodyContent = "<div>foo bar test</div>";
  setBodyContent(bodyContent);
  Range* range = getBodyRange();

  Range* expectedRange = range->cloneRange();
  expectedRange->collapse(false);

  // A lone lead surrogate (0xDA0A) example taken from fuzz-58.
  static const UChar invalid1[] = {0x1461u, 0x2130u, 0x129bu, 0xd711u, 0xd6feu,
                                   0xccadu, 0x7064u, 0xd6a0u, 0x4e3bu, 0x03abu,
                                   0x17dcu, 0xb8b7u, 0xbf55u, 0xfca0u, 0x07fau,
                                   0x0427u, 0xda0au, 0};

  // A lone trailing surrogate (U+DC01).
  static const UChar invalid2[] = {0x1461u, 0x2130u, 0x129bu, 0xdc01u,
                                   0xd6feu, 0xccadu, 0};
  // A trailing surrogate followed by a lead surrogate (U+DC03 U+D901).
  static const UChar invalid3[] = {0xd800u, 0xdc00u, 0x0061u, 0xdc03u,
                                   0xd901u, 0xccadu, 0};

  static const UChar* invalidUStrings[] = {invalid1, invalid2, invalid3};

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(invalidUStrings); ++i) {
    String invalidTarget(invalidUStrings[i]);
    EphemeralRange foundRange =
        findPlainText(EphemeralRange(range), invalidTarget, 0);
    Range* actualRange = Range::create(document(), foundRange.startPosition(),
                                       foundRange.endPosition());
    EXPECT_TRUE(areRangesEqual(expectedRange, actualRange));
  }
}

}  // namespace blink
