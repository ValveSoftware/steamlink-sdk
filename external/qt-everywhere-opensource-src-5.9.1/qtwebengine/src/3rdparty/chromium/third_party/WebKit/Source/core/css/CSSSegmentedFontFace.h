/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSSegmentedFontFace_h
#define CSSSegmentedFontFace_h

#include "platform/fonts/FontCacheKey.h"
#include "platform/fonts/FontTraits.h"
#include "platform/fonts/SegmentedFontData.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/ListHashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSFontSelector;
class FontData;
class FontDescription;
class FontFace;
class SegmentedFontData;

class CSSSegmentedFontFace final
    : public GarbageCollectedFinalized<CSSSegmentedFontFace> {
 public:
  static CSSSegmentedFontFace* create(CSSFontSelector* selector,
                                      FontTraits traits) {
    return new CSSSegmentedFontFace(selector, traits);
  }
  ~CSSSegmentedFontFace();

  CSSFontSelector* fontSelector() const { return m_fontSelector; }
  FontTraits traits() const { return m_traits; }

  // Called when status of a FontFace has changed (e.g. loaded or timed out)
  // so cached FontData must be discarded.
  void fontFaceInvalidated();

  void addFontFace(FontFace*, bool cssConnected);
  void removeFontFace(FontFace*);
  bool isEmpty() const { return m_fontFaces.isEmpty(); }

  PassRefPtr<FontData> getFontData(const FontDescription&);

  bool checkFont(const String&) const;
  void match(const String&, HeapVector<Member<FontFace>>&) const;
  void willUseFontData(const FontDescription&, const String& text);
  void willUseRange(const FontDescription&, const blink::FontDataForRangeSet&);
  size_t approximateCharacterCount() const {
    return m_approximateCharacterCount;
  }

  DECLARE_TRACE();

 private:
  CSSSegmentedFontFace(CSSFontSelector*, FontTraits);

  void pruneTable();
  bool isValid() const;

  using FontFaceList = HeapListHashSet<Member<FontFace>>;

  Member<CSSFontSelector> m_fontSelector;
  FontTraits m_traits;
  HashMap<FontCacheKey,
          RefPtr<SegmentedFontData>,
          FontCacheKeyHash,
          FontCacheKeyTraits>
      m_fontDataTable;
  // All non-CSS-connected FontFaces are stored after the CSS-connected ones.
  FontFaceList m_fontFaces;
  FontFaceList::iterator m_firstNonCssConnectedFace;

  // Approximate number of characters styled with this CSSSegmentedFontFace.
  // LayoutText::styleDidChange() increments this on the first
  // CSSSegmentedFontFace in the style's font family list, so this is not
  // counted if this font is used as a fallback font. Also, this may be double
  // counted by style recalcs.
  // TODO(ksakamoto): Revisit the necessity of this. crbug.com/613500
  size_t m_approximateCharacterCount;
};

}  // namespace blink

#endif  // CSSSegmentedFontFace_h
