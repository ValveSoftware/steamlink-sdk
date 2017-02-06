// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSPaintValue.h"

#include "core/css/CSSCustomIdentValue.h"
#include "core/layout/LayoutObject.h"
#include "platform/graphics/Image.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSPaintValue::CSSPaintValue(CSSCustomIdentValue* name)
    : CSSImageGeneratorValue(PaintClass)
    , m_name(name)
    , m_paintImageGeneratorObserver(new Observer(this))
{
}

CSSPaintValue::~CSSPaintValue()
{
}

String CSSPaintValue::customCSSText() const
{
    StringBuilder result;
    result.append("paint(");
    result.append(m_name->customCSSText());
    result.append(')');
    return result.toString();
}

String CSSPaintValue::name() const
{
    return m_name->value();
}

PassRefPtr<Image> CSSPaintValue::image(const LayoutObject& layoutObject, const IntSize& size)
{
    if (!m_generator)
        m_generator = CSSPaintImageGenerator::create(name(), layoutObject.document(), m_paintImageGeneratorObserver);

    return m_generator->paint(layoutObject, size);
}

void CSSPaintValue::Observer::paintImageGeneratorReady()
{
    m_ownerValue->paintImageGeneratorReady();
}

void CSSPaintValue::paintImageGeneratorReady()
{
    for (const LayoutObject* client : clients().keys()) {
        const_cast<LayoutObject*>(client)->imageChanged(static_cast<WrappedImagePtr>(this));
    }
}

bool CSSPaintValue::equals(const CSSPaintValue& other) const
{
    return name() == other.name();
}

DEFINE_TRACE_AFTER_DISPATCH(CSSPaintValue)
{
    visitor->trace(m_name);
    visitor->trace(m_generator);
    visitor->trace(m_paintImageGeneratorObserver);
    CSSImageGeneratorValue::traceAfterDispatch(visitor);
}

} // namespace blink
