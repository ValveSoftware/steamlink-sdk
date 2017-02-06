// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxge/include/fx_ge.h"

#if defined(_SKIA_SUPPORT_)
#include <algorithm>
#include <vector>

#include "core/fxcodec/include/fx_codec.h"

#include "core/fpdfapi/fpdf_page/cpdf_shadingpattern.h"
#include "core/fpdfapi/fpdf_page/pageint.h"
#include "core/fpdfapi/fpdf_parser/include/cpdf_array.h"
#include "core/fpdfapi/fpdf_parser/include/cpdf_dictionary.h"
#include "core/fpdfapi/fpdf_parser/include/cpdf_stream_acc.h"
#include "core/fxge/skia/fx_skia_device.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/pathops/SkPathOps.h"

namespace {

#define SHOW_SKIA_PATH 0  // set to 1 to print the path contents
#define DRAW_SKIA_CLIP 0  // set to 1 to draw a green rectangle around the clip

void DebugShowSkiaPath(const SkPath& path) {
#if SHOW_SKIA_PATH
  char buffer[4096];
  sk_bzero(buffer, sizeof(buffer));
  SkMemoryWStream stream(buffer, sizeof(buffer));
  path.dump(&stream, false, false);
  printf("%s\n", buffer);
#endif  // SHOW_SKIA_PATH
}

void DebugShowCanvasMatrix(const SkCanvas* canvas) {
#if SHOW_SKIA_PATH
  SkMatrix matrix = canvas->getTotalMatrix();
  SkScalar m[9];
  matrix.get9(m);
  printf("(%g,%g,%g) (%g,%g,%g) (%g,%g,%g)\n", m[0], m[1], m[2], m[3], m[4],
         m[5], m[6], m[7], m[8]);
#endif  // SHOW_SKIA_PATH
}

#if DRAW_SKIA_CLIP

SkPaint DebugClipPaint() {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(SK_ColorGREEN);
  paint.setStyle(SkPaint::kStroke_Style);
  return paint;
}

void DebugDrawSkiaClipRect(SkCanvas* canvas, const SkRect& rect) {
  SkPaint paint = DebugClipPaint();
  canvas->drawRect(rect, paint);
}

void DebugDrawSkiaClipPath(SkCanvas* canvas, const SkPath& path) {
  SkPaint paint = DebugClipPaint();
  canvas->drawPath(path, paint);
}

#else  // DRAW_SKIA_CLIP

void DebugDrawSkiaClipRect(SkCanvas* canvas, const SkRect& rect) {}
void DebugDrawSkiaClipPath(SkCanvas* canvas, const SkPath& path) {}

#endif  // DRAW_SKIA_CLIP

#undef SHOW_SKIA_PATH
#undef DRAW_SKIA_CLIP

static void DebugVerifyBitmapIsPreMultiplied(void* buffer,
                                             int width,
                                             int height) {
#ifdef SK_DEBUG
  // verify that input is really premultiplied
  for (int y = 0; y < height; ++y) {
    const uint32_t* srcRow = static_cast<const uint32_t*>(buffer) + y * width;
    for (int x = 0; x < width; ++x) {
      uint8_t a = SkGetPackedA32(srcRow[x]);
      uint8_t r = SkGetPackedR32(srcRow[x]);
      uint8_t g = SkGetPackedG32(srcRow[x]);
      uint8_t b = SkGetPackedB32(srcRow[x]);
      SkA32Assert(a);
      SkASSERT(r <= a);
      SkASSERT(g <= a);
      SkASSERT(b <= a);
    }
  }
#endif
}

static void DebugValidate(const CFX_DIBitmap* bitmap,
                          const CFX_DIBitmap* device) {
  if (bitmap) {
    SkASSERT(bitmap->GetBPP() == 8 || bitmap->GetBPP() == 32);
    if (bitmap->GetBPP() == 32) {
      DebugVerifyBitmapIsPreMultiplied(bitmap->GetBuffer(), bitmap->GetWidth(),
                                       bitmap->GetHeight());
    }
  }
  if (device) {
    SkASSERT(device->GetBPP() == 8 || device->GetBPP() == 32);
    if (device->GetBPP() == 32) {
      DebugVerifyBitmapIsPreMultiplied(device->GetBuffer(), device->GetWidth(),
                                       device->GetHeight());
    }
  }
}

SkPath BuildPath(const CFX_PathData* pPathData) {
  SkPath skPath;
  const CFX_PathData* pFPath = pPathData;
  int nPoints = pFPath->GetPointCount();
  FX_PATHPOINT* pPoints = pFPath->GetPoints();
  for (int i = 0; i < nPoints; i++) {
    FX_FLOAT x = pPoints[i].m_PointX;
    FX_FLOAT y = pPoints[i].m_PointY;
    int point_type = pPoints[i].m_Flag & FXPT_TYPE;
    if (point_type == FXPT_MOVETO) {
      skPath.moveTo(x, y);
    } else if (point_type == FXPT_LINETO) {
      skPath.lineTo(x, y);
    } else if (point_type == FXPT_BEZIERTO) {
      FX_FLOAT x2 = pPoints[i + 1].m_PointX, y2 = pPoints[i + 1].m_PointY;
      FX_FLOAT x3 = pPoints[i + 2].m_PointX, y3 = pPoints[i + 2].m_PointY;
      skPath.cubicTo(x, y, x2, y2, x3, y3);
      i += 2;
    }
    if (pPoints[i].m_Flag & FXPT_CLOSEFIGURE)
      skPath.close();
  }
  return skPath;
}

SkMatrix ToSkMatrix(const CFX_Matrix& m) {
  SkMatrix skMatrix;
  skMatrix.setAll(m.a, m.b, m.e, m.c, m.d, m.f, 0, 0, 1);
  return skMatrix;
}

// use when pdf's y-axis points up insead of down
SkMatrix ToFlippedSkMatrix(const CFX_Matrix& m) {
  SkMatrix skMatrix;
  skMatrix.setAll(m.a, m.b, m.e, -m.c, -m.d, m.f, 0, 0, 1);
  return skMatrix;
}

SkXfermode::Mode GetSkiaBlendMode(int blend_type) {
  switch (blend_type) {
    case FXDIB_BLEND_MULTIPLY:
      return SkXfermode::kMultiply_Mode;
    case FXDIB_BLEND_SCREEN:
      return SkXfermode::kScreen_Mode;
    case FXDIB_BLEND_OVERLAY:
      return SkXfermode::kOverlay_Mode;
    case FXDIB_BLEND_DARKEN:
      return SkXfermode::kDarken_Mode;
    case FXDIB_BLEND_LIGHTEN:
      return SkXfermode::kLighten_Mode;
    case FXDIB_BLEND_COLORDODGE:
      return SkXfermode::kColorDodge_Mode;
    case FXDIB_BLEND_COLORBURN:
      return SkXfermode::kColorBurn_Mode;
    case FXDIB_BLEND_HARDLIGHT:
      return SkXfermode::kHardLight_Mode;
    case FXDIB_BLEND_SOFTLIGHT:
      return SkXfermode::kSoftLight_Mode;
    case FXDIB_BLEND_DIFFERENCE:
      return SkXfermode::kDifference_Mode;
    case FXDIB_BLEND_EXCLUSION:
      return SkXfermode::kExclusion_Mode;
    case FXDIB_BLEND_HUE:
      return SkXfermode::kHue_Mode;
    case FXDIB_BLEND_SATURATION:
      return SkXfermode::kSaturation_Mode;
    case FXDIB_BLEND_COLOR:
      return SkXfermode::kColor_Mode;
    case FXDIB_BLEND_LUMINOSITY:
      return SkXfermode::kLuminosity_Mode;
    case FXDIB_BLEND_NORMAL:
    default:
      return SkXfermode::kSrcOver_Mode;
  }
}

bool AddColors(const CPDF_ExpIntFunc* pFunc, SkTDArray<SkColor>* skColors) {
  if (pFunc->CountInputs() != 1)
    return false;
  if (pFunc->m_Exponent != 1)
    return false;
  if (pFunc->m_nOrigOutputs != 3)
    return false;
  skColors->push(
      SkColorSetARGB(0xFF, SkUnitScalarClampToByte(pFunc->m_pBeginValues[0]),
                     SkUnitScalarClampToByte(pFunc->m_pBeginValues[1]),
                     SkUnitScalarClampToByte(pFunc->m_pBeginValues[2])));
  skColors->push(
      SkColorSetARGB(0xFF, SkUnitScalarClampToByte(pFunc->m_pEndValues[0]),
                     SkUnitScalarClampToByte(pFunc->m_pEndValues[1]),
                     SkUnitScalarClampToByte(pFunc->m_pEndValues[2])));
  return true;
}

uint8_t FloatToByte(FX_FLOAT f) {
  ASSERT(0 <= f && f <= 1);
  return (uint8_t)(f * 255.99f);
}

bool AddSamples(const CPDF_SampledFunc* pFunc,
                SkTDArray<SkColor>* skColors,
                SkTDArray<SkScalar>* skPos) {
  if (pFunc->CountInputs() != 1)
    return false;
  if (pFunc->CountOutputs() != 3)  // expect rgb
    return false;
  if (pFunc->GetEncodeInfo().empty())
    return false;
  const CPDF_SampledFunc::SampleEncodeInfo& encodeInfo =
      pFunc->GetEncodeInfo()[0];
  if (encodeInfo.encode_min != 0)
    return false;
  if (encodeInfo.encode_max != encodeInfo.sizes - 1)
    return false;
  uint32_t sampleSize = pFunc->GetBitsPerSample();
  uint32_t sampleCount = encodeInfo.sizes;
  if (sampleCount != 1U << sampleSize)
    return false;
  if (pFunc->GetSampleStream()->GetSize() < sampleCount * 3 * sampleSize / 8)
    return false;

  FX_FLOAT colorsMin[3];
  FX_FLOAT colorsMax[3];
  for (int i = 0; i < 3; ++i) {
    colorsMin[i] = pFunc->GetRange(i * 2);
    colorsMax[i] = pFunc->GetRange(i * 2 + 1);
  }
  const uint8_t* pSampleData = pFunc->GetSampleStream()->GetData();
  for (uint32_t i = 0; i < sampleCount; ++i) {
    FX_FLOAT floatColors[3];
    for (uint32_t j = 0; j < 3; ++j) {
      int sample = GetBits32(pSampleData, (i * 3 + j) * sampleSize, sampleSize);
      FX_FLOAT interp = (FX_FLOAT)sample / (sampleCount - 1);
      floatColors[j] = colorsMin[j] + (colorsMax[j] - colorsMin[j]) * interp;
    }
    SkColor color =
        SkPackARGB32(0xFF, FloatToByte(floatColors[0]),
                     FloatToByte(floatColors[1]), FloatToByte(floatColors[2]));
    skColors->push(color);
    skPos->push((FX_FLOAT)i / (sampleCount - 1));
  }
  return true;
}

bool AddStitching(const CPDF_StitchFunc* pFunc,
                  SkTDArray<SkColor>* skColors,
                  SkTDArray<SkScalar>* skPos) {
  int inputs = pFunc->CountInputs();
  FX_FLOAT boundsStart = pFunc->GetDomain(0);

  const auto& subFunctions = pFunc->GetSubFunctions();
  for (int i = 0; i < inputs; ++i) {
    const CPDF_ExpIntFunc* pSubFunc = subFunctions[i]->ToExpIntFunc();
    if (!pSubFunc)
      return false;
    if (!AddColors(pSubFunc, skColors))
      return false;
    FX_FLOAT boundsEnd =
        i < inputs - 1 ? pFunc->GetBound(i) : pFunc->GetDomain(1);
    skPos->push(boundsStart);
    skPos->push(boundsEnd);
    boundsStart = boundsEnd;
  }
  return true;
}

void RgbByteOrderTransferBitmap(CFX_DIBitmap* pBitmap,
                                int dest_left,
                                int dest_top,
                                int width,
                                int height,
                                const CFX_DIBSource* pSrcBitmap,
                                int src_left,
                                int src_top) {
  if (!pBitmap)
    return;
  pBitmap->GetOverlapRect(dest_left, dest_top, width, height,
                          pSrcBitmap->GetWidth(), pSrcBitmap->GetHeight(),
                          src_left, src_top, nullptr);
  if (width == 0 || height == 0)
    return;
  int Bpp = pBitmap->GetBPP() / 8;
  FXDIB_Format dest_format = pBitmap->GetFormat();
  FXDIB_Format src_format = pSrcBitmap->GetFormat();
  int pitch = pBitmap->GetPitch();
  uint8_t* buffer = pBitmap->GetBuffer();
  if (dest_format == src_format) {
    for (int row = 0; row < height; row++) {
      uint8_t* dest_scan = buffer + (dest_top + row) * pitch + dest_left * Bpp;
      uint8_t* src_scan =
          (uint8_t*)pSrcBitmap->GetScanline(src_top + row) + src_left * Bpp;
      if (Bpp == 4) {
        for (int col = 0; col < width; col++) {
          FXARGB_SETDIB(dest_scan, FXARGB_MAKE(src_scan[3], src_scan[0],
                                               src_scan[1], src_scan[2]));
          dest_scan += 4;
          src_scan += 4;
        }
      } else {
        for (int col = 0; col < width; col++) {
          *dest_scan++ = src_scan[2];
          *dest_scan++ = src_scan[1];
          *dest_scan++ = src_scan[0];
          src_scan += 3;
        }
      }
    }
    return;
  }
  uint8_t* dest_buf = buffer + dest_top * pitch + dest_left * Bpp;
  if (dest_format == FXDIB_Rgb) {
    if (src_format == FXDIB_Rgb32) {
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = dest_buf + row * pitch;
        uint8_t* src_scan =
            (uint8_t*)pSrcBitmap->GetScanline(src_top + row) + src_left * 4;
        for (int col = 0; col < width; col++) {
          *dest_scan++ = src_scan[2];
          *dest_scan++ = src_scan[1];
          *dest_scan++ = src_scan[0];
          src_scan += 4;
        }
      }
    } else {
      ASSERT(FALSE);
    }
  } else if (dest_format == FXDIB_Argb || dest_format == FXDIB_Rgb32) {
    if (src_format == FXDIB_Rgb) {
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = (uint8_t*)(dest_buf + row * pitch);
        uint8_t* src_scan =
            (uint8_t*)pSrcBitmap->GetScanline(src_top + row) + src_left * 3;
        for (int col = 0; col < width; col++) {
          FXARGB_SETDIB(dest_scan, FXARGB_MAKE(0xff, src_scan[0], src_scan[1],
                                               src_scan[2]));
          dest_scan += 4;
          src_scan += 3;
        }
      }
    } else if (src_format == FXDIB_Rgb32) {
      ASSERT(dest_format == FXDIB_Argb);
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = dest_buf + row * pitch;
        uint8_t* src_scan =
            (uint8_t*)(pSrcBitmap->GetScanline(src_top + row) + src_left * 4);
        for (int col = 0; col < width; col++) {
          FXARGB_SETDIB(dest_scan, FXARGB_MAKE(0xff, src_scan[0], src_scan[1],
                                               src_scan[2]));
          src_scan += 4;
          dest_scan += 4;
        }
      }
    }
  } else {
    ASSERT(FALSE);
  }
}

// see https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line
SkScalar LineSide(const SkPoint line[2], const SkPoint& pt) {
  return (line[1].fY - line[0].fY) * pt.fX - (line[1].fX - line[0].fX) * pt.fY +
         line[1].fX * line[0].fY - line[1].fY * line[0].fX;
}

SkPoint IntersectSides(const SkPoint& parallelPt,
                       const SkVector& paraRay,
                       const SkPoint& perpendicularPt) {
  SkVector perpRay = {paraRay.fY, -paraRay.fX};
  SkScalar denom = perpRay.fY * paraRay.fX - paraRay.fY * perpRay.fX;
  if (!denom) {
    SkPoint zeroPt = {0, 0};
    return zeroPt;
  }
  SkVector ab0 = parallelPt - perpendicularPt;
  SkScalar numerA = ab0.fY * perpRay.fX - perpRay.fY * ab0.fX;
  numerA /= denom;
  SkPoint result = {parallelPt.fX + paraRay.fX * numerA,
                    parallelPt.fY + paraRay.fY * numerA};
  return result;
}

void ClipAngledGradient(const SkPoint pts[2],
                        SkPoint rectPts[4],
                        bool clipStart,
                        bool clipEnd,
                        SkPath* clip) {
  // find the corners furthest from the gradient perpendiculars
  SkScalar minPerpDist = SK_ScalarMax;
  SkScalar maxPerpDist = SK_ScalarMin;
  int minPerpPtIndex = -1;
  int maxPerpPtIndex = -1;
  SkVector slope = pts[1] - pts[0];
  SkPoint startPerp[2] = {pts[0], {pts[0].fX + slope.fY, pts[0].fY - slope.fX}};
  SkPoint endPerp[2] = {pts[1], {pts[1].fX + slope.fY, pts[1].fY - slope.fX}};
  for (int i = 0; i < 4; ++i) {
    SkScalar sDist = LineSide(startPerp, rectPts[i]);
    SkScalar eDist = LineSide(endPerp, rectPts[i]);
    if (sDist * eDist <= 0)  // if the signs are different,
      continue;              // the point is inside the gradient
    if (sDist < 0) {
      SkScalar smaller = SkTMin(sDist, eDist);
      if (minPerpDist > smaller) {
        minPerpDist = smaller;
        minPerpPtIndex = i;
      }
    } else {
      SkScalar larger = SkTMax(sDist, eDist);
      if (maxPerpDist < larger) {
        maxPerpDist = larger;
        maxPerpPtIndex = i;
      }
    }
  }
  if (minPerpPtIndex < 0 && maxPerpPtIndex < 0)  // nothing's outside
    return;
  // determine if negative distances are before start or after end
  SkPoint beforeStart = {pts[0].fX * 2 - pts[1].fX, pts[0].fY * 2 - pts[1].fY};
  bool beforeNeg = LineSide(startPerp, beforeStart) < 0;
  const SkPoint& startEdgePt =
      clipStart ? pts[0] : beforeNeg ? rectPts[minPerpPtIndex]
                                     : rectPts[maxPerpPtIndex];
  const SkPoint& endEdgePt = clipEnd ? pts[1] : beforeNeg
                                                    ? rectPts[maxPerpPtIndex]
                                                    : rectPts[minPerpPtIndex];
  // find the corners that bound the gradient
  SkScalar minDist = SK_ScalarMax;
  SkScalar maxDist = SK_ScalarMin;
  int minBounds = -1;
  int maxBounds = -1;
  for (int i = 0; i < 4; ++i) {
    SkScalar dist = LineSide(pts, rectPts[i]);
    if (minDist > dist) {
      minDist = dist;
      minBounds = i;
    }
    if (maxDist < dist) {
      maxDist = dist;
      maxBounds = i;
    }
  }
  ASSERT(minBounds >= 0);
  ASSERT(maxBounds != minBounds && maxBounds >= 0);
  // construct a clip parallel to the gradient that goes through
  // rectPts[minBounds] and rectPts[maxBounds] and perpendicular to the
  // gradient that goes through startEdgePt, endEdgePt.
  clip->moveTo(IntersectSides(rectPts[minBounds], slope, startEdgePt));
  clip->lineTo(IntersectSides(rectPts[minBounds], slope, endEdgePt));
  clip->lineTo(IntersectSides(rectPts[maxBounds], slope, endEdgePt));
  clip->lineTo(IntersectSides(rectPts[maxBounds], slope, startEdgePt));
}

}  // namespace

// convert a stroking path to scanlines
void CFX_SkiaDeviceDriver::PaintStroke(SkPaint* spaint,
                                       const CFX_GraphStateData* pGraphState,
                                       const SkMatrix& matrix) {
  SkPaint::Cap cap;
  switch (pGraphState->m_LineCap) {
    case CFX_GraphStateData::LineCapRound:
      cap = SkPaint::kRound_Cap;
      break;
    case CFX_GraphStateData::LineCapSquare:
      cap = SkPaint::kSquare_Cap;
      break;
    default:
      cap = SkPaint::kButt_Cap;
      break;
  }
  SkPaint::Join join;
  switch (pGraphState->m_LineJoin) {
    case CFX_GraphStateData::LineJoinRound:
      join = SkPaint::kRound_Join;
      break;
    case CFX_GraphStateData::LineJoinBevel:
      join = SkPaint::kBevel_Join;
      break;
    default:
      join = SkPaint::kMiter_Join;
      break;
  }
  SkMatrix inverse;
  if (!matrix.invert(&inverse))
    return;  // give up if the matrix is degenerate, and not invertable
  inverse.set(SkMatrix::kMTransX, 0);
  inverse.set(SkMatrix::kMTransY, 0);
  SkVector deviceUnits[2] = {{0, 1}, {1, 0}};
  inverse.mapPoints(deviceUnits, SK_ARRAY_COUNT(deviceUnits));
  FX_FLOAT width =
      SkTMax(pGraphState->m_LineWidth,
             SkTMin(deviceUnits[0].length(), deviceUnits[1].length()));
  if (pGraphState->m_DashArray) {
    int count = (pGraphState->m_DashCount + 1) / 2;
    SkScalar* intervals = FX_Alloc2D(SkScalar, count, sizeof(SkScalar));
    // Set dash pattern
    for (int i = 0; i < count; i++) {
      FX_FLOAT on = pGraphState->m_DashArray[i * 2];
      if (on <= 0.000001f)
        on = 1.f / 10;
      FX_FLOAT off = i * 2 + 1 == pGraphState->m_DashCount
                         ? on
                         : pGraphState->m_DashArray[i * 2 + 1];
      if (off < 0)
        off = 0;
      intervals[i * 2] = on;
      intervals[i * 2 + 1] = off;
    }
    spaint->setPathEffect(
        SkDashPathEffect::Make(intervals, count * 2, pGraphState->m_DashPhase));
  }
  spaint->setStyle(SkPaint::kStroke_Style);
  spaint->setAntiAlias(true);
  spaint->setStrokeWidth(width);
  spaint->setStrokeMiter(pGraphState->m_MiterLimit);
  spaint->setStrokeCap(cap);
  spaint->setStrokeJoin(join);
}

CFX_SkiaDeviceDriver::CFX_SkiaDeviceDriver(CFX_DIBitmap* pBitmap,
                                           FX_BOOL bRgbByteOrder,
                                           CFX_DIBitmap* pOriDevice,
                                           FX_BOOL bGroupKnockout)
    : m_pBitmap(pBitmap),
      m_pOriDevice(pOriDevice),
      m_pRecorder(nullptr),
      m_bRgbByteOrder(bRgbByteOrder),
      m_bGroupKnockout(bGroupKnockout) {
  SkBitmap skBitmap;
  SkASSERT(pBitmap->GetBPP() == 8 || pBitmap->GetBPP() == 32);
  SkImageInfo imageInfo = SkImageInfo::Make(
      pBitmap->GetWidth(), pBitmap->GetHeight(),
      pBitmap->GetBPP() == 8 ? kAlpha_8_SkColorType : kN32_SkColorType,
      kOpaque_SkAlphaType);
  skBitmap.installPixels(imageInfo, pBitmap->GetBuffer(), pBitmap->GetPitch(),
                         nullptr, /* to do : set color table */
                         nullptr, nullptr);
  m_pCanvas = new SkCanvas(skBitmap);
  if (m_bGroupKnockout)
    SkDebugf("");  // FIXME(caryclark) suppress 'm_bGroupKnockout is unused'
}

CFX_SkiaDeviceDriver::CFX_SkiaDeviceDriver(int size_x, int size_y)
    : m_pBitmap(nullptr),
      m_pOriDevice(nullptr),
      m_pRecorder(new SkPictureRecorder),
      m_bRgbByteOrder(FALSE),
      m_bGroupKnockout(FALSE) {
  m_pRecorder->beginRecording(SkIntToScalar(size_x), SkIntToScalar(size_y));
  m_pCanvas = m_pRecorder->getRecordingCanvas();
}

CFX_SkiaDeviceDriver::CFX_SkiaDeviceDriver(SkPictureRecorder* recorder)
    : m_pBitmap(nullptr),
      m_pOriDevice(nullptr),
      m_pRecorder(recorder),
      m_bRgbByteOrder(FALSE),
      m_bGroupKnockout(FALSE) {
  m_pCanvas = m_pRecorder->getRecordingCanvas();
}

CFX_SkiaDeviceDriver::~CFX_SkiaDeviceDriver() {
  if (!m_pRecorder)
    delete m_pCanvas;
}

FX_BOOL CFX_SkiaDeviceDriver::DrawDeviceText(int nChars,
                                             const FXTEXT_CHARPOS* pCharPos,
                                             CFX_Font* pFont,
                                             CFX_FontCache* pCache,
                                             const CFX_Matrix* pObject2Device,
                                             FX_FLOAT font_size,
                                             uint32_t color) {
  sk_sp<SkTypeface> typeface(SkSafeRef(pCache->GetDeviceCache(pFont)));
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  paint.setTypeface(typeface);
  paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
  paint.setTextSize(font_size);
  paint.setSubpixelText(true);
  m_pCanvas->save();
  SkMatrix skMatrix = ToFlippedSkMatrix(*pObject2Device);
  m_pCanvas->concat(skMatrix);
  SkTDArray<SkPoint> positions;
  positions.setCount(nChars);
  SkTDArray<uint16_t> glyphs;
  glyphs.setCount(nChars);
  for (int index = 0; index < nChars; ++index) {
    const FXTEXT_CHARPOS& cp = pCharPos[index];
    positions[index] = {cp.m_OriginX, cp.m_OriginY};
    glyphs[index] = (uint16_t)cp.m_GlyphIndex;
  }
  m_pCanvas->drawPosText(glyphs.begin(), nChars * 2, positions.begin(), paint);
  m_pCanvas->restore();
  return TRUE;
}

int CFX_SkiaDeviceDriver::GetDeviceCaps(int caps_id) const {
  switch (caps_id) {
    case FXDC_DEVICE_CLASS:
      return FXDC_DISPLAY;
    case FXDC_PIXEL_WIDTH:
      return m_pCanvas->imageInfo().width();
    case FXDC_PIXEL_HEIGHT:
      return m_pCanvas->imageInfo().height();
    case FXDC_BITS_PIXEL:
      return 32;
    case FXDC_HORZ_SIZE:
    case FXDC_VERT_SIZE:
      return 0;
    case FXDC_RENDER_CAPS:
      return FXRC_GET_BITS | FXRC_ALPHA_PATH | FXRC_ALPHA_IMAGE |
             FXRC_BLEND_MODE | FXRC_SOFT_CLIP | FXRC_ALPHA_OUTPUT |
             FXRC_FILLSTROKE_PATH | FXRC_SHADING;
  }
  return 0;
}

void CFX_SkiaDeviceDriver::SaveState() {
  m_pCanvas->save();
}

void CFX_SkiaDeviceDriver::RestoreState(bool bKeepSaved) {
  m_pCanvas->restore();
  if (bKeepSaved)
    m_pCanvas->save();
}

FX_BOOL CFX_SkiaDeviceDriver::SetClip_PathFill(
    const CFX_PathData* pPathData,     // path info
    const CFX_Matrix* pObject2Device,  // flips object's y-axis
    int fill_mode                      // fill mode, WINDING or ALTERNATE
    ) {
  if (pPathData->GetPointCount() == 5 || pPathData->GetPointCount() == 4) {
    CFX_FloatRect rectf;
    if (pPathData->IsRect(pObject2Device, &rectf)) {
      rectf.Intersect(
          CFX_FloatRect(0, 0, (FX_FLOAT)GetDeviceCaps(FXDC_PIXEL_WIDTH),
                        (FX_FLOAT)GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
      // note that PDF's y-axis goes up; Skia's y-axis goes down
      SkRect skClipRect =
          SkRect::MakeLTRB(rectf.left, rectf.bottom, rectf.right, rectf.top);
      DebugDrawSkiaClipRect(m_pCanvas, skClipRect);
      m_pCanvas->clipRect(skClipRect);
      return TRUE;
    }
  }
  SkPath skClipPath = BuildPath(pPathData);
  skClipPath.setFillType((fill_mode & 3) == FXFILL_WINDING
                             ? SkPath::kWinding_FillType
                             : SkPath::kEvenOdd_FillType);
  SkMatrix skMatrix = ToSkMatrix(*pObject2Device);
  skClipPath.transform(skMatrix);
  DebugShowSkiaPath(skClipPath);
  DebugDrawSkiaClipPath(m_pCanvas, skClipPath);
  m_pCanvas->clipPath(skClipPath);

  return TRUE;
}

FX_BOOL CFX_SkiaDeviceDriver::SetClip_PathStroke(
    const CFX_PathData* pPathData,         // path info
    const CFX_Matrix* pObject2Device,      // optional transformation
    const CFX_GraphStateData* pGraphState  // graphic state, for pen attributes
    ) {
  // build path data
  SkPath skPath = BuildPath(pPathData);
  skPath.setFillType(SkPath::kWinding_FillType);

  SkMatrix skMatrix = ToSkMatrix(*pObject2Device);
  SkPaint spaint;
  PaintStroke(&spaint, pGraphState, skMatrix);
  SkPath dst_path;
  spaint.getFillPath(skPath, &dst_path);
  dst_path.transform(skMatrix);
  DebugDrawSkiaClipPath(m_pCanvas, dst_path);
  m_pCanvas->clipPath(dst_path);
  return TRUE;
}

FX_BOOL CFX_SkiaDeviceDriver::DrawPath(
    const CFX_PathData* pPathData,          // path info
    const CFX_Matrix* pObject2Device,       // optional transformation
    const CFX_GraphStateData* pGraphState,  // graphic state, for pen attributes
    uint32_t fill_color,                    // fill color
    uint32_t stroke_color,                  // stroke color
    int fill_mode,  // fill mode, WINDING or ALTERNATE. 0 for not filled
    int blend_type) {
  SkIRect rect;
  rect.set(0, 0, GetDeviceCaps(FXDC_PIXEL_WIDTH),
           GetDeviceCaps(FXDC_PIXEL_HEIGHT));
  SkMatrix skMatrix;
  if (pObject2Device)
    skMatrix = ToSkMatrix(*pObject2Device);
  else
    skMatrix.setIdentity();
  SkPaint skPaint;
  skPaint.setAntiAlias(true);
  int stroke_alpha = FXARGB_A(stroke_color);
  if (pGraphState && stroke_alpha)
    PaintStroke(&skPaint, pGraphState, skMatrix);
  SkPath skPath = BuildPath(pPathData);
  m_pCanvas->save();
  m_pCanvas->concat(skMatrix);
  if ((fill_mode & 3) && fill_color) {
    skPath.setFillType((fill_mode & 3) == FXFILL_WINDING
                           ? SkPath::kWinding_FillType
                           : SkPath::kEvenOdd_FillType);
    SkPath strokePath;
    const SkPath* fillPath = &skPath;
    if (pGraphState && stroke_alpha) {
      SkAlpha fillA = SkColorGetA(fill_color);
      SkAlpha strokeA = SkColorGetA(stroke_color);
      if (fillA && fillA < 0xFF && strokeA && strokeA < 0xFF) {
        skPaint.getFillPath(skPath, &strokePath);
        if (Op(skPath, strokePath, SkPathOp::kDifference_SkPathOp,
               &strokePath)) {
          fillPath = &strokePath;
        }
      }
    }
    skPaint.setStyle(SkPaint::kFill_Style);
    skPaint.setColor(fill_color);
    m_pCanvas->drawPath(*fillPath, skPaint);
  }
  if (pGraphState && stroke_alpha) {
    DebugShowSkiaPath(skPath);
    DebugShowCanvasMatrix(m_pCanvas);
    skPaint.setStyle(SkPaint::kStroke_Style);
    skPaint.setColor(stroke_color);
    m_pCanvas->drawPath(skPath, skPaint);
  }
  m_pCanvas->restore();
  return TRUE;
}

FX_BOOL CFX_SkiaDeviceDriver::DrawCosmeticLine(FX_FLOAT x1,
                                               FX_FLOAT y1,
                                               FX_FLOAT x2,
                                               FX_FLOAT y2,
                                               uint32_t color,
                                               int blend_type) {
  return FALSE;
}

FX_BOOL CFX_SkiaDeviceDriver::FillRectWithBlend(const FX_RECT* pRect,
                                                uint32_t fill_color,
                                                int blend_type) {
  SkPaint spaint;
  spaint.setAntiAlias(true);
  spaint.setColor(fill_color);
  spaint.setXfermodeMode(GetSkiaBlendMode(blend_type));

  m_pCanvas->drawRect(
      SkRect::MakeLTRB(pRect->left, pRect->top, pRect->right, pRect->bottom),
      spaint);
  return TRUE;
}

FX_BOOL CFX_SkiaDeviceDriver::DrawShading(const CPDF_ShadingPattern* pPattern,
                                          const CFX_Matrix* pMatrix,
                                          const FX_RECT& clip_rect,
                                          int alpha,
                                          FX_BOOL bAlphaMode) {
  if (kAxialShading != pPattern->GetShadingType() &&
      kRadialShading != pPattern->GetShadingType()) {
    // TODO(caryclark) more types
    return false;
  }
  const std::vector<std::unique_ptr<CPDF_Function>>& pFuncs =
      pPattern->GetFuncs();
  int nFuncs = pFuncs.size();
  if (nFuncs != 1)  // TODO(caryclark) remove this restriction
    return false;
  CPDF_Dictionary* pDict = pPattern->GetShadingObject()->GetDict();
  CPDF_Array* pCoords = pDict->GetArrayBy("Coords");
  if (!pCoords)
    return true;
  // TODO(caryclark) Respect Domain[0], Domain[1]. (Don't know what they do
  // yet.)
  SkTDArray<SkColor> skColors;
  SkTDArray<SkScalar> skPos;
  for (int j = 0; j < nFuncs; j++) {
    if (!pFuncs[j])
      continue;

    if (const CPDF_SampledFunc* pSampledFunc = pFuncs[j]->ToSampledFunc()) {
      /* TODO(caryclark)
         Type 0 Sampled Functions in PostScript can also have an Order integer
         in the dictionary. PDFium doesn't appear to check for this anywhere.
       */
      if (!AddSamples(pSampledFunc, &skColors, &skPos))
        return false;
    } else if (const CPDF_ExpIntFunc* pExpIntFuc = pFuncs[j]->ToExpIntFunc()) {
      if (!AddColors(pExpIntFuc, &skColors))
        return false;
      skPos.push(0);
      skPos.push(1);
    } else if (const CPDF_StitchFunc* pStitchFunc = pFuncs[j]->ToStitchFunc()) {
      if (!AddStitching(pStitchFunc, &skColors, &skPos))
        return false;
    } else {
      return false;
    }
  }
  CPDF_Array* pArray = pDict->GetArrayBy("Extend");
  bool clipStart = !pArray || !pArray->GetIntegerAt(0);
  bool clipEnd = !pArray || !pArray->GetIntegerAt(1);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setAlpha(alpha);
  SkMatrix skMatrix = ToSkMatrix(*pMatrix);
  SkRect skRect = SkRect::MakeLTRB(clip_rect.left, clip_rect.top,
                                   clip_rect.right, clip_rect.bottom);
  SkPath skClip;
  SkPath skPath;
  if (kAxialShading == pPattern->GetShadingType()) {
    FX_FLOAT start_x = pCoords->GetNumberAt(0);
    FX_FLOAT start_y = pCoords->GetNumberAt(1);
    FX_FLOAT end_x = pCoords->GetNumberAt(2);
    FX_FLOAT end_y = pCoords->GetNumberAt(3);
    SkPoint pts[] = {{start_x, start_y}, {end_x, end_y}};
    skMatrix.mapPoints(pts, SK_ARRAY_COUNT(pts));
    paint.setShader(SkGradientShader::MakeLinear(
        pts, skColors.begin(), skPos.begin(), skColors.count(),
        SkShader::kClamp_TileMode));
    if (clipStart || clipEnd) {
      // if the gradient is horizontal or vertical, modify the draw rectangle
      if (pts[0].fX == pts[1].fX) {  // vertical
        if (pts[0].fY > pts[1].fY) {
          SkTSwap(pts[0].fY, pts[1].fY);
          SkTSwap(clipStart, clipEnd);
        }
        if (clipStart)
          skRect.fTop = SkTMax(skRect.fTop, pts[0].fY);
        if (clipEnd)
          skRect.fBottom = SkTMin(skRect.fBottom, pts[1].fY);
      } else if (pts[0].fY == pts[1].fY) {  // horizontal
        if (pts[0].fX > pts[1].fX) {
          SkTSwap(pts[0].fX, pts[1].fX);
          SkTSwap(clipStart, clipEnd);
        }
        if (clipStart)
          skRect.fLeft = SkTMax(skRect.fLeft, pts[0].fX);
        if (clipEnd)
          skRect.fRight = SkTMin(skRect.fRight, pts[1].fX);
      } else {  // if the gradient is angled and contained by the rect, clip
        SkPoint rectPts[4] = {{skRect.fLeft, skRect.fTop},
                              {skRect.fRight, skRect.fTop},
                              {skRect.fRight, skRect.fBottom},
                              {skRect.fLeft, skRect.fBottom}};
        ClipAngledGradient(pts, rectPts, clipStart, clipEnd, &skClip);
      }
    }
    skPath.addRect(skRect);
    skMatrix.setIdentity();
  } else {
    ASSERT(kRadialShading == pPattern->GetShadingType());
    FX_FLOAT start_x = pCoords->GetNumberAt(0);
    FX_FLOAT start_y = pCoords->GetNumberAt(1);
    FX_FLOAT start_r = pCoords->GetNumberAt(2);
    FX_FLOAT end_x = pCoords->GetNumberAt(3);
    FX_FLOAT end_y = pCoords->GetNumberAt(4);
    FX_FLOAT end_r = pCoords->GetNumberAt(5);
    SkPoint pts[] = {{start_x, start_y}, {end_x, end_y}};

    paint.setShader(SkGradientShader::MakeTwoPointConical(
        pts[0], start_r, pts[1], end_r, skColors.begin(), skPos.begin(),
        skColors.count(), SkShader::kClamp_TileMode));
    if (clipStart || clipEnd) {
      if (clipStart && start_r)
        skClip.addCircle(pts[0].fX, pts[0].fY, start_r);
      if (clipEnd)
        skClip.addCircle(pts[1].fX, pts[1].fY, end_r, SkPath::kCCW_Direction);
      else
        skClip.setFillType(SkPath::kInverseWinding_FillType);
      skClip.transform(skMatrix);
    }
    SkMatrix inverse;
    if (!skMatrix.invert(&inverse))
      return false;
    skPath.addRect(skRect);
    skPath.transform(inverse);
  }
  m_pCanvas->save();
  if (!skClip.isEmpty())
    m_pCanvas->clipPath(skClip);
  m_pCanvas->concat(skMatrix);
  m_pCanvas->drawPath(skPath, paint);
  m_pCanvas->restore();
  return true;
}

uint8_t* CFX_SkiaDeviceDriver::GetBuffer() const {
  return m_pBitmap->GetBuffer();
}

FX_BOOL CFX_SkiaDeviceDriver::GetClipBox(FX_RECT* pRect) {
  // TODO(caryclark) call m_canvas->getClipDeviceBounds() instead
  pRect->left = 0;
  pRect->top = 0;
  const SkImageInfo& canvasSize = m_pCanvas->imageInfo();
  pRect->right = canvasSize.width();
  pRect->bottom = canvasSize.height();
  return TRUE;
}

FX_BOOL CFX_SkiaDeviceDriver::GetDIBits(CFX_DIBitmap* pBitmap,
                                        int left,
                                        int top) {
  if (!m_pBitmap || !m_pBitmap->GetBuffer())
    return TRUE;

  FX_RECT rect(left, top, left + pBitmap->GetWidth(),
               top + pBitmap->GetHeight());
  std::unique_ptr<CFX_DIBitmap> pBack;
  if (m_pOriDevice) {
    pBack.reset(m_pOriDevice->Clone(&rect));
    if (!pBack)
      return TRUE;

    pBack->CompositeBitmap(0, 0, pBack->GetWidth(), pBack->GetHeight(),
                           m_pBitmap, 0, 0);
  } else {
    pBack.reset(m_pBitmap->Clone(&rect));
    if (!pBack)
      return TRUE;
  }

  left = std::min(left, 0);
  top = std::min(top, 0);
  if (m_bRgbByteOrder) {
    RgbByteOrderTransferBitmap(pBitmap, 0, 0, rect.Width(), rect.Height(),
                               pBack.get(), left, top);
    return TRUE;
  }

  return pBitmap->TransferBitmap(0, 0, rect.Width(), rect.Height(), pBack.get(),
                                 left, top);
}

CFX_DIBitmap* CFX_SkiaDeviceDriver::GetBackDrop() {
  return m_pOriDevice;
}

FX_BOOL CFX_SkiaDeviceDriver::SetDIBits(const CFX_DIBSource* pBitmap,
                                        uint32_t argb,
                                        const FX_RECT* pSrcRect,
                                        int left,
                                        int top,
                                        int blend_type) {
  if (!m_pBitmap || !m_pBitmap->GetBuffer())
    return TRUE;

  CFX_Matrix m(pBitmap->GetWidth(), 0, 0, -pBitmap->GetHeight(), left,
               top + pBitmap->GetHeight());
  void* dummy;
  return StartDIBits(pBitmap, 0xFF, argb, &m, 0, dummy, blend_type);
}

FX_BOOL CFX_SkiaDeviceDriver::StretchDIBits(const CFX_DIBSource* pSource,
                                            uint32_t argb,
                                            int dest_left,
                                            int dest_top,
                                            int dest_width,
                                            int dest_height,
                                            const FX_RECT* pClipRect,
                                            uint32_t flags,
                                            int blend_type) {
  if (!m_pBitmap->GetBuffer())
    return TRUE;
  CFX_Matrix m(dest_width, 0, 0, -dest_height, dest_left,
               dest_top + dest_height);

  m_pCanvas->save();
  SkRect skClipRect = SkRect::MakeLTRB(pClipRect->left, pClipRect->bottom,
                                       pClipRect->right, pClipRect->top);
  m_pCanvas->clipRect(skClipRect);
  void* dummy;
  FX_BOOL result = StartDIBits(pSource, 0xFF, argb, &m, 0, dummy, blend_type);
  m_pCanvas->restore();

  return result;
}

FX_BOOL CFX_SkiaDeviceDriver::StartDIBits(const CFX_DIBSource* pSource,
                                          int bitmap_alpha,
                                          uint32_t argb,
                                          const CFX_Matrix* pMatrix,
                                          uint32_t render_flags,
                                          void*& handle,
                                          int blend_type) {
  DebugValidate(m_pBitmap, m_pOriDevice);
  SkColorType colorType = pSource->IsAlphaMask()
                              ? SkColorType::kAlpha_8_SkColorType
                              : SkColorType::kGray_8_SkColorType;
  SkAlphaType alphaType =
      pSource->IsAlphaMask() ? kPremul_SkAlphaType : kOpaque_SkAlphaType;
  SkColorTable* ct = nullptr;
  void* buffer = pSource->GetBuffer();
  std::unique_ptr<uint8_t, FxFreeDeleter> dst8Storage;
  std::unique_ptr<uint32_t, FxFreeDeleter> dst32Storage;
  int width = pSource->GetWidth();
  int height = pSource->GetHeight();
  int rowBytes = pSource->GetPitch();
  switch (pSource->GetBPP()) {
    case 1: {
      dst8Storage.reset(FX_Alloc2D(uint8_t, width, height));
      uint8_t* dst8Pixels = dst8Storage.get();
      for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow =
            static_cast<const uint8_t*>(buffer) + y * rowBytes;
        uint8_t* dstRow = dst8Pixels + y * width;
        for (int x = 0; x < width; ++x)
          dstRow[x] = srcRow[x >> 3] & (1 << (~x & 0x07)) ? 0xFF : 0x00;
      }
      buffer = dst8Storage.get();
      rowBytes = width;
    } break;
    case 8:
      if (pSource->GetPalette()) {
        ct = new SkColorTable(pSource->GetPalette(), pSource->GetPaletteSize());
        colorType = SkColorType::kIndex_8_SkColorType;
      }
      break;
    case 24: {
      dst32Storage.reset(FX_Alloc2D(uint32_t, width, height));
      uint32_t* dst32Pixels = dst32Storage.get();
      for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow =
            static_cast<const uint8_t*>(buffer) + y * rowBytes;
        uint32_t* dstRow = dst32Pixels + y * width;
        for (int x = 0; x < width; ++x) {
          dstRow[x] = SkPackARGB32(0xFF, srcRow[x * 3 + 2], srcRow[x * 3 + 1],
                                   srcRow[x * 3 + 0]);
        }
      }
      buffer = dst32Storage.get();
      rowBytes = width * sizeof(uint32_t);
      colorType = SkColorType::kN32_SkColorType;
      alphaType = kOpaque_SkAlphaType;
    } break;
    case 32:
      colorType = SkColorType::kN32_SkColorType;
      alphaType = kPremul_SkAlphaType;
      DebugVerifyBitmapIsPreMultiplied(buffer, width, height);
      break;
    default:
      SkASSERT(0);  // TODO(caryclark) ensure that all cases are covered
      colorType = SkColorType::kUnknown_SkColorType;
  }
  SkImageInfo imageInfo =
      SkImageInfo::Make(width, height, colorType, alphaType);
  SkBitmap skBitmap;
  skBitmap.installPixels(imageInfo, buffer, rowBytes, ct, nullptr, nullptr);
  m_pCanvas->save();
  SkMatrix skMatrix;
  const CFX_Matrix& m = *pMatrix;
  skMatrix.setAll(m.a / width, -m.c / height, m.c + m.e, m.b / width,
                  -m.d / height, m.d + m.f, 0, 0, 1);
  m_pCanvas->concat(skMatrix);
  SkPaint paint;
  paint.setAntiAlias(true);
  if (pSource->IsAlphaMask()) {
    paint.setColorFilter(
        SkColorFilter::MakeModeFilter(argb, SkXfermode::kSrc_Mode));
  }
  // paint.setFilterQuality(kHigh_SkFilterQuality);
  paint.setXfermodeMode(GetSkiaBlendMode(blend_type));
  paint.setAlpha(bitmap_alpha);
  m_pCanvas->drawBitmap(skBitmap, 0, 0, &paint);
  m_pCanvas->restore();
  if (ct)
    ct->unref();
  DebugValidate(m_pBitmap, m_pOriDevice);
  return TRUE;
}

FX_BOOL CFX_SkiaDeviceDriver::ContinueDIBits(void* handle, IFX_Pause* pPause) {
  return FALSE;
}

void CFX_SkiaDeviceDriver::PreMultiply() {
  void* buffer = m_pBitmap->GetBuffer();
  if (!buffer)
    return;
  if (m_pBitmap->GetBPP() != 32) {
    return;
  }
  int height = m_pBitmap->GetHeight();
  int width = m_pBitmap->GetWidth();
  int rowBytes = m_pBitmap->GetPitch();
  SkImageInfo unpremultipliedInfo =
      SkImageInfo::Make(width, height, kN32_SkColorType, kUnpremul_SkAlphaType);
  SkPixmap unpremultiplied(unpremultipliedInfo, buffer, rowBytes);
  SkImageInfo premultipliedInfo =
      SkImageInfo::Make(width, height, kN32_SkColorType, kPremul_SkAlphaType);
  SkPixmap premultiplied(premultipliedInfo, buffer, rowBytes);
  unpremultiplied.readPixels(premultiplied);
  DebugVerifyBitmapIsPreMultiplied(buffer, width, height);
}

CFX_FxgeDevice::CFX_FxgeDevice() {
  m_bOwnedBitmap = FALSE;
}

SkPictureRecorder* CFX_FxgeDevice::CreateRecorder(int size_x, int size_y) {
  CFX_SkiaDeviceDriver* skDriver = new CFX_SkiaDeviceDriver(size_x, size_y);
  SetDeviceDriver(skDriver);
  return skDriver->GetRecorder();
}

bool CFX_FxgeDevice::Attach(CFX_DIBitmap* pBitmap,
                            bool bRgbByteOrder,
                            CFX_DIBitmap* pOriDevice,
                            bool bGroupKnockout) {
  if (!pBitmap)
    return false;
  SetBitmap(pBitmap);
  SetDeviceDriver(new CFX_SkiaDeviceDriver(pBitmap, bRgbByteOrder, pOriDevice,
                                           bGroupKnockout));
  return true;
}

bool CFX_FxgeDevice::AttachRecorder(SkPictureRecorder* recorder) {
  if (!recorder)
    return false;
  SetDeviceDriver(new CFX_SkiaDeviceDriver(recorder));
  return true;
}

bool CFX_FxgeDevice::Create(int width,
                            int height,
                            FXDIB_Format format,
                            CFX_DIBitmap* pOriDevice) {
  m_bOwnedBitmap = TRUE;
  CFX_DIBitmap* pBitmap = new CFX_DIBitmap;
  if (!pBitmap->Create(width, height, format)) {
    delete pBitmap;
    return false;
  }
  SetBitmap(pBitmap);
  CFX_SkiaDeviceDriver* pDriver =
      new CFX_SkiaDeviceDriver(pBitmap, FALSE, pOriDevice, FALSE);
  SetDeviceDriver(pDriver);
  return true;
}

CFX_FxgeDevice::~CFX_FxgeDevice() {
  if (m_bOwnedBitmap && GetBitmap())
    delete GetBitmap();
}

void CFX_FxgeDevice::PreMultiply() {
  (static_cast<CFX_SkiaDeviceDriver*>(GetDeviceDriver()))->PreMultiply();
}

#endif
