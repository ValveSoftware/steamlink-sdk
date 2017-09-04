/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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

#include "core/css/CSSFontFace.h"

#include "core/css/CSSFontFaceSource.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/FontFaceSet.h"
#include "core/css/RemoteFontFaceSource.h"
#include "core/frame/UseCounter.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"
#include <algorithm>

namespace blink {

void CSSFontFace::addSource(CSSFontFaceSource* source) {
  source->setFontFace(this);
  m_sources.append(source);
}

void CSSFontFace::setSegmentedFontFace(
    CSSSegmentedFontFace* segmentedFontFace) {
  ASSERT(!m_segmentedFontFace);
  m_segmentedFontFace = segmentedFontFace;
}

void CSSFontFace::didBeginLoad() {
  if (loadStatus() == FontFace::Unloaded)
    setLoadStatus(FontFace::Loading);
}

void CSSFontFace::fontLoaded(RemoteFontFaceSource* source) {
  if (!isValid() || source != m_sources.first())
    return;

  if (loadStatus() == FontFace::Loading) {
    if (source->isValid()) {
      setLoadStatus(FontFace::Loaded);
    } else if (source->getDisplayPeriod() ==
               RemoteFontFaceSource::FailurePeriod) {
      m_sources.clear();
      setLoadStatus(FontFace::Error);
    } else {
      m_sources.removeFirst();
      load();
    }
  }

  if (m_segmentedFontFace)
    m_segmentedFontFace->fontFaceInvalidated();
}

size_t CSSFontFace::approximateBlankCharacterCount() const {
  if (!m_sources.isEmpty() && m_sources.first()->isBlank() &&
      m_segmentedFontFace)
    return m_segmentedFontFace->approximateCharacterCount();
  return 0;
}

void CSSFontFace::didBecomeVisibleFallback(RemoteFontFaceSource* source) {
  if (!isValid() || source != m_sources.first())
    return;
  if (m_segmentedFontFace)
    m_segmentedFontFace->fontFaceInvalidated();
}

PassRefPtr<SimpleFontData> CSSFontFace::getFontData(
    const FontDescription& fontDescription) {
  if (!isValid())
    return nullptr;

  while (!m_sources.isEmpty()) {
    Member<CSSFontFaceSource>& source = m_sources.first();
    if (RefPtr<SimpleFontData> result = source->getFontData(fontDescription)) {
      if (loadStatus() == FontFace::Unloaded &&
          (source->isLoading() || source->isLoaded()))
        setLoadStatus(FontFace::Loading);
      if (loadStatus() == FontFace::Loading && source->isLoaded())
        setLoadStatus(FontFace::Loaded);
      return result.release();
    }
    m_sources.removeFirst();
  }

  if (loadStatus() == FontFace::Unloaded)
    setLoadStatus(FontFace::Loading);
  if (loadStatus() == FontFace::Loading)
    setLoadStatus(FontFace::Error);
  return nullptr;
}

bool CSSFontFace::maybeLoadFont(const FontDescription& fontDescription,
                                const String& text) {
  // This is a fast path of loading web font in style phase. For speed, this
  // only checks if the first character of the text is included in the font's
  // unicode range. If this font is needed by subsequent characters, load is
  // kicked off in layout phase.
  UChar32 character = text.characterStartingAt(0);
  if (m_ranges->contains(character)) {
    if (loadStatus() == FontFace::Unloaded)
      load(fontDescription);
    return true;
  }
  return false;
}

bool CSSFontFace::maybeLoadFont(const FontDescription& fontDescription,
                                const FontDataForRangeSet& rangeSet) {
  if (m_ranges == rangeSet.ranges()) {
    if (loadStatus() == FontFace::Unloaded) {
      load(fontDescription);
    }
    return true;
  }
  return false;
}

void CSSFontFace::load() {
  FontDescription fontDescription;
  FontFamily fontFamily;
  fontFamily.setFamily(m_fontFace->family());
  fontDescription.setFamily(fontFamily);
  fontDescription.setTraits(m_fontFace->traits());
  load(fontDescription);
}

void CSSFontFace::load(const FontDescription& fontDescription) {
  if (loadStatus() == FontFace::Unloaded)
    setLoadStatus(FontFace::Loading);
  ASSERT(loadStatus() == FontFace::Loading);

  while (!m_sources.isEmpty()) {
    Member<CSSFontFaceSource>& source = m_sources.first();
    if (source->isValid()) {
      if (source->isLocal()) {
        if (source->isLocalFontAvailable(fontDescription)) {
          setLoadStatus(FontFace::Loaded);
          return;
        }
      } else {
        if (!source->isLoaded())
          source->beginLoadIfNeeded();
        else
          setLoadStatus(FontFace::Loaded);
        return;
      }
    }
    m_sources.removeFirst();
  }
  setLoadStatus(FontFace::Error);
}

void CSSFontFace::setLoadStatus(FontFace::LoadStatusType newStatus) {
  ASSERT(m_fontFace);
  if (newStatus == FontFace::Error)
    m_fontFace->setError();
  else
    m_fontFace->setLoadStatus(newStatus);

  if (!m_segmentedFontFace)
    return;
  Document* document = m_segmentedFontFace->fontSelector()->document();
  if (document && newStatus == FontFace::Loading)
    FontFaceSet::from(*document)->beginFontLoading(m_fontFace);
}

DEFINE_TRACE(CSSFontFace) {
  visitor->trace(m_segmentedFontFace);
  visitor->trace(m_sources);
  visitor->trace(m_fontFace);
}

}  // namespace blink
