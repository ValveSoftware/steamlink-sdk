/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (c) 2007, 2008, 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "platform/fonts/Font.h"

#include "platform/LayoutTestSupport.h"
#include "platform/LayoutUnit.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/CharacterRange.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontFallbackIterator.h"
#include "platform/fonts/FontFallbackList.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/shaping/CachingWordShaper.h"
#include "platform/geometry/FloatRect.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/Character.h"
#include "platform/text/TextRun.h"
#include "platform/text/TextRunIterator.h"
#include "platform/transforms/AffineTransform.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/Unicode.h"

using namespace WTF;
using namespace Unicode;

namespace blink {

Font::Font() : m_canShapeWordByWord(0), m_shapeWordByWordComputed(0) {}

Font::Font(const FontDescription& fd)
    : m_fontDescription(fd),
      m_canShapeWordByWord(0),
      m_shapeWordByWordComputed(0) {}

Font::Font(const Font& other)
    : m_fontDescription(other.m_fontDescription),
      m_fontFallbackList(other.m_fontFallbackList),
      // TODO(yosin): We should have a comment the reason why don't we copy
      // |m_canShapeWordByWord| and |m_shapeWordByWordComputed| from |other|,
      // since |operator=()| copies them from |other|.
      m_canShapeWordByWord(0),
      m_shapeWordByWordComputed(0) {}

Font& Font::operator=(const Font& other) {
  m_fontDescription = other.m_fontDescription;
  m_fontFallbackList = other.m_fontFallbackList;
  m_canShapeWordByWord = other.m_canShapeWordByWord;
  m_shapeWordByWordComputed = other.m_shapeWordByWordComputed;
  return *this;
}

bool Font::operator==(const Font& other) const {
  FontSelector* first =
      m_fontFallbackList ? m_fontFallbackList->getFontSelector() : 0;
  FontSelector* second = other.m_fontFallbackList
                             ? other.m_fontFallbackList->getFontSelector()
                             : 0;

  return first == second && m_fontDescription == other.m_fontDescription &&
         (m_fontFallbackList ? m_fontFallbackList->fontSelectorVersion() : 0) ==
             (other.m_fontFallbackList
                  ? other.m_fontFallbackList->fontSelectorVersion()
                  : 0) &&
         (m_fontFallbackList ? m_fontFallbackList->generation() : 0) ==
             (other.m_fontFallbackList ? other.m_fontFallbackList->generation()
                                       : 0);
}

void Font::update(FontSelector* fontSelector) const {
  // FIXME: It is pretty crazy that we are willing to just poke into a RefPtr,
  // but it ends up being reasonably safe (because inherited fonts in the render
  // tree pick up the new style anyway. Other copies are transient, e.g., the
  // state in the GraphicsContext, and won't stick around long enough to get you
  // in trouble). Still, this is pretty disgusting, and could eventually be
  // rectified by using RefPtrs for Fonts themselves.
  if (!m_fontFallbackList)
    m_fontFallbackList = FontFallbackList::create();
  m_fontFallbackList->invalidate(fontSelector);
}

float Font::buildGlyphBuffer(const TextRunPaintInfo& runInfo,
                             GlyphBuffer& glyphBuffer,
                             const GlyphData* emphasisData) const {
  float width;
  CachingWordShaper shaper(m_fontFallbackList->shapeCache(m_fontDescription));
  if (emphasisData) {
    width = shaper.fillGlyphBufferForTextEmphasis(this, runInfo.run,
                                                  emphasisData, &glyphBuffer,
                                                  runInfo.from, runInfo.to);
  } else {
    width = shaper.fillGlyphBuffer(this, runInfo.run, nullptr, &glyphBuffer,
                                   runInfo.from, runInfo.to);
  }
  return width;
}

bool Font::drawText(SkCanvas* canvas,
                    const TextRunPaintInfo& runInfo,
                    const FloatPoint& point,
                    float deviceScaleFactor,
                    const SkPaint& paint) const {
  // Don't draw anything while we are using custom fonts that are in the process
  // of loading.
  if (shouldSkipDrawing())
    return false;

  if (runInfo.cachedTextBlob && runInfo.cachedTextBlob->get()) {
    // we have a pre-cached blob -- happy joy!
    canvas->drawTextBlob(runInfo.cachedTextBlob->get(), point.x(), point.y(),
                         paint);
    return true;
  }

  GlyphBuffer glyphBuffer;
  buildGlyphBuffer(runInfo, glyphBuffer);

  drawGlyphBuffer(canvas, paint, runInfo, glyphBuffer, point,
                  deviceScaleFactor);
  return true;
}

bool Font::drawBidiText(SkCanvas* canvas,
                        const TextRunPaintInfo& runInfo,
                        const FloatPoint& point,
                        CustomFontNotReadyAction customFontNotReadyAction,
                        float deviceScaleFactor,
                        const SkPaint& paint) const {
  // Don't draw anything while we are using custom fonts that are in the process
  // of loading, except if the 'force' argument is set to true (in which case it
  // will use a fallback font).
  if (shouldSkipDrawing() &&
      customFontNotReadyAction == DoNotPaintIfFontNotReady)
    return false;

  // sub-run painting is not supported for Bidi text.
  const TextRun& run = runInfo.run;
  ASSERT((runInfo.from == 0) && (runInfo.to == run.length()));
  BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
  bidiResolver.setStatus(
      BidiStatus(run.direction(), run.directionalOverride()));
  bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));

  // FIXME: This ownership should be reversed. We should pass BidiRunList
  // to BidiResolver in createBidiRunsForLine.
  BidiRunList<BidiCharacterRun>& bidiRuns = bidiResolver.runs();
  bidiResolver.createBidiRunsForLine(TextRunIterator(&run, run.length()));
  if (!bidiRuns.runCount())
    return true;

  FloatPoint currPoint = point;
  BidiCharacterRun* bidiRun = bidiRuns.firstRun();
  while (bidiRun) {
    TextRun subrun =
        run.subRun(bidiRun->start(), bidiRun->stop() - bidiRun->start());
    bool isRTL = bidiRun->level() % 2;
    subrun.setDirection(isRTL ? RTL : LTR);
    subrun.setDirectionalOverride(bidiRun->dirOverride(false));

    TextRunPaintInfo subrunInfo(subrun);
    subrunInfo.bounds = runInfo.bounds;

    // TODO: investigate blob consolidation/caching (technically,
    //       all subruns could be part of the same blob).
    GlyphBuffer glyphBuffer;
    float runWidth = buildGlyphBuffer(subrunInfo, glyphBuffer);
    drawGlyphBuffer(canvas, paint, subrunInfo, glyphBuffer, currPoint,
                    deviceScaleFactor);

    bidiRun = bidiRun->next();
    currPoint.move(runWidth, 0);
  }

  bidiRuns.deleteRuns();
  return true;
}

void Font::drawEmphasisMarks(SkCanvas* canvas,
                             const TextRunPaintInfo& runInfo,
                             const AtomicString& mark,
                             const FloatPoint& point,
                             float deviceScaleFactor,
                             const SkPaint& paint) const {
  if (shouldSkipDrawing())
    return;

  FontCachePurgePreventer purgePreventer;

  GlyphData emphasisGlyphData;
  if (!getEmphasisMarkGlyphData(mark, emphasisGlyphData))
    return;

  ASSERT(emphasisGlyphData.fontData);
  if (!emphasisGlyphData.fontData)
    return;

  GlyphBuffer glyphBuffer;
  buildGlyphBuffer(runInfo, glyphBuffer, &emphasisGlyphData);

  if (glyphBuffer.isEmpty())
    return;

  drawGlyphBuffer(canvas, paint, runInfo, glyphBuffer, point,
                  deviceScaleFactor);
}

float Font::width(const TextRun& run,
                  HashSet<const SimpleFontData*>* fallbackFonts,
                  FloatRect* glyphBounds) const {
  FontCachePurgePreventer purgePreventer;
  CachingWordShaper shaper(m_fontFallbackList->shapeCache(m_fontDescription));
  float width = shaper.width(this, run, fallbackFonts, glyphBounds);
  return width;
}

namespace {

enum BlobRotation {
  NoRotation,
  CCWRotation,
};

class GlyphBufferBloberizer {
  STACK_ALLOCATED()
 public:
  GlyphBufferBloberizer(const GlyphBuffer& buffer,
                        const Font* font,
                        float deviceScaleFactor)
      : m_buffer(buffer),
        m_font(font),
        m_deviceScaleFactor(deviceScaleFactor),
        m_hasVerticalOffsets(buffer.hasVerticalOffsets()),
        m_index(0),
        m_blobCount(0),
        m_rotation(buffer.isEmpty() ? NoRotation : computeBlobRotation(
                                                       buffer.fontDataAt(0))) {}

  bool done() const { return m_index >= m_buffer.size(); }
  unsigned blobCount() const { return m_blobCount; }

  std::pair<sk_sp<SkTextBlob>, BlobRotation> next() {
    ASSERT(!done());
    const BlobRotation currentRotation = m_rotation;

    while (m_index < m_buffer.size()) {
      const SimpleFontData* fontData = m_buffer.fontDataAt(m_index);
      ASSERT(fontData);

      const BlobRotation newRotation = computeBlobRotation(fontData);
      if (newRotation != m_rotation) {
        // We're switching to an orientation which requires a different rotation
        //   => emit the pending blob (and start a new one with the new
        //      rotation).
        m_rotation = newRotation;
        break;
      }

      const unsigned start = m_index++;
      while (m_index < m_buffer.size() &&
             m_buffer.fontDataAt(m_index) == fontData)
        m_index++;

      appendRun(start, m_index - start, fontData);
    }

    m_blobCount++;
    return std::make_pair(m_builder.make(), currentRotation);
  }

 private:
  static BlobRotation computeBlobRotation(const SimpleFontData* font) {
    // For vertical upright text we need to compensate the inherited 90deg CW
    // rotation (using a 90deg CCW rotation).
    return (font->platformData().isVerticalAnyUpright() && font->verticalData())
               ? CCWRotation
               : NoRotation;
  }

  void appendRun(unsigned start,
                 unsigned count,
                 const SimpleFontData* fontData) {
    SkPaint paint;
    fontData->platformData().setupPaint(&paint, m_deviceScaleFactor, m_font);
    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

    const SkTextBlobBuilder::RunBuffer& buffer =
        m_hasVerticalOffsets ? m_builder.allocRunPos(paint, count)
                             : m_builder.allocRunPosH(paint, count, 0);

    const uint16_t* glyphs = m_buffer.glyphs(start);
    const float* offsets = m_buffer.offsets(start);
    std::copy(glyphs, glyphs + count, buffer.glyphs);

    if (m_rotation == NoRotation) {
      std::copy(offsets, offsets + (m_hasVerticalOffsets ? 2 * count : count),
                buffer.pos);
    } else {
      ASSERT(m_hasVerticalOffsets);

      const float verticalBaselineXOffset =
          fontData->getFontMetrics().floatAscent() -
          fontData->getFontMetrics().floatAscent(IdeographicBaseline);

      // TODO(fmalita): why don't we apply this adjustment when building the
      // glyph buffer?
      for (unsigned i = 0; i < 2 * count; i += 2) {
        buffer.pos[i] = SkFloatToScalar(offsets[i] + verticalBaselineXOffset);
        buffer.pos[i + 1] = SkFloatToScalar(offsets[i + 1]);
      }
    }
  }

  const GlyphBuffer& m_buffer;
  const Font* m_font;
  const float m_deviceScaleFactor;
  const bool m_hasVerticalOffsets;

  SkTextBlobBuilder m_builder;
  unsigned m_index;
  unsigned m_blobCount;
  BlobRotation m_rotation;
};

}  // anonymous namespace

void Font::drawGlyphBuffer(SkCanvas* canvas,
                           const SkPaint& paint,
                           const TextRunPaintInfo& runInfo,
                           const GlyphBuffer& glyphBuffer,
                           const FloatPoint& point,
                           float deviceScaleFactor) const {
  GlyphBufferBloberizer bloberizer(glyphBuffer, this, deviceScaleFactor);
  std::pair<sk_sp<SkTextBlob>, BlobRotation> blob;

  while (!bloberizer.done()) {
    blob = bloberizer.next();
    ASSERT(blob.first);

    SkAutoCanvasRestore autoRestore(canvas, false);
    if (blob.second == CCWRotation) {
      canvas->save();

      SkMatrix m;
      m.setSinCos(-1, 0, point.x(), point.y());
      canvas->concat(m);
    }

    canvas->drawTextBlob(blob.first, point.x(), point.y(), paint);
  }

  // Cache results when
  //   1) requested by clients, and
  //   2) the glyph buffer is encoded as a single blob, and
  //   3) the blob is not upright/rotated
  if (runInfo.cachedTextBlob && bloberizer.blobCount() == 1 &&
      blob.second == NoRotation) {
    ASSERT(!*runInfo.cachedTextBlob);
    *runInfo.cachedTextBlob = std::move(blob.first);
    ASSERT(*runInfo.cachedTextBlob);
  }
}

static int getInterceptsFromBloberizer(const GlyphBuffer& glyphBuffer,
                                       const Font* font,
                                       const SkPaint& paint,
                                       float deviceScaleFactor,
                                       const std::tuple<float, float>& bounds,
                                       SkScalar* interceptsBuffer) {
  SkScalar boundsArray[2] = {std::get<0>(bounds), std::get<1>(bounds)};
  GlyphBufferBloberizer bloberizer(glyphBuffer, font, deviceScaleFactor);
  std::pair<sk_sp<SkTextBlob>, BlobRotation> blob;

  int numIntervals = 0;
  while (!bloberizer.done()) {
    blob = bloberizer.next();
    DCHECK(blob.first);

    // GlyphBufferBloberizer splits for a new blob rotation, but does not split
    // for a change in font. A TextBlob can contain runs with differing fonts
    // and the getTextBlobIntercepts method handles multiple fonts for us. For
    // upright in vertical blobs we currently have to bail, see crbug.com/655154
    if (blob.second == BlobRotation::CCWRotation)
      continue;

    SkScalar* offsetInterceptsBuffer = nullptr;
    if (interceptsBuffer)
      offsetInterceptsBuffer = &interceptsBuffer[numIntervals];
    numIntervals += paint.getTextBlobIntercepts(blob.first.get(), boundsArray,
                                                offsetInterceptsBuffer);
  }
  return numIntervals;
}

void Font::getTextIntercepts(const TextRunPaintInfo& runInfo,
                             float deviceScaleFactor,
                             const SkPaint& paint,
                             const std::tuple<float, float>& bounds,
                             Vector<TextIntercept>& intercepts) const {
  if (shouldSkipDrawing())
    return;

  if (runInfo.cachedTextBlob && runInfo.cachedTextBlob->get()) {
    SkScalar boundsArray[2] = {std::get<0>(bounds), std::get<1>(bounds)};
    int numIntervals = paint.getTextBlobIntercepts(
        runInfo.cachedTextBlob->get(), boundsArray, nullptr);
    if (!numIntervals)
      return;
    DCHECK_EQ(numIntervals % 2, 0);
    intercepts.resize(numIntervals / 2);
    paint.getTextBlobIntercepts(runInfo.cachedTextBlob->get(), boundsArray,
                                reinterpret_cast<SkScalar*>(intercepts.data()));
    return;
  }

  GlyphBuffer glyphBuffer;
  buildGlyphBuffer(runInfo, glyphBuffer);

  // Get the number of intervals, without copying the actual values by
  // specifying nullptr for the buffer, following the Skia allocation model for
  // retrieving text intercepts.
  int numIntervals = getInterceptsFromBloberizer(
      glyphBuffer, this, paint, deviceScaleFactor, bounds, nullptr);
  if (!numIntervals)
    return;
  DCHECK_EQ(numIntervals % 2, 0);
  intercepts.resize(numIntervals / 2);

  getInterceptsFromBloberizer(glyphBuffer, this, paint, deviceScaleFactor,
                              bounds,
                              reinterpret_cast<SkScalar*>(intercepts.data()));
}

static inline FloatRect pixelSnappedSelectionRect(FloatRect rect) {
  // Using roundf() rather than ceilf() for the right edge as a compromise to
  // ensure correct caret positioning.
  float roundedX = roundf(rect.x());
  return FloatRect(roundedX, rect.y(), roundf(rect.maxX() - roundedX),
                   rect.height());
}

FloatRect Font::selectionRectForText(const TextRun& run,
                                     const FloatPoint& point,
                                     int height,
                                     int from,
                                     int to,
                                     bool accountForGlyphBounds) const {
  to = (to == -1 ? run.length() : to);

  TextRunPaintInfo runInfo(run);
  runInfo.from = from;
  runInfo.to = to;

  FontCachePurgePreventer purgePreventer;

  CachingWordShaper shaper(m_fontFallbackList->shapeCache(m_fontDescription));
  CharacterRange range = shaper.getCharacterRange(this, run, from, to);

  return pixelSnappedSelectionRect(
      FloatRect(point.x() + range.start, point.y(), range.width(), height));
}

int Font::offsetForPosition(const TextRun& run,
                            float xFloat,
                            bool includePartialGlyphs) const {
  FontCachePurgePreventer purgePreventer;
  CachingWordShaper shaper(m_fontFallbackList->shapeCache(m_fontDescription));
  return shaper.offsetForPosition(this, run, xFloat, includePartialGlyphs);
}

ShapeCache* Font::shapeCache() const {
  return m_fontFallbackList->shapeCache(m_fontDescription);
}

bool Font::canShapeWordByWord() const {
  if (!m_shapeWordByWordComputed) {
    m_canShapeWordByWord = computeCanShapeWordByWord();
    m_shapeWordByWordComputed = true;
  }
  return m_canShapeWordByWord;
};

bool Font::computeCanShapeWordByWord() const {
  if (!getFontDescription().getTypesettingFeatures())
    return true;

  if (!primaryFont())
    return false;

  const FontPlatformData& platformData = primaryFont()->platformData();
  TypesettingFeatures features = getFontDescription().getTypesettingFeatures();
  return !platformData.hasSpaceInLigaturesOrKerning(features);
};

void Font::willUseFontData(const String& text) const {
  const FontFamily& family = getFontDescription().family();
  if (m_fontFallbackList && m_fontFallbackList->getFontSelector() &&
      !family.familyIsEmpty())
    m_fontFallbackList->getFontSelector()->willUseFontData(
        getFontDescription(), family.family(), text);
}

PassRefPtr<FontFallbackIterator> Font::createFontFallbackIterator(
    FontFallbackPriority fallbackPriority) const {
  return FontFallbackIterator::create(m_fontDescription, m_fontFallbackList,
                                      fallbackPriority);
}

bool Font::getEmphasisMarkGlyphData(const AtomicString& mark,
                                    GlyphData& glyphData) const {
  if (mark.isEmpty())
    return false;

  TextRun emphasisMarkRun(mark, mark.length());
  TextRunPaintInfo emphasisPaintInfo(emphasisMarkRun);
  GlyphBuffer glyphBuffer;
  buildGlyphBuffer(emphasisPaintInfo, glyphBuffer);

  if (glyphBuffer.isEmpty())
    return false;

  ASSERT(glyphBuffer.fontDataAt(0));
  glyphData.fontData =
      glyphBuffer.fontDataAt(0)->emphasisMarkFontData(m_fontDescription).get();
  glyphData.glyph = glyphBuffer.glyphAt(0);

  return true;
}

int Font::emphasisMarkAscent(const AtomicString& mark) const {
  FontCachePurgePreventer purgePreventer;

  GlyphData markGlyphData;
  if (!getEmphasisMarkGlyphData(mark, markGlyphData))
    return 0;

  const SimpleFontData* markFontData = markGlyphData.fontData;
  ASSERT(markFontData);
  if (!markFontData)
    return 0;

  return markFontData->getFontMetrics().ascent();
}

int Font::emphasisMarkDescent(const AtomicString& mark) const {
  FontCachePurgePreventer purgePreventer;

  GlyphData markGlyphData;
  if (!getEmphasisMarkGlyphData(mark, markGlyphData))
    return 0;

  const SimpleFontData* markFontData = markGlyphData.fontData;
  ASSERT(markFontData);
  if (!markFontData)
    return 0;

  return markFontData->getFontMetrics().descent();
}

int Font::emphasisMarkHeight(const AtomicString& mark) const {
  FontCachePurgePreventer purgePreventer;

  GlyphData markGlyphData;
  if (!getEmphasisMarkGlyphData(mark, markGlyphData))
    return 0;

  const SimpleFontData* markFontData = markGlyphData.fontData;
  ASSERT(markFontData);
  if (!markFontData)
    return 0;

  return markFontData->getFontMetrics().height();
}

CharacterRange Font::getCharacterRange(const TextRun& run,
                                       unsigned from,
                                       unsigned to) const {
  FontCachePurgePreventer purgePreventer;
  CachingWordShaper shaper(m_fontFallbackList->shapeCache(m_fontDescription));
  return shaper.getCharacterRange(this, run, from, to);
}

Vector<CharacterRange> Font::individualCharacterRanges(
    const TextRun& run) const {
  FontCachePurgePreventer purgePreventer;
  CachingWordShaper shaper(m_fontFallbackList->shapeCache(m_fontDescription));
  auto ranges = shaper.individualCharacterRanges(this, run);
  // The shaper should return ranges.size == run.length but on some platforms
  // (OSX10.9.5) we are seeing cases in the upper end of the unicode range
  // where this is not true (see: crbug.com/620952). To catch these cases on
  // more popular platforms, and to protect users, we are using a CHECK here.
  CHECK_EQ(ranges.size(), run.length());
  return ranges;
}

bool Font::loadingCustomFonts() const {
  return m_fontFallbackList && m_fontFallbackList->loadingCustomFonts();
}

bool Font::isFallbackValid() const {
  return !m_fontFallbackList || m_fontFallbackList->isValid();
}

}  // namespace blink
