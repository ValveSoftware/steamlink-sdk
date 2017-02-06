// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPaintImageGeneratorImpl_h
#define CSSPaintImageGeneratorImpl_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "core/css/CSSPaintImageGenerator.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include <v8.h>

namespace blink {

class CSSPaintDefinition;
class Document;
class Image;

class CSSPaintImageGeneratorImpl final : public CSSPaintImageGenerator {
public:
    static CSSPaintImageGenerator* create(const String& name, Document&, Observer*);
    ~CSSPaintImageGeneratorImpl() override;

    PassRefPtr<Image> paint(const LayoutObject&, const IntSize&) final;
    const Vector<CSSPropertyID>& nativeInvalidationProperties() const final;
    const Vector<AtomicString>& customInvalidationProperties() const final;

    // Should be called from the PaintWorkletGlobalScope when a javascript class
    // is registered with the same name.
    void setDefinition(CSSPaintDefinition*);

    DECLARE_VIRTUAL_TRACE();

private:
    CSSPaintImageGeneratorImpl(Observer*);
    CSSPaintImageGeneratorImpl(CSSPaintDefinition*);

    Member<CSSPaintDefinition> m_definition;
    Member<Observer> m_observer;
};

} // namespace blink

#endif // CSSPaintImageGeneratorImpl_h
