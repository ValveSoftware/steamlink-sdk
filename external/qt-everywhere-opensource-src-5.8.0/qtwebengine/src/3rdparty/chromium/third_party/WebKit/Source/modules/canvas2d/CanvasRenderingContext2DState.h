// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasRenderingContext2DState_h
#define CanvasRenderingContext2DState_h

#include "core/css/CSSFontSelectorClient.h"
#include "modules/canvas2d/ClipList.h"
#include "platform/fonts/Font.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/Vector.h"

namespace blink {

class CanvasRenderingContext2D;
class CanvasStyle;
class CSSValue;
class Element;

class CanvasRenderingContext2DState final : public GarbageCollectedFinalized<CanvasRenderingContext2DState>, public CSSFontSelectorClient {
    WTF_MAKE_NONCOPYABLE(CanvasRenderingContext2DState);
    USING_GARBAGE_COLLECTED_MIXIN(CanvasRenderingContext2DState);
public:
    static CanvasRenderingContext2DState* create()
    {
        return new CanvasRenderingContext2DState;
    }

    ~CanvasRenderingContext2DState() override;

    DECLARE_VIRTUAL_TRACE();

    enum ClipListCopyMode {
        CopyClipList,
        DontCopyClipList
    };

    enum PaintType {
        FillPaintType,
        StrokePaintType,
        ImagePaintType,
    };

    static CanvasRenderingContext2DState* create(const CanvasRenderingContext2DState& other, ClipListCopyMode mode)
    {
        return new CanvasRenderingContext2DState(other, mode);
    }

    // CSSFontSelectorClient implementation
    void fontsNeedUpdate(CSSFontSelector*) override;

    bool hasUnrealizedSaves() const { return m_unrealizedSaveCount; }
    void save() { ++m_unrealizedSaveCount; }
    void restore() { --m_unrealizedSaveCount; }
    void resetUnrealizedSaveCount() { m_unrealizedSaveCount = 0; }

    void setLineDash(const Vector<double>&);
    const Vector<double>& lineDash() const { return m_lineDash; }

    void setShouldAntialias(bool);
    bool shouldAntialias() const;

    void setLineDashOffset(double);
    double lineDashOffset() const { return m_lineDashOffset; }

    // setTransform returns true iff transform is invertible;
    void setTransform(const AffineTransform&);
    void resetTransform();
    AffineTransform transform() const { return m_transform; }
    bool isTransformInvertible() const { return m_isTransformInvertible; }

    void clipPath(const SkPath&, AntiAliasingMode);
    bool hasClip() const { return m_hasClip; }
    bool hasComplexClip() const { return m_hasComplexClip; }
    void playbackClips(SkCanvas* canvas) const { m_clipList.playback(canvas); }
    const SkPath& getCurrentClipPath() const { return m_clipList.getCurrentClipPath(); }

    void setFont(const Font&, CSSFontSelector*);
    const Font& font() const;
    bool hasRealizedFont() const { return m_realizedFont; }
    void setUnparsedFont(const String& font) { m_unparsedFont = font; }
    const String& unparsedFont() const { return m_unparsedFont; }

    void setFontForFilter(const Font& font) { m_fontForFilter = font; }

    void setFilter(const CSSValue*);
    void setUnparsedFilter(const String& filterString) { m_unparsedFilter = filterString; }
    const String& unparsedFilter() const { return m_unparsedFilter; }
    SkImageFilter* getFilter(Element*, IntSize canvasSize, CanvasRenderingContext2D*) const;
    bool hasFilter(Element*, IntSize canvasSize, CanvasRenderingContext2D*) const;
    void clearResolvedFilter() const;

    void setStrokeStyle(CanvasStyle*);
    CanvasStyle* strokeStyle() const { return m_strokeStyle.get(); }

    void setFillStyle(CanvasStyle*);
    CanvasStyle* fillStyle() const { return m_fillStyle.get(); }

    CanvasStyle* style(PaintType) const;

    enum Direction {
        DirectionInherit,
        DirectionRTL,
        DirectionLTR
    };

    void setDirection(Direction direction) { m_direction = direction; }
    Direction getDirection() const { return m_direction; }

    void setTextAlign(TextAlign align) { m_textAlign = align; }
    TextAlign getTextAlign() const { return m_textAlign; }

    void setTextBaseline(TextBaseline baseline) { m_textBaseline = baseline; }
    TextBaseline getTextBaseline() const { return m_textBaseline; }

    void setLineWidth(double lineWidth) { m_strokePaint.setStrokeWidth(lineWidth); }
    double lineWidth() const { return m_strokePaint.getStrokeWidth(); }

    void setLineCap(LineCap lineCap) { m_strokePaint.setStrokeCap(static_cast<SkPaint::Cap>(lineCap)); }
    LineCap getLineCap() const { return static_cast<LineCap>(m_strokePaint.getStrokeCap()); }

    void setLineJoin(LineJoin lineJoin) { m_strokePaint.setStrokeJoin(static_cast<SkPaint::Join>(lineJoin)); }
    LineJoin getLineJoin() const { return static_cast<LineJoin>(m_strokePaint.getStrokeJoin()); }

    void setMiterLimit(double miterLimit) { m_strokePaint.setStrokeMiter(miterLimit); }
    double miterLimit() const { return m_strokePaint.getStrokeMiter(); }

    void setShadowOffsetX(double);
    void setShadowOffsetY(double);
    const FloatSize& shadowOffset() const { return m_shadowOffset; }

    void setShadowBlur(double);
    double shadowBlur() const { return m_shadowBlur; }

    void setShadowColor(SkColor);
    SkColor shadowColor() const { return m_shadowColor; }

    void setGlobalAlpha(double);
    double globalAlpha() const { return m_globalAlpha; }

    void setGlobalComposite(SkXfermode::Mode);
    SkXfermode::Mode globalComposite() const;

    void setImageSmoothingEnabled(bool);
    bool imageSmoothingEnabled() const;
    void setImageSmoothingQuality(const String&);
    String imageSmoothingQuality() const;

    void setUnparsedStrokeColor(const String& color) { m_unparsedStrokeColor = color; }
    const String& unparsedStrokeColor() const { return m_unparsedStrokeColor; }

    void setUnparsedFillColor(const String& color) { m_unparsedFillColor = color; }
    const String& unparsedFillColor() const { return m_unparsedFillColor; }

    bool shouldDrawShadows() const;

    enum ImageType {
        NoImage,
        OpaqueImage,
        NonOpaqueImage
    };

    // If paint will not be used for painting a bitmap, set bitmapOpacity to Opaque
    const SkPaint* getPaint(PaintType, ShadowMode, ImageType = NoImage) const;

private:
    CanvasRenderingContext2DState();
    CanvasRenderingContext2DState(const CanvasRenderingContext2DState&, ClipListCopyMode);

    void updateLineDash() const;
    void updateStrokeStyle() const;
    void updateFillStyle() const;
    void updateFilterQuality() const;
    void updateFilterQualityWithSkFilterQuality(const SkFilterQuality&) const;
    void shadowParameterChanged();
    SkDrawLooper* emptyDrawLooper() const;
    SkDrawLooper* shadowOnlyDrawLooper() const;
    SkDrawLooper* shadowAndForegroundDrawLooper() const;
    SkImageFilter* shadowOnlyImageFilter() const;
    SkImageFilter* shadowAndForegroundImageFilter() const;

    unsigned m_unrealizedSaveCount;

    String m_unparsedStrokeColor;
    String m_unparsedFillColor;
    Member<CanvasStyle> m_strokeStyle;
    Member<CanvasStyle> m_fillStyle;

    mutable SkPaint m_strokePaint;
    mutable SkPaint m_fillPaint;
    mutable SkPaint m_imagePaint;

    FloatSize m_shadowOffset;
    double m_shadowBlur;
    SkColor m_shadowColor;
    mutable RefPtr<SkDrawLooper> m_emptyDrawLooper;
    mutable RefPtr<SkDrawLooper> m_shadowOnlyDrawLooper;
    mutable RefPtr<SkDrawLooper> m_shadowAndForegroundDrawLooper;
    mutable sk_sp<SkImageFilter> m_shadowOnlyImageFilter;
    mutable sk_sp<SkImageFilter> m_shadowAndForegroundImageFilter;

    double m_globalAlpha;
    AffineTransform m_transform;
    Vector<double> m_lineDash;
    double m_lineDashOffset;

    String m_unparsedFont;
    Font m_font;
    Font m_fontForFilter;

    String m_unparsedFilter;
    Member<const CSSValue> m_filterValue;
    mutable sk_sp<SkImageFilter> m_resolvedFilter;

    // Text state.
    TextAlign m_textAlign;
    TextBaseline m_textBaseline;
    Direction m_direction;

    bool m_realizedFont : 1;
    bool m_isTransformInvertible : 1;
    bool m_hasClip : 1;
    bool m_hasComplexClip : 1;
    mutable bool m_fillStyleDirty : 1;
    mutable bool m_strokeStyleDirty : 1;
    mutable bool m_lineDashDirty : 1;

    bool m_imageSmoothingEnabled;
    SkFilterQuality m_imageSmoothingQuality;

    ClipList m_clipList;
};

} // namespace blink

#endif
