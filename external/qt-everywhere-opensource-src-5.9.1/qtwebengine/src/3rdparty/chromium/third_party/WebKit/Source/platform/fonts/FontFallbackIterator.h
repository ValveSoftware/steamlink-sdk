// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontFallbackIterator_h
#define FontFallbackIterator_h

#include "platform/fonts/FontDataForRangeSet.h"
#include "platform/fonts/FontFallbackPriority.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/Unicode.h"

namespace blink {

using namespace WTF;

class FontDescription;
class FontFallbackList;
class SimpleFontData;

class FontFallbackIterator : public RefCounted<FontFallbackIterator> {
  WTF_MAKE_NONCOPYABLE(FontFallbackIterator);

 public:
  static PassRefPtr<FontFallbackIterator> create(const FontDescription&,
                                                 PassRefPtr<FontFallbackList>,
                                                 FontFallbackPriority);

  bool hasNext() const { return m_fallbackStage != OutOfLuck; };

  // Some system fallback APIs (Windows, Android) require a character, or a
  // portion of the string to be passed.  On Mac and Linux, we get a list of
  // fonts without passing in characters.
  PassRefPtr<FontDataForRangeSet> next(const Vector<UChar32>& hintList);

 private:
  FontFallbackIterator(const FontDescription&,
                       PassRefPtr<FontFallbackList>,
                       FontFallbackPriority);
  bool rangeSetContributesForHint(const Vector<UChar32> hintList,
                                  const FontDataForRangeSet*);
  bool alreadyLoadingRangeForHintChar(UChar32 hintChar);
  void willUseRange(const AtomicString& family, const FontDataForRangeSet&);

  PassRefPtr<FontDataForRangeSet> uniqueOrNext(
      PassRefPtr<FontDataForRangeSet> candidate,
      const Vector<UChar32>& hintList);

  PassRefPtr<SimpleFontData> fallbackPriorityFont(UChar32 hint);
  PassRefPtr<SimpleFontData> uniqueSystemFontForHint(UChar32 hint);

  const FontDescription& m_fontDescription;
  RefPtr<FontFallbackList> m_fontFallbackList;
  int m_currentFontDataIndex;
  unsigned m_segmentedFaceIndex;

  enum FallbackStage {
    FallbackPriorityFonts,
    FontGroupFonts,
    SegmentedFace,
    PreferencesFonts,
    SystemFonts,
    OutOfLuck
  };

  FallbackStage m_fallbackStage;
  HashSet<UChar32> m_previouslyAskedForHint;
  // FontFallbackIterator is meant for single use by HarfBuzzShaper,
  // traversing through the fonts for shaping only once. We must not return
  // duplicate FontDataForRangeSet objects from the next() iteration functions
  // as returning a duplicate value causes a shaping run that won't return any
  // results.
  HashSet<uint32_t> m_uniqueFontDataForRangeSetsReturned;
  Vector<RefPtr<FontDataForRangeSet>> m_trackedLoadingRangeSets;
  FontFallbackPriority m_fontFallbackPriority;
};

}  // namespace blink

#endif
