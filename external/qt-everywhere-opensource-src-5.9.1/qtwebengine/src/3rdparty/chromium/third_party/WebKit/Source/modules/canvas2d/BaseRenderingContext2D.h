// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BaseRenderingContext2D_h
#define BaseRenderingContext2D_h

#include "bindings/modules/v8/CSSImageValueOrHTMLImageElementOrHTMLVideoElementOrHTMLCanvasElementOrImageBitmapOrOffscreenCanvas.h"
#include "bindings/modules/v8/StringOrCanvasGradientOrCanvasPattern.h"
#include "core/html/ImageData.h"
#include "modules/ModulesExport.h"
#include "modules/canvas2d/CanvasGradient.h"
#include "modules/canvas2d/CanvasPathMethods.h"
#include "modules/canvas2d/CanvasRenderingContext2DState.h"
#include "modules/canvas2d/CanvasStyle.h"
#include "platform/graphics/ExpensiveCanvasHeuristicParameters.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/effects/SkComposeImageFilter.h"

namespace blink {

class CanvasImageSource;
class Color;
class Image;
class ImageBuffer;
class Path2D;
class SVGMatrixTearOff;

typedef CSSImageValueOrHTMLImageElementOrHTMLVideoElementOrHTMLCanvasElementOrImageBitmapOrOffscreenCanvas
    CanvasImageSourceUnion;

class MODULES_EXPORT BaseRenderingContext2D : public GarbageCollectedMixin,
                                              public CanvasPathMethods {
  WTF_MAKE_NONCOPYABLE(BaseRenderingContext2D);

 public:
  ~BaseRenderingContext2D() override;

  void reset();

  void strokeStyle(StringOrCanvasGradientOrCanvasPattern&) const;
  void setStrokeStyle(const StringOrCanvasGradientOrCanvasPattern&);

  void fillStyle(StringOrCanvasGradientOrCanvasPattern&) const;
  void setFillStyle(const StringOrCanvasGradientOrCanvasPattern&);

  double lineWidth() const;
  void setLineWidth(double);

  String lineCap() const;
  void setLineCap(const String&);

  String lineJoin() const;
  void setLineJoin(const String&);

  double miterLimit() const;
  void setMiterLimit(double);

  const Vector<double>& getLineDash() const;
  void setLineDash(const Vector<double>&);

  double lineDashOffset() const;
  void setLineDashOffset(double);

  double shadowOffsetX() const;
  void setShadowOffsetX(double);

  double shadowOffsetY() const;
  void setShadowOffsetY(double);

  double shadowBlur() const;
  void setShadowBlur(double);

  String shadowColor() const;
  void setShadowColor(const String&);

  double globalAlpha() const;
  void setGlobalAlpha(double);

  String globalCompositeOperation() const;
  void setGlobalCompositeOperation(const String&);

  String filter() const;
  void setFilter(const String&);

  void save();
  void restore();

  SVGMatrixTearOff* currentTransform() const;
  void setCurrentTransform(SVGMatrixTearOff*);

  void scale(double sx, double sy);
  void rotate(double angleInRadians);
  void translate(double tx, double ty);
  void transform(double m11,
                 double m12,
                 double m21,
                 double m22,
                 double dx,
                 double dy);
  void setTransform(double m11,
                    double m12,
                    double m21,
                    double m22,
                    double dx,
                    double dy);
  void resetTransform();

  void beginPath();

  void fill(const String& winding = "nonzero");
  void fill(Path2D*, const String& winding = "nonzero");
  void stroke();
  void stroke(Path2D*);
  void clip(const String& winding = "nonzero");
  void clip(Path2D*, const String& winding = "nonzero");

  bool isPointInPath(const double x,
                     const double y,
                     const String& winding = "nonzero");
  bool isPointInPath(Path2D*,
                     const double x,
                     const double y,
                     const String& winding = "nonzero");
  bool isPointInStroke(const double x, const double y);
  bool isPointInStroke(Path2D*, const double x, const double y);

  void clearRect(double x, double y, double width, double height);
  void fillRect(double x, double y, double width, double height);
  void strokeRect(double x, double y, double width, double height);

  void drawImage(ExecutionContext*,
                 const CanvasImageSourceUnion&,
                 double x,
                 double y,
                 ExceptionState&);
  void drawImage(ExecutionContext*,
                 const CanvasImageSourceUnion&,
                 double x,
                 double y,
                 double width,
                 double height,
                 ExceptionState&);
  void drawImage(ExecutionContext*,
                 const CanvasImageSourceUnion&,
                 double sx,
                 double sy,
                 double sw,
                 double sh,
                 double dx,
                 double dy,
                 double dw,
                 double dh,
                 ExceptionState&);
  void drawImage(ExecutionContext*,
                 CanvasImageSource*,
                 double sx,
                 double sy,
                 double sw,
                 double sh,
                 double dx,
                 double dy,
                 double dw,
                 double dh,
                 ExceptionState&);

  CanvasGradient* createLinearGradient(double x0,
                                       double y0,
                                       double x1,
                                       double y1);
  CanvasGradient* createRadialGradient(double x0,
                                       double y0,
                                       double r0,
                                       double x1,
                                       double y1,
                                       double r1,
                                       ExceptionState&);
  CanvasPattern* createPattern(ExecutionContext*,
                               const CanvasImageSourceUnion&,
                               const String& repetitionType,
                               ExceptionState&);
  CanvasPattern* createPattern(ExecutionContext*,
                               CanvasImageSource*,
                               const String& repetitionType,
                               ExceptionState&);

  ImageData* createImageData(ImageData*, ExceptionState&) const;
  ImageData* createImageData(double width,
                             double height,
                             ExceptionState&) const;
  ImageData* getImageData(double sx,
                          double sy,
                          double sw,
                          double sh,
                          ExceptionState&) const;
  void putImageData(ImageData*, double dx, double dy, ExceptionState&);
  void putImageData(ImageData*,
                    double dx,
                    double dy,
                    double dirtyX,
                    double dirtyY,
                    double dirtyWidth,
                    double dirtyHeight,
                    ExceptionState&);

  bool imageSmoothingEnabled() const;
  void setImageSmoothingEnabled(bool);
  String imageSmoothingQuality() const;
  void setImageSmoothingQuality(const String&);

  virtual bool originClean() const = 0;
  virtual void setOriginTainted() = 0;
  virtual bool wouldTaintOrigin(CanvasImageSource*, ExecutionContext*) = 0;

  virtual int width() const = 0;
  virtual int height() const = 0;

  virtual bool hasImageBuffer() const = 0;
  virtual ImageBuffer* imageBuffer() const = 0;

  virtual bool parseColorOrCurrentColor(Color&,
                                        const String& colorString) const = 0;

  virtual SkCanvas* drawingCanvas() const = 0;
  virtual SkCanvas* existingDrawingCanvas() const = 0;
  virtual void disableDeferral(DisableDeferralReason) = 0;

  virtual AffineTransform baseTransform() const = 0;

  virtual void didDraw(const SkIRect& dirtyRect) = 0;

  virtual bool stateHasFilter() = 0;
  virtual sk_sp<SkImageFilter> stateGetFilter() = 0;
  virtual void snapshotStateForFilter() = 0;

  virtual void validateStateStack() const = 0;

  virtual bool hasAlpha() const = 0;

  virtual bool isContextLost() const = 0;

  void restoreMatrixClipStack(SkCanvas*) const;

  DECLARE_VIRTUAL_TRACE();

  enum DrawCallType {
    StrokePath = 0,
    FillPath,
    DrawVectorImage,
    DrawBitmapImage,
    FillText,
    StrokeText,
    FillRect,
    StrokeRect,
    DrawCallTypeCount  // used to specify the size of storage arrays
  };

  enum PathFillType {
    ColorFillType,
    LinearGradientFillType,
    RadialGradientFillType,
    PatternFillType,
    PathFillTypeCount  // used to specify the size of storage arrays
  };

  struct UsageCounters {
    int numDrawCalls[DrawCallTypeCount];  // use DrawCallType enum as index
    float boundingBoxPerimeterDrawCalls[DrawCallTypeCount];
    float boundingBoxAreaDrawCalls[DrawCallTypeCount];
    float boundingBoxAreaFillType[PathFillTypeCount];
    int numNonConvexFillPathCalls;
    float nonConvexFillPathArea;
    int numRadialGradients;
    int numLinearGradients;
    int numPatterns;
    int numDrawWithComplexClips;
    int numBlurredShadows;
    float boundingBoxAreaTimesShadowBlurSquared;
    float boundingBoxPerimeterTimesShadowBlurSquared;
    int numFilters;
    int numGetImageDataCalls;
    float areaGetImageDataCalls;
    int numPutImageDataCalls;
    float areaPutImageDataCalls;
    int numClearRectCalls;
    int numDrawFocusCalls;
    int numFramesSinceReset;

    UsageCounters();
  };

  const UsageCounters& getUsage();

 protected:
  BaseRenderingContext2D();

  CanvasRenderingContext2DState& modifiableState();
  const CanvasRenderingContext2DState& state() const {
    return *m_stateStack.last();
  }

  bool computeDirtyRect(const FloatRect& localBounds, SkIRect*);
  bool computeDirtyRect(const FloatRect& localBounds,
                        const SkIRect& transformedClipBounds,
                        SkIRect*);

  template <typename DrawFunc, typename ContainsFunc>
  bool draw(const DrawFunc&,
            const ContainsFunc&,
            const SkRect& bounds,
            CanvasRenderingContext2DState::PaintType,
            CanvasRenderingContext2DState::ImageType =
                CanvasRenderingContext2DState::NoImage);

  void inflateStrokeRect(FloatRect&) const;

  void unwindStateStack();

  enum DrawType {
    ClipFill,  // Fill that is already known to cover the current clip
    UntransformedUnclippedFill
  };

  void checkOverdraw(const SkRect&,
                     const SkPaint*,
                     CanvasRenderingContext2DState::ImageType,
                     DrawType);

  HeapVector<Member<CanvasRenderingContext2DState>> m_stateStack;
  AntiAliasingMode m_clipAntialiasing;

  void trackDrawCall(DrawCallType,
                     Path2D* path2d = nullptr,
                     int width = 0,
                     int height = 0);

  mutable UsageCounters m_usageCounters;

  float estimateRenderingCost(
      ExpensiveCanvasHeuristicParameters::RenderingModeCostIndex) const;

 private:
  void realizeSaves();

  bool shouldDrawImageAntialiased(const FloatRect& destRect) const;

  void drawPathInternal(const Path&,
                        CanvasRenderingContext2DState::PaintType,
                        SkPath::FillType = SkPath::kWinding_FillType);
  void drawImageInternal(SkCanvas*,
                         CanvasImageSource*,
                         Image*,
                         const FloatRect& srcRect,
                         const FloatRect& dstRect,
                         const SkPaint*);
  void clipInternal(const Path&, const String& windingRuleString);

  bool isPointInPathInternal(const Path&,
                             const double x,
                             const double y,
                             const String& windingRuleString);
  bool isPointInStrokeInternal(const Path&, const double x, const double y);

  static bool isFullCanvasCompositeMode(SkBlendMode);

  template <typename DrawFunc>
  void compositedDraw(const DrawFunc&,
                      SkCanvas*,
                      CanvasRenderingContext2DState::PaintType,
                      CanvasRenderingContext2DState::ImageType);

  void clearCanvas();
  bool rectContainsTransformedRect(const FloatRect&, const SkIRect&) const;
};

template <typename DrawFunc, typename ContainsFunc>
bool BaseRenderingContext2D::draw(
    const DrawFunc& drawFunc,
    const ContainsFunc& drawCoversClipBounds,
    const SkRect& bounds,
    CanvasRenderingContext2DState::PaintType paintType,
    CanvasRenderingContext2DState::ImageType imageType) {
  if (!state().isTransformInvertible())
    return false;

  SkIRect clipBounds;
  if (!drawingCanvas() || !drawingCanvas()->getClipDeviceBounds(&clipBounds))
    return false;

  // If gradient size is zero, then paint nothing.
  CanvasStyle* style = state().style(paintType);
  if (style) {
    CanvasGradient* gradient = style->getCanvasGradient();
    if (gradient && gradient->getGradient()->isZeroSize())
      return false;
  }

  if (isFullCanvasCompositeMode(state().globalComposite()) ||
      stateHasFilter()) {
    compositedDraw(drawFunc, drawingCanvas(), paintType, imageType);
    didDraw(clipBounds);
  } else if (state().globalComposite() == SkBlendMode::kSrc) {
    clearCanvas();  // takes care of checkOverdraw()
    const SkPaint* paint =
        state().getPaint(paintType, DrawForegroundOnly, imageType);
    drawFunc(drawingCanvas(), paint);
    didDraw(clipBounds);
  } else {
    SkIRect dirtyRect;
    if (computeDirtyRect(bounds, clipBounds, &dirtyRect)) {
      const SkPaint* paint =
          state().getPaint(paintType, DrawShadowAndForeground, imageType);
      if (paintType != CanvasRenderingContext2DState::StrokePaintType &&
          drawCoversClipBounds(clipBounds))
        checkOverdraw(bounds, paint, imageType, ClipFill);
      drawFunc(drawingCanvas(), paint);
      didDraw(dirtyRect);
    }
  }
  return true;
}

template <typename DrawFunc>
void BaseRenderingContext2D::compositedDraw(
    const DrawFunc& drawFunc,
    SkCanvas* c,
    CanvasRenderingContext2DState::PaintType paintType,
    CanvasRenderingContext2DState::ImageType imageType) {
  sk_sp<SkImageFilter> filter = stateGetFilter();
  ASSERT(isFullCanvasCompositeMode(state().globalComposite()) || filter);
  SkMatrix ctm = c->getTotalMatrix();
  c->resetMatrix();
  SkPaint compositePaint;
  compositePaint.setBlendMode((SkBlendMode)state().globalComposite());
  if (state().shouldDrawShadows()) {
    // unroll into two independently composited passes if drawing shadows
    SkPaint shadowPaint =
        *state().getPaint(paintType, DrawShadowOnly, imageType);
    int saveCount = c->getSaveCount();
    if (filter) {
      SkPaint foregroundPaint =
          *state().getPaint(paintType, DrawForegroundOnly, imageType);
      sk_sp<SkImageFilter> composedFilter =
          sk_ref_sp(foregroundPaint.getImageFilter());
      composedFilter = SkComposeImageFilter::Make(
          std::move(composedFilter), sk_ref_sp(shadowPaint.getImageFilter()));
      composedFilter =
          SkComposeImageFilter::Make(std::move(composedFilter), filter);
      foregroundPaint.setImageFilter(std::move(composedFilter));
      c->setMatrix(ctm);
      drawFunc(c, &foregroundPaint);
    } else {
      ASSERT(isFullCanvasCompositeMode(state().globalComposite()));
      c->saveLayer(nullptr, &compositePaint);
      shadowPaint.setBlendMode(SkBlendMode::kSrcOver);
      c->setMatrix(ctm);
      drawFunc(c, &shadowPaint);
    }
    c->restoreToCount(saveCount);
  }

  compositePaint.setImageFilter(std::move(filter));
  c->saveLayer(nullptr, &compositePaint);
  SkPaint foregroundPaint =
      *state().getPaint(paintType, DrawForegroundOnly, imageType);
  foregroundPaint.setBlendMode(SkBlendMode::kSrcOver);
  c->setMatrix(ctm);
  drawFunc(c, &foregroundPaint);
  c->restore();
  c->setMatrix(ctm);
}

}  // namespace blink

#endif  // BaseRenderingContext2D_h
