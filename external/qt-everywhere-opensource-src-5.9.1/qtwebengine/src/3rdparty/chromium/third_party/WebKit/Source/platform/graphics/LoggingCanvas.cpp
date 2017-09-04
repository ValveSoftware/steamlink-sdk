/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "platform/graphics/LoggingCanvas.h"

#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/skia/ImagePixelLocker.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/image-encoders/PNGImageEncoder.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "wtf/HexNumber.h"
#include "wtf/text/Base64.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

namespace {

struct VerbParams {
  STACK_ALLOCATED();
  String name;
  unsigned pointCount;
  unsigned pointOffset;

  VerbParams(const String& name, unsigned pointCount, unsigned pointOffset)
      : name(name), pointCount(pointCount), pointOffset(pointOffset) {}
};

std::unique_ptr<JSONObject> objectForSkRect(const SkRect& rect) {
  std::unique_ptr<JSONObject> rectItem = JSONObject::create();
  rectItem->setDouble("left", rect.left());
  rectItem->setDouble("top", rect.top());
  rectItem->setDouble("right", rect.right());
  rectItem->setDouble("bottom", rect.bottom());
  return rectItem;
}

std::unique_ptr<JSONObject> objectForSkIRect(const SkIRect& rect) {
  std::unique_ptr<JSONObject> rectItem = JSONObject::create();
  rectItem->setDouble("left", rect.left());
  rectItem->setDouble("top", rect.top());
  rectItem->setDouble("right", rect.right());
  rectItem->setDouble("bottom", rect.bottom());
  return rectItem;
}

String pointModeName(SkCanvas::PointMode mode) {
  switch (mode) {
    case SkCanvas::kPoints_PointMode:
      return "Points";
    case SkCanvas::kLines_PointMode:
      return "Lines";
    case SkCanvas::kPolygon_PointMode:
      return "Polygon";
    default:
      ASSERT_NOT_REACHED();
      return "?";
  };
}

std::unique_ptr<JSONObject> objectForSkPoint(const SkPoint& point) {
  std::unique_ptr<JSONObject> pointItem = JSONObject::create();
  pointItem->setDouble("x", point.x());
  pointItem->setDouble("y", point.y());
  return pointItem;
}

std::unique_ptr<JSONArray> arrayForSkPoints(size_t count,
                                            const SkPoint points[]) {
  std::unique_ptr<JSONArray> pointsArrayItem = JSONArray::create();
  for (size_t i = 0; i < count; ++i)
    pointsArrayItem->pushObject(objectForSkPoint(points[i]));
  return pointsArrayItem;
}

std::unique_ptr<JSONObject> objectForRadius(const SkRRect& rrect,
                                            SkRRect::Corner corner) {
  std::unique_ptr<JSONObject> radiusItem = JSONObject::create();
  SkVector radius = rrect.radii(corner);
  radiusItem->setDouble("xRadius", radius.x());
  radiusItem->setDouble("yRadius", radius.y());
  return radiusItem;
}

String rrectTypeName(SkRRect::Type type) {
  switch (type) {
    case SkRRect::kEmpty_Type:
      return "Empty";
    case SkRRect::kRect_Type:
      return "Rect";
    case SkRRect::kOval_Type:
      return "Oval";
    case SkRRect::kSimple_Type:
      return "Simple";
    case SkRRect::kNinePatch_Type:
      return "Nine-patch";
    case SkRRect::kComplex_Type:
      return "Complex";
    default:
      ASSERT_NOT_REACHED();
      return "?";
  };
}

String radiusName(SkRRect::Corner corner) {
  switch (corner) {
    case SkRRect::kUpperLeft_Corner:
      return "upperLeftRadius";
    case SkRRect::kUpperRight_Corner:
      return "upperRightRadius";
    case SkRRect::kLowerRight_Corner:
      return "lowerRightRadius";
    case SkRRect::kLowerLeft_Corner:
      return "lowerLeftRadius";
    default:
      ASSERT_NOT_REACHED();
      return "?";
  }
}

std::unique_ptr<JSONObject> objectForSkRRect(const SkRRect& rrect) {
  std::unique_ptr<JSONObject> rrectItem = JSONObject::create();
  rrectItem->setString("type", rrectTypeName(rrect.type()));
  rrectItem->setDouble("left", rrect.rect().left());
  rrectItem->setDouble("top", rrect.rect().top());
  rrectItem->setDouble("right", rrect.rect().right());
  rrectItem->setDouble("bottom", rrect.rect().bottom());
  for (int i = 0; i < 4; ++i)
    rrectItem->setObject(radiusName((SkRRect::Corner)i),
                         objectForRadius(rrect, (SkRRect::Corner)i));
  return rrectItem;
}

String fillTypeName(SkPath::FillType type) {
  switch (type) {
    case SkPath::kWinding_FillType:
      return "Winding";
    case SkPath::kEvenOdd_FillType:
      return "EvenOdd";
    case SkPath::kInverseWinding_FillType:
      return "InverseWinding";
    case SkPath::kInverseEvenOdd_FillType:
      return "InverseEvenOdd";
    default:
      ASSERT_NOT_REACHED();
      return "?";
  };
}

String convexityName(SkPath::Convexity convexity) {
  switch (convexity) {
    case SkPath::kUnknown_Convexity:
      return "Unknown";
    case SkPath::kConvex_Convexity:
      return "Convex";
    case SkPath::kConcave_Convexity:
      return "Concave";
    default:
      ASSERT_NOT_REACHED();
      return "?";
  };
}

VerbParams segmentParams(SkPath::Verb verb) {
  switch (verb) {
    case SkPath::kMove_Verb:
      return VerbParams("Move", 1, 0);
    case SkPath::kLine_Verb:
      return VerbParams("Line", 1, 1);
    case SkPath::kQuad_Verb:
      return VerbParams("Quad", 2, 1);
    case SkPath::kConic_Verb:
      return VerbParams("Conic", 2, 1);
    case SkPath::kCubic_Verb:
      return VerbParams("Cubic", 3, 1);
    case SkPath::kClose_Verb:
      return VerbParams("Close", 0, 0);
    case SkPath::kDone_Verb:
      return VerbParams("Done", 0, 0);
    default:
      ASSERT_NOT_REACHED();
      return VerbParams("?", 0, 0);
  };
}

std::unique_ptr<JSONObject> objectForSkPath(const SkPath& path) {
  std::unique_ptr<JSONObject> pathItem = JSONObject::create();
  pathItem->setString("fillType", fillTypeName(path.getFillType()));
  pathItem->setString("convexity", convexityName(path.getConvexity()));
  pathItem->setBoolean("isRect", path.isRect(0));
  SkPath::Iter iter(path, false);
  SkPoint points[4];
  std::unique_ptr<JSONArray> pathPointsArray = JSONArray::create();
  for (SkPath::Verb verb = iter.next(points, false); verb != SkPath::kDone_Verb;
       verb = iter.next(points, false)) {
    VerbParams verbParams = segmentParams(verb);
    std::unique_ptr<JSONObject> pathPointItem = JSONObject::create();
    pathPointItem->setString("verb", verbParams.name);
    ASSERT(verbParams.pointCount + verbParams.pointOffset <=
           WTF_ARRAY_LENGTH(points));
    pathPointItem->setArray("points",
                            arrayForSkPoints(verbParams.pointCount,
                                             points + verbParams.pointOffset));
    if (SkPath::kConic_Verb == verb)
      pathPointItem->setDouble("conicWeight", iter.conicWeight());
    pathPointsArray->pushObject(std::move(pathPointItem));
  }
  pathItem->setArray("pathPoints", std::move(pathPointsArray));
  pathItem->setObject("bounds", objectForSkRect(path.getBounds()));
  return pathItem;
}

String colorTypeName(SkColorType colorType) {
  switch (colorType) {
    case kUnknown_SkColorType:
      return "None";
    case kAlpha_8_SkColorType:
      return "A8";
    case kIndex_8_SkColorType:
      return "Index8";
    case kRGB_565_SkColorType:
      return "RGB565";
    case kARGB_4444_SkColorType:
      return "ARGB4444";
    case kN32_SkColorType:
      return "ARGB8888";
    default:
      ASSERT_NOT_REACHED();
      return "?";
  };
}

std::unique_ptr<JSONObject> objectForBitmapData(const SkBitmap& bitmap) {
  Vector<unsigned char> output;

  if (sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap)) {
    ImagePixelLocker pixelLocker(image, kUnpremul_SkAlphaType,
                                 kRGBA_8888_SkColorType);
    ImageDataBuffer imageData(
        IntSize(image->width(), image->height()),
        static_cast<const unsigned char*>(pixelLocker.pixels()));

    PNGImageEncoder::encode(imageData, &output);
  }

  std::unique_ptr<JSONObject> dataItem = JSONObject::create();
  dataItem->setString(
      "base64",
      WTF::base64Encode(reinterpret_cast<char*>(output.data()), output.size()));
  dataItem->setString("mimeType", "image/png");
  return dataItem;
}

std::unique_ptr<JSONObject> objectForSkBitmap(const SkBitmap& bitmap) {
  std::unique_ptr<JSONObject> bitmapItem = JSONObject::create();
  bitmapItem->setInteger("width", bitmap.width());
  bitmapItem->setInteger("height", bitmap.height());
  bitmapItem->setString("config", colorTypeName(bitmap.colorType()));
  bitmapItem->setBoolean("opaque", bitmap.isOpaque());
  bitmapItem->setBoolean("immutable", bitmap.isImmutable());
  bitmapItem->setBoolean("volatile", bitmap.isVolatile());
  bitmapItem->setInteger("genID", bitmap.getGenerationID());
  bitmapItem->setObject("data", objectForBitmapData(bitmap));
  return bitmapItem;
}

std::unique_ptr<JSONObject> objectForSkImage(const SkImage* image) {
  std::unique_ptr<JSONObject> imageItem = JSONObject::create();
  imageItem->setInteger("width", image->width());
  imageItem->setInteger("height", image->height());
  imageItem->setBoolean("opaque", image->isOpaque());
  imageItem->setInteger("uniqueID", image->uniqueID());
  return imageItem;
}

std::unique_ptr<JSONArray> arrayForSkMatrix(const SkMatrix& matrix) {
  std::unique_ptr<JSONArray> matrixArray = JSONArray::create();
  for (int i = 0; i < 9; ++i)
    matrixArray->pushDouble(matrix[i]);
  return matrixArray;
}

std::unique_ptr<JSONObject> objectForSkShader(const SkShader& shader) {
  std::unique_ptr<JSONObject> shaderItem = JSONObject::create();
  const SkMatrix localMatrix = shader.getLocalMatrix();
  if (!localMatrix.isIdentity())
    shaderItem->setArray("localMatrix", arrayForSkMatrix(localMatrix));
  return shaderItem;
}

String stringForSkColor(const SkColor& color) {
  // #AARRGGBB.
  Vector<LChar, 9> result;
  result.append('#');
  appendUnsignedAsHex(color, result);
  return String(result.data(), result.size());
}

void appendFlagToString(String* flagsString, bool isSet, const String& name) {
  if (!isSet)
    return;
  if (flagsString->length())
    flagsString->append("|");
  flagsString->append(name);
}

String stringForSkPaintFlags(const SkPaint& paint) {
  if (!paint.getFlags())
    return "none";
  String flagsString = "";
  appendFlagToString(&flagsString, paint.isAntiAlias(), "AntiAlias");
  appendFlagToString(&flagsString, paint.isDither(), "Dither");
  appendFlagToString(&flagsString, paint.isUnderlineText(), "UnderlinText");
  appendFlagToString(&flagsString, paint.isStrikeThruText(), "StrikeThruText");
  appendFlagToString(&flagsString, paint.isFakeBoldText(), "FakeBoldText");
  appendFlagToString(&flagsString, paint.isLinearText(), "LinearText");
  appendFlagToString(&flagsString, paint.isSubpixelText(), "SubpixelText");
  appendFlagToString(&flagsString, paint.isDevKernText(), "DevKernText");
  appendFlagToString(&flagsString, paint.isLCDRenderText(), "LCDRenderText");
  appendFlagToString(&flagsString, paint.isEmbeddedBitmapText(),
                     "EmbeddedBitmapText");
  appendFlagToString(&flagsString, paint.isAutohinted(), "Autohinted");
  appendFlagToString(&flagsString, paint.isVerticalText(), "VerticalText");
  return flagsString;
}

String filterQualityName(SkFilterQuality filterQuality) {
  switch (filterQuality) {
    case kNone_SkFilterQuality:
      return "None";
    case kLow_SkFilterQuality:
      return "Low";
    case kMedium_SkFilterQuality:
      return "Medium";
    case kHigh_SkFilterQuality:
      return "High";
    default:
      ASSERT_NOT_REACHED();
      return "?";
  };
}

String textAlignName(SkPaint::Align align) {
  switch (align) {
    case SkPaint::kLeft_Align:
      return "Left";
    case SkPaint::kCenter_Align:
      return "Center";
    case SkPaint::kRight_Align:
      return "Right";
    default:
      ASSERT_NOT_REACHED();
      return "?";
  };
}

String strokeCapName(SkPaint::Cap cap) {
  switch (cap) {
    case SkPaint::kButt_Cap:
      return "Butt";
    case SkPaint::kRound_Cap:
      return "Round";
    case SkPaint::kSquare_Cap:
      return "Square";
    default:
      ASSERT_NOT_REACHED();
      return "?";
  };
}

String strokeJoinName(SkPaint::Join join) {
  switch (join) {
    case SkPaint::kMiter_Join:
      return "Miter";
    case SkPaint::kRound_Join:
      return "Round";
    case SkPaint::kBevel_Join:
      return "Bevel";
    default:
      ASSERT_NOT_REACHED();
      return "?";
  };
}

String styleName(SkPaint::Style style) {
  switch (style) {
    case SkPaint::kFill_Style:
      return "Fill";
    case SkPaint::kStroke_Style:
      return "Stroke";
    case SkPaint::kStrokeAndFill_Style:
      return "StrokeAndFill";
    default:
      ASSERT_NOT_REACHED();
      return "?";
  };
}

String textEncodingName(SkPaint::TextEncoding encoding) {
  switch (encoding) {
    case SkPaint::kUTF8_TextEncoding:
      return "UTF-8";
    case SkPaint::kUTF16_TextEncoding:
      return "UTF-16";
    case SkPaint::kUTF32_TextEncoding:
      return "UTF-32";
    case SkPaint::kGlyphID_TextEncoding:
      return "GlyphID";
    default:
      ASSERT_NOT_REACHED();
      return "?";
  };
}

String hintingName(SkPaint::Hinting hinting) {
  switch (hinting) {
    case SkPaint::kNo_Hinting:
      return "None";
    case SkPaint::kSlight_Hinting:
      return "Slight";
    case SkPaint::kNormal_Hinting:
      return "Normal";
    case SkPaint::kFull_Hinting:
      return "Full";
    default:
      ASSERT_NOT_REACHED();
      return "?";
  };
}

std::unique_ptr<JSONObject> objectForSkPaint(const SkPaint& paint) {
  std::unique_ptr<JSONObject> paintItem = JSONObject::create();
  paintItem->setDouble("textSize", paint.getTextSize());
  paintItem->setDouble("textScaleX", paint.getTextScaleX());
  paintItem->setDouble("textSkewX", paint.getTextSkewX());
  if (SkShader* shader = paint.getShader())
    paintItem->setObject("shader", objectForSkShader(*shader));
  paintItem->setString("color", stringForSkColor(paint.getColor()));
  paintItem->setDouble("strokeWidth", paint.getStrokeWidth());
  paintItem->setDouble("strokeMiter", paint.getStrokeMiter());
  paintItem->setString("flags", stringForSkPaintFlags(paint));
  paintItem->setString("filterLevel",
                       filterQualityName(paint.getFilterQuality()));
  paintItem->setString("textAlign", textAlignName(paint.getTextAlign()));
  paintItem->setString("strokeCap", strokeCapName(paint.getStrokeCap()));
  paintItem->setString("strokeJoin", strokeJoinName(paint.getStrokeJoin()));
  paintItem->setString("styleName", styleName(paint.getStyle()));
  paintItem->setString("textEncoding",
                       textEncodingName(paint.getTextEncoding()));
  paintItem->setString("hinting", hintingName(paint.getHinting()));
  return paintItem;
}

std::unique_ptr<JSONArray> arrayForSkScalars(size_t n,
                                             const SkScalar scalars[]) {
  std::unique_ptr<JSONArray> scalarsArray = JSONArray::create();
  for (size_t i = 0; i < n; ++i)
    scalarsArray->pushDouble(scalars[i]);
  return scalarsArray;
}

String regionOpName(SkRegion::Op op) {
  switch (op) {
    case SkRegion::kDifference_Op:
      return "kDifference_Op";
    case SkRegion::kIntersect_Op:
      return "kIntersect_Op";
    case SkRegion::kUnion_Op:
      return "kUnion_Op";
    case SkRegion::kXOR_Op:
      return "kXOR_Op";
    case SkRegion::kReverseDifference_Op:
      return "kReverseDifference_Op";
    case SkRegion::kReplace_Op:
      return "kReplace_Op";
    default:
      return "Unknown type";
  };
}

String saveLayerFlagsToString(SkCanvas::SaveLayerFlags flags) {
  String flagsString = "";
  if (flags & SkCanvas::kIsOpaque_SaveLayerFlag)
    flagsString.append("kIsOpaque_SaveLayerFlag ");
  if (flags & SkCanvas::kPreserveLCDText_SaveLayerFlag)
    flagsString.append("kPreserveLCDText_SaveLayerFlag ");
  return flagsString;
}

String textEncodingCanonicalName(SkPaint::TextEncoding encoding) {
  String name = textEncodingName(encoding);
  if (encoding == SkPaint::kUTF16_TextEncoding ||
      encoding == SkPaint::kUTF32_TextEncoding)
    name.append("LE");
  return name;
}

String stringForUTFText(const void* text,
                        size_t length,
                        SkPaint::TextEncoding encoding) {
  return WTF::TextEncoding(textEncodingCanonicalName(encoding))
      .decode((const char*)text, length);
}

String stringForText(const void* text,
                     size_t byteLength,
                     const SkPaint& paint) {
  SkPaint::TextEncoding encoding = paint.getTextEncoding();
  switch (encoding) {
    case SkPaint::kUTF8_TextEncoding:
    case SkPaint::kUTF16_TextEncoding:
    case SkPaint::kUTF32_TextEncoding:
      return stringForUTFText(text, byteLength, encoding);
    case SkPaint::kGlyphID_TextEncoding: {
      WTF::Vector<SkUnichar> dataVector(byteLength / 2);
      SkUnichar* textData = dataVector.data();
      paint.glyphsToUnichars(static_cast<const uint16_t*>(text), byteLength / 2,
                             textData);
      return WTF::UTF32LittleEndianEncoding().decode(
          reinterpret_cast<const char*>(textData), byteLength * 2);
    }
    default:
      ASSERT_NOT_REACHED();
      return "?";
  }
}

}  // namespace

class AutoLogger
    : InterceptingCanvasBase::CanvasInterceptorBase<LoggingCanvas> {
 public:
  explicit AutoLogger(LoggingCanvas* canvas)
      : InterceptingCanvasBase::CanvasInterceptorBase<LoggingCanvas>(canvas) {}

  JSONObject* logItem(const String& name);
  JSONObject* logItemWithParams(const String& name);
  ~AutoLogger() {
    if (topLevelCall())
      canvas()->m_log->pushObject(std::move(m_logItem));
  }

 private:
  std::unique_ptr<JSONObject> m_logItem;
};

JSONObject* AutoLogger::logItem(const String& name) {
  std::unique_ptr<JSONObject> item = JSONObject::create();
  item->setString("method", name);
  m_logItem = std::move(item);
  return m_logItem.get();
}

JSONObject* AutoLogger::logItemWithParams(const String& name) {
  JSONObject* item = logItem(name);
  std::unique_ptr<JSONObject> params = JSONObject::create();
  item->setObject("params", std::move(params));
  return item->getObject("params");
}

LoggingCanvas::LoggingCanvas(int width, int height)
    : InterceptingCanvasBase(width, height), m_log(JSONArray::create()) {}

void LoggingCanvas::onDrawPaint(const SkPaint& paint) {
  AutoLogger logger(this);
  logger.logItemWithParams("drawPaint")
      ->setObject("paint", objectForSkPaint(paint));
  this->SkCanvas::onDrawPaint(paint);
}

void LoggingCanvas::onDrawPoints(PointMode mode,
                                 size_t count,
                                 const SkPoint pts[],
                                 const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawPoints");
  params->setString("pointMode", pointModeName(mode));
  params->setArray("points", arrayForSkPoints(count, pts));
  params->setObject("paint", objectForSkPaint(paint));
  this->SkCanvas::onDrawPoints(mode, count, pts, paint);
}

void LoggingCanvas::onDrawRect(const SkRect& rect, const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawRect");
  params->setObject("rect", objectForSkRect(rect));
  params->setObject("paint", objectForSkPaint(paint));
  this->SkCanvas::onDrawRect(rect, paint);
}

void LoggingCanvas::onDrawOval(const SkRect& oval, const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawOval");
  params->setObject("oval", objectForSkRect(oval));
  params->setObject("paint", objectForSkPaint(paint));
  this->SkCanvas::onDrawOval(oval, paint);
}

void LoggingCanvas::onDrawRRect(const SkRRect& rrect, const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawRRect");
  params->setObject("rrect", objectForSkRRect(rrect));
  params->setObject("paint", objectForSkPaint(paint));
  this->SkCanvas::onDrawRRect(rrect, paint);
}

void LoggingCanvas::onDrawPath(const SkPath& path, const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawPath");
  params->setObject("path", objectForSkPath(path));
  params->setObject("paint", objectForSkPaint(paint));
  this->SkCanvas::onDrawPath(path, paint);
}

void LoggingCanvas::onDrawBitmap(const SkBitmap& bitmap,
                                 SkScalar left,
                                 SkScalar top,
                                 const SkPaint* paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawBitmap");
  params->setDouble("left", left);
  params->setDouble("top", top);
  params->setObject("bitmap", objectForSkBitmap(bitmap));
  if (paint)
    params->setObject("paint", objectForSkPaint(*paint));
  this->SkCanvas::onDrawBitmap(bitmap, left, top, paint);
}

void LoggingCanvas::onDrawBitmapRect(const SkBitmap& bitmap,
                                     const SkRect* src,
                                     const SkRect& dst,
                                     const SkPaint* paint,
                                     SrcRectConstraint constraint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawBitmapRectToRect");
  params->setObject("bitmap", objectForSkBitmap(bitmap));
  if (src)
    params->setObject("src", objectForSkRect(*src));
  params->setObject("dst", objectForSkRect(dst));
  if (paint)
    params->setObject("paint", objectForSkPaint(*paint));
  params->setInteger("flags", constraint);
  this->SkCanvas::onDrawBitmapRect(bitmap, src, dst, paint, constraint);
}

void LoggingCanvas::onDrawBitmapNine(const SkBitmap& bitmap,
                                     const SkIRect& center,
                                     const SkRect& dst,
                                     const SkPaint* paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawBitmapNine");
  params->setObject("bitmap", objectForSkBitmap(bitmap));
  params->setObject("center", objectForSkIRect(center));
  params->setObject("dst", objectForSkRect(dst));
  if (paint)
    params->setObject("paint", objectForSkPaint(*paint));
  this->SkCanvas::onDrawBitmapNine(bitmap, center, dst, paint);
}

void LoggingCanvas::onDrawImage(const SkImage* image,
                                SkScalar left,
                                SkScalar top,
                                const SkPaint* paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawImage");
  params->setDouble("left", left);
  params->setDouble("top", top);
  params->setObject("image", objectForSkImage(image));
  if (paint)
    params->setObject("paint", objectForSkPaint(*paint));
  this->SkCanvas::onDrawImage(image, left, top, paint);
}

void LoggingCanvas::onDrawImageRect(const SkImage* image,
                                    const SkRect* src,
                                    const SkRect& dst,
                                    const SkPaint* paint,
                                    SrcRectConstraint constraint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawImageRect");
  params->setObject("image", objectForSkImage(image));
  if (src)
    params->setObject("src", objectForSkRect(*src));
  params->setObject("dst", objectForSkRect(dst));
  if (paint)
    params->setObject("paint", objectForSkPaint(*paint));
  this->SkCanvas::onDrawImageRect(image, src, dst, paint, constraint);
}

void LoggingCanvas::onDrawVertices(VertexMode vmode,
                                   int vertexCount,
                                   const SkPoint vertices[],
                                   const SkPoint texs[],
                                   const SkColor colors[],
                                   SkBlendMode bmode,
                                   const uint16_t indices[],
                                   int indexCount,
                                   const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawVertices");
  params->setObject("paint", objectForSkPaint(paint));
  this->SkCanvas::onDrawVertices(vmode, vertexCount, vertices, texs, colors,
                                 bmode, indices, indexCount, paint);
}

void LoggingCanvas::onDrawDRRect(const SkRRect& outer,
                                 const SkRRect& inner,
                                 const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawDRRect");
  params->setObject("outer", objectForSkRRect(outer));
  params->setObject("inner", objectForSkRRect(inner));
  params->setObject("paint", objectForSkPaint(paint));
  this->SkCanvas::onDrawDRRect(outer, inner, paint);
}

void LoggingCanvas::onDrawText(const void* text,
                               size_t byteLength,
                               SkScalar x,
                               SkScalar y,
                               const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawText");
  params->setString("text", stringForText(text, byteLength, paint));
  params->setDouble("x", x);
  params->setDouble("y", y);
  params->setObject("paint", objectForSkPaint(paint));
  this->SkCanvas::onDrawText(text, byteLength, x, y, paint);
}

void LoggingCanvas::onDrawPosText(const void* text,
                                  size_t byteLength,
                                  const SkPoint pos[],
                                  const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawPosText");
  params->setString("text", stringForText(text, byteLength, paint));
  size_t pointsCount = paint.countText(text, byteLength);
  params->setArray("pos", arrayForSkPoints(pointsCount, pos));
  params->setObject("paint", objectForSkPaint(paint));
  this->SkCanvas::onDrawPosText(text, byteLength, pos, paint);
}

void LoggingCanvas::onDrawPosTextH(const void* text,
                                   size_t byteLength,
                                   const SkScalar xpos[],
                                   SkScalar constY,
                                   const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawPosTextH");
  params->setString("text", stringForText(text, byteLength, paint));
  size_t pointsCount = paint.countText(text, byteLength);
  params->setArray("xpos", arrayForSkScalars(pointsCount, xpos));
  params->setDouble("constY", constY);
  params->setObject("paint", objectForSkPaint(paint));
  this->SkCanvas::onDrawPosTextH(text, byteLength, xpos, constY, paint);
}

void LoggingCanvas::onDrawTextOnPath(const void* text,
                                     size_t byteLength,
                                     const SkPath& path,
                                     const SkMatrix* matrix,
                                     const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawTextOnPath");
  params->setString("text", stringForText(text, byteLength, paint));
  params->setObject("path", objectForSkPath(path));
  if (matrix)
    params->setArray("matrix", arrayForSkMatrix(*matrix));
  params->setObject("paint", objectForSkPaint(paint));
  this->SkCanvas::onDrawTextOnPath(text, byteLength, path, matrix, paint);
}

void LoggingCanvas::onDrawTextBlob(const SkTextBlob* blob,
                                   SkScalar x,
                                   SkScalar y,
                                   const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("drawTextBlob");
  params->setDouble("x", x);
  params->setDouble("y", y);
  params->setObject("paint", objectForSkPaint(paint));
  this->SkCanvas::onDrawTextBlob(blob, x, y, paint);
}

void LoggingCanvas::onClipRect(const SkRect& rect,
                               SkRegion::Op op,
                               ClipEdgeStyle style) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("clipRect");
  params->setObject("rect", objectForSkRect(rect));
  params->setString("SkRegion::Op", regionOpName(op));
  params->setBoolean("softClipEdgeStyle", kSoft_ClipEdgeStyle == style);
  this->SkCanvas::onClipRect(rect, op, style);
}

void LoggingCanvas::onClipRRect(const SkRRect& rrect,
                                SkRegion::Op op,
                                ClipEdgeStyle style) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("clipRRect");
  params->setObject("rrect", objectForSkRRect(rrect));
  params->setString("SkRegion::Op", regionOpName(op));
  params->setBoolean("softClipEdgeStyle", kSoft_ClipEdgeStyle == style);
  this->SkCanvas::onClipRRect(rrect, op, style);
}

void LoggingCanvas::onClipPath(const SkPath& path,
                               SkRegion::Op op,
                               ClipEdgeStyle style) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("clipPath");
  params->setObject("path", objectForSkPath(path));
  params->setString("SkRegion::Op", regionOpName(op));
  params->setBoolean("softClipEdgeStyle", kSoft_ClipEdgeStyle == style);
  this->SkCanvas::onClipPath(path, op, style);
}

void LoggingCanvas::onClipRegion(const SkRegion& region, SkRegion::Op op) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("clipRegion");
  params->setString("op", regionOpName(op));
  this->SkCanvas::onClipRegion(region, op);
}

void LoggingCanvas::onDrawPicture(const SkPicture* picture,
                                  const SkMatrix* matrix,
                                  const SkPaint* paint) {
  this->unrollDrawPicture(picture, matrix, paint, nullptr);
}

void LoggingCanvas::didSetMatrix(const SkMatrix& matrix) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("setMatrix");
  params->setArray("matrix", arrayForSkMatrix(matrix));
  this->SkCanvas::didSetMatrix(matrix);
}

void LoggingCanvas::didConcat(const SkMatrix& matrix) {
  AutoLogger logger(this);
  JSONObject* params;

  switch (matrix.getType()) {
    case SkMatrix::kTranslate_Mask:
      params = logger.logItemWithParams("translate");
      params->setDouble("dx", matrix.getTranslateX());
      params->setDouble("dy", matrix.getTranslateY());
      break;

    case SkMatrix::kScale_Mask:
      params = logger.logItemWithParams("scale");
      params->setDouble("scaleX", matrix.getScaleX());
      params->setDouble("scaleY", matrix.getScaleY());
      break;

    default:
      params = logger.logItemWithParams("concat");
      params->setArray("matrix", arrayForSkMatrix(matrix));
  }
  this->SkCanvas::didConcat(matrix);
}

void LoggingCanvas::willSave() {
  AutoLogger logger(this);
  logger.logItem("save");
  this->SkCanvas::willSave();
}

SkCanvas::SaveLayerStrategy LoggingCanvas::getSaveLayerStrategy(
    const SaveLayerRec& rec) {
  AutoLogger logger(this);
  JSONObject* params = logger.logItemWithParams("saveLayer");
  if (rec.fBounds)
    params->setObject("bounds", objectForSkRect(*rec.fBounds));
  if (rec.fPaint)
    params->setObject("paint", objectForSkPaint(*rec.fPaint));
  params->setString("saveFlags", saveLayerFlagsToString(rec.fSaveLayerFlags));
  return this->SkCanvas::getSaveLayerStrategy(rec);
}

void LoggingCanvas::willRestore() {
  AutoLogger logger(this);
  logger.logItem("restore");
  this->SkCanvas::willRestore();
}

std::unique_ptr<JSONArray> LoggingCanvas::log() {
  return JSONArray::from(m_log->clone());
}

#ifndef NDEBUG
String pictureAsDebugString(const SkPicture* picture) {
  const SkIRect bounds = picture->cullRect().roundOut();
  LoggingCanvas canvas(bounds.width(), bounds.height());
  picture->playback(&canvas);
  std::unique_ptr<JSONObject> pictureAsJSON = JSONObject::create();
  pictureAsJSON->setObject("cullRect", objectForSkRect(picture->cullRect()));
  pictureAsJSON->setArray("operations", canvas.log());
  return pictureAsJSON->toPrettyJSONString();
}

void showSkPicture(const SkPicture* picture) {
  WTFLogAlways("%s\n", pictureAsDebugString(picture).utf8().data());
}
#endif

}  // namespace blink
