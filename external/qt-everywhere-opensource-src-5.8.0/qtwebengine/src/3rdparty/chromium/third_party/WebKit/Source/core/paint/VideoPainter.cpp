// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/VideoPainter.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLVideoElement.h"
#include "core/layout/LayoutVideo.h"
#include "core/paint/ImagePainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/ClipRecorder.h"

namespace blink {

void VideoPainter::paintReplaced(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    WebMediaPlayer* mediaPlayer = m_layoutVideo.mediaElement()->webMediaPlayer();
    bool displayingPoster = m_layoutVideo.videoElement()->shouldDisplayPosterImage();
    if (!displayingPoster && !mediaPlayer)
        return;

    LayoutRect rect(m_layoutVideo.videoBox());
    if (rect.isEmpty())
        return;
    rect.moveBy(paintOffset);

    GraphicsContext& context = paintInfo.context;
    LayoutRect contentRect = m_layoutVideo.contentBoxRect();
    contentRect.moveBy(paintOffset);

    Optional<ClipRecorder> clipRecorder;
    if (!contentRect.contains(rect))
        clipRecorder.emplace(context, m_layoutVideo, paintInfo.displayItemTypeForClipping(), pixelSnappedIntRect(contentRect));

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(context, m_layoutVideo, paintInfo.phase))
        return;

    LayoutObjectDrawingRecorder drawingRecorder(context, m_layoutVideo, paintInfo.phase, contentRect);

    // Video frames are only painted in software for printing or capturing node images via web APIs.
    bool forceSoftwareVideoPaint = paintInfo.getGlobalPaintFlags() & GlobalPaintFlattenCompositingLayers;

    if (displayingPoster || !forceSoftwareVideoPaint) {
        // This will display the poster image, if one is present, and otherwise paint nothing.
        ImagePainter(m_layoutVideo).paintIntoRect(context, rect);
    } else {
        SkPaint videoPaint = context.fillPaint();
        videoPaint.setColor(SK_ColorBLACK);
        m_layoutVideo.videoElement()->paintCurrentFrame(context.canvas(), pixelSnappedIntRect(rect), &videoPaint);
    }
}

} // namespace blink
