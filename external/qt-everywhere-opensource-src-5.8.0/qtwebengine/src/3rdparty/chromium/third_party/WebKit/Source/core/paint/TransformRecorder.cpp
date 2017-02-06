// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TransformRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/TransformDisplayItem.h"

namespace blink {

TransformRecorder::TransformRecorder(GraphicsContext& context, const DisplayItemClient& client, const AffineTransform& transform)
    : m_context(context)
    , m_client(client)
{
    m_skipRecordingForIdentityTransform = transform.isIdentity();

    if (m_skipRecordingForIdentityTransform)
        return;

    m_context.getPaintController().createAndAppend<BeginTransformDisplayItem>(m_client, transform);
}

TransformRecorder::~TransformRecorder()
{
    if (m_skipRecordingForIdentityTransform)
        return;

    m_context.getPaintController().endItem<EndTransformDisplayItem>(m_client);
}

} // namespace blink
