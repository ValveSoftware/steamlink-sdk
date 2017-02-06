// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaValues_h
#define MediaValues_h

#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "platform/heap/Handle.h"
#include "public/platform/PointerProperties.h"
#include "public/platform/WebDisplayMode.h"

namespace blink {

class Document;
class CSSPrimitiveValue;
class LocalFrame;

class CORE_EXPORT MediaValues : public GarbageCollectedFinalized<MediaValues> {
public:

    virtual ~MediaValues() { }
    DEFINE_INLINE_VIRTUAL_TRACE() { }

    static MediaValues* createDynamicIfFrameExists(LocalFrame*);
    virtual MediaValues* copy() const = 0;

    static bool computeLengthImpl(double value, CSSPrimitiveValue::UnitType, unsigned defaultFontSize, double viewportWidth, double viewportHeight, double& result);
    template<typename T>
    static bool computeLength(double value, CSSPrimitiveValue::UnitType type, unsigned defaultFontSize, double viewportWidth, double viewportHeight, T& result)
    {
        double tempResult;
        if (!computeLengthImpl(value, type, defaultFontSize, viewportWidth, viewportHeight, tempResult))
            return false;
        result = clampTo<T>(tempResult);
        return true;
    }
    virtual bool computeLength(double value, CSSPrimitiveValue::UnitType, int& result) const = 0;
    virtual bool computeLength(double value, CSSPrimitiveValue::UnitType, double& result) const = 0;

    virtual double viewportWidth() const = 0;
    virtual double viewportHeight() const = 0;
    virtual int deviceWidth() const = 0;
    virtual int deviceHeight() const = 0;
    virtual float devicePixelRatio() const = 0;
    virtual int colorBitsPerComponent() const = 0;
    virtual int monochromeBitsPerComponent() const = 0;
    virtual PointerType primaryPointerType() const = 0;
    virtual int availablePointerTypes() const = 0;
    virtual HoverType primaryHoverType() const = 0;
    virtual int availableHoverTypes() const = 0;
    virtual bool threeDEnabled() const = 0;
    virtual const String mediaType() const = 0;
    virtual WebDisplayMode displayMode() const = 0;
    virtual bool strictMode() const = 0;
    virtual Document* document() const = 0;
    virtual bool hasValues() const = 0;

    virtual void overrideViewportDimensions(double width, double height) = 0;

protected:
    static double calculateViewportWidth(LocalFrame*);
    static double calculateViewportHeight(LocalFrame*);
    static int calculateDeviceWidth(LocalFrame*);
    static int calculateDeviceHeight(LocalFrame*);
    static bool calculateStrictMode(LocalFrame*);
    static float calculateDevicePixelRatio(LocalFrame*);
    static int calculateColorBitsPerComponent(LocalFrame*);
    static int calculateMonochromeBitsPerComponent(LocalFrame*);
    static int calculateDefaultFontSize(LocalFrame*);
    static const String calculateMediaType(LocalFrame*);
    static WebDisplayMode calculateDisplayMode(LocalFrame*);
    static bool calculateThreeDEnabled(LocalFrame*);
    static PointerType calculatePrimaryPointerType(LocalFrame*);
    static int calculateAvailablePointerTypes(LocalFrame*);
    static HoverType calculatePrimaryHoverType(LocalFrame*);
    static int calculateAvailableHoverTypes(LocalFrame*);
    static LocalFrame* frameFrom(Document&);

};

} // namespace blink

#endif // MediaValues_h
