// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPaintImageGenerator_h
#define CSSPaintImageGenerator_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Image;
class LayoutObject;

// Produces a PaintGeneratedImage from a CSS Paint API callback.
// https://drafts.css-houdini.org/css-paint-api/
class CORE_EXPORT CSSPaintImageGenerator : public GarbageCollectedFinalized<CSSPaintImageGenerator> {
public:
    // This observer is used if the paint worklet doesn't have a javascript
    // class registered with the correct name yet.
    // paintImageGeneratorReady is called when the javascript class is
    // registered and ready to use.
    class Observer : public GarbageCollectedFinalized<Observer> {
    public:
        virtual ~Observer() { };
        virtual void paintImageGeneratorReady() = 0;
        DEFINE_INLINE_VIRTUAL_TRACE() { }
    };

    static CSSPaintImageGenerator* create(const String& name, Document&, Observer*);
    virtual ~CSSPaintImageGenerator();

    typedef CSSPaintImageGenerator* (*CSSPaintImageGeneratorCreateFunction)(const String&, Document&, Observer*);
    static void init(CSSPaintImageGeneratorCreateFunction);

    // Invokes the CSS Paint API 'paint' callback. May return a nullptr
    // representing an invalid image if an error occurred.
    virtual PassRefPtr<Image> paint(const LayoutObject&, const IntSize&) = 0;

    virtual const Vector<CSSPropertyID>& nativeInvalidationProperties() const = 0;
    virtual const Vector<AtomicString>& customInvalidationProperties() const = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() { }
};

} // namespace blink

#endif // CSSPaintImageGenerator_h
