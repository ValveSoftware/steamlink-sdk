// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ClipPathRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

ClipPathRecorder::ClipPathRecorder(GraphicsContext& context, const DisplayItemClient& client, const Path& clipPath)
    : m_context(context)
    , m_client(client)
{
    m_context.getPaintController().createAndAppend<BeginClipPathDisplayItem>(m_client, clipPath);
}

ClipPathRecorder::~ClipPathRecorder()
{
    m_context.getPaintController().endItem<EndClipPathDisplayItem>(m_client);
}

} // namespace blink
