// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/SkPictureBuilder.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "wtf/PtrUtil.h"

namespace blink {

SkPictureBuilder::SkPictureBuilder(const FloatRect& bounds, SkMetaData* metaData, GraphicsContext* containingContext)
    : m_bounds(bounds)
{
    GraphicsContext::DisabledMode disabledMode = GraphicsContext::NothingDisabled;
    if (containingContext && containingContext->contextDisabled())
        disabledMode = GraphicsContext::FullyDisabled;

    m_paintController = PaintController::create();
    m_paintController->beginSkippingCache();
    m_context = wrapUnique(new GraphicsContext(*m_paintController, disabledMode, metaData));

    if (containingContext) {
        m_context->setDeviceScaleFactor(containingContext->deviceScaleFactor());
        m_context->setPrinting(containingContext->printing());
    }
}

SkPictureBuilder::~SkPictureBuilder() {}

PassRefPtr<SkPicture> SkPictureBuilder::endRecording()
{
    m_context->beginRecording(m_bounds);
    m_paintController->endSkippingCache();
    m_paintController->commitNewDisplayItems();
    m_paintController->paintArtifact().replay(*m_context);
    return m_context->endRecording();
}

} // namespace blink
