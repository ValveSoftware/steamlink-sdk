// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/CSSPaintImageGeneratorImpl.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "modules/csspaint/CSSPaintDefinition.h"
#include "modules/csspaint/PaintWorklet.h"
#include "modules/csspaint/WindowPaintWorklet.h"
#include "platform/graphics/Image.h"

namespace blink {

CSSPaintImageGenerator* CSSPaintImageGeneratorImpl::create(const String& name, Document& document, Observer* observer)
{
    LocalDOMWindow* domWindow = document.domWindow();
    PaintWorklet* paintWorklet = WindowPaintWorklet::from(*domWindow).paintWorklet(&document);

    CSSPaintDefinition* paintDefinition = paintWorklet->findDefinition(name);
    CSSPaintImageGeneratorImpl* generator;
    if (!paintDefinition) {
        generator = new CSSPaintImageGeneratorImpl(observer);
        paintWorklet->addPendingGenerator(name, generator);
    } else {
        generator = new CSSPaintImageGeneratorImpl(paintDefinition);
    }

    return generator;
}

CSSPaintImageGeneratorImpl::CSSPaintImageGeneratorImpl(CSSPaintDefinition* definition)
    : m_definition(definition)
{
}

CSSPaintImageGeneratorImpl::CSSPaintImageGeneratorImpl(Observer* observer)
    : m_observer(observer)
{
}

CSSPaintImageGeneratorImpl::~CSSPaintImageGeneratorImpl()
{
}

void CSSPaintImageGeneratorImpl::setDefinition(CSSPaintDefinition* definition)
{
    ASSERT(!m_definition);
    m_definition = definition;

    ASSERT(m_observer);
    m_observer->paintImageGeneratorReady();
}

PassRefPtr<Image> CSSPaintImageGeneratorImpl::paint(const LayoutObject& layoutObject, const IntSize& size)
{
    return m_definition ? m_definition->paint(layoutObject, size) : nullptr;
}

const Vector<CSSPropertyID>& CSSPaintImageGeneratorImpl::nativeInvalidationProperties() const
{
    DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, emptyVector, ());
    return m_definition ? m_definition->nativeInvalidationProperties() : emptyVector;
}

const Vector<AtomicString>& CSSPaintImageGeneratorImpl::customInvalidationProperties() const
{
    DEFINE_STATIC_LOCAL(Vector<AtomicString>, emptyVector, ());
    return m_definition ? m_definition->customInvalidationProperties() : emptyVector;
}

DEFINE_TRACE(CSSPaintImageGeneratorImpl)
{
    visitor->trace(m_definition);
    visitor->trace(m_observer);
    CSSPaintImageGenerator::trace(visitor);
}

} // namespace blink
