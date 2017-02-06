// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaValuesDynamic_h
#define MediaValuesDynamic_h

#include "core/css/MediaValues.h"

namespace blink {

class Document;

class MediaValuesDynamic final : public MediaValues {
public:
    static MediaValues* create(Document&);
    static MediaValues* create(LocalFrame*);
    MediaValues* copy() const override;
    bool computeLength(double value, CSSPrimitiveValue::UnitType, int& result) const override;
    bool computeLength(double value, CSSPrimitiveValue::UnitType, double& result) const override;

    double viewportWidth() const override;
    double viewportHeight() const override;
    int deviceWidth() const override;
    int deviceHeight() const override;
    float devicePixelRatio() const override;
    int colorBitsPerComponent() const override;
    int monochromeBitsPerComponent() const override;
    PointerType primaryPointerType() const override;
    int availablePointerTypes() const override;
    HoverType primaryHoverType() const override;
    int availableHoverTypes() const override;
    bool threeDEnabled() const override;
    bool strictMode() const override;
    const String mediaType() const override;
    WebDisplayMode displayMode() const override;
    Document* document() const override;
    bool hasValues() const override;
    void overrideViewportDimensions(double width, double height) override;

    DECLARE_VIRTUAL_TRACE();

protected:
    MediaValuesDynamic(LocalFrame*);
    MediaValuesDynamic(LocalFrame*, bool overriddenViewportDimensions, double viewportWidth, double viewportHeight);

    // This raw ptr is safe, as MediaValues would not outlive MediaQueryEvaluator, and
    // MediaQueryEvaluator is reset on |Document::detach|.
    Member<LocalFrame> m_frame;
    bool m_viewportDimensionsOverridden;
    double m_viewportWidthOverride;
    double m_viewportHeightOverride;
};

} // namespace blink

#endif // MediaValuesDynamic_h
