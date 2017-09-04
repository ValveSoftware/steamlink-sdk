/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/LayoutVideo.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/html/HTMLVideoElement.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutFullScreen.h"
#include "core/paint/VideoPainter.h"
#include "public/platform/WebLayer.h"

namespace blink {

using namespace HTMLNames;

LayoutVideo::LayoutVideo(HTMLVideoElement* video) : LayoutMedia(video) {
  setIntrinsicSize(calculateIntrinsicSize());
}

LayoutVideo::~LayoutVideo() {}

LayoutSize LayoutVideo::defaultSize() {
  return LayoutSize(defaultWidth, defaultHeight);
}

void LayoutVideo::intrinsicSizeChanged() {
  if (videoElement()->shouldDisplayPosterImage())
    LayoutMedia::intrinsicSizeChanged();
  updateIntrinsicSize();
}

void LayoutVideo::updateIntrinsicSize() {
  LayoutSize size = calculateIntrinsicSize();
  size.scale(style()->effectiveZoom());

  // Never set the element size to zero when in a media document.
  if (size.isEmpty() && node()->ownerDocument() &&
      node()->ownerDocument()->isMediaDocument())
    return;

  if (size == intrinsicSize())
    return;

  setIntrinsicSize(size);
  setPreferredLogicalWidthsDirty();
  setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::SizeChanged);
}

LayoutSize LayoutVideo::calculateIntrinsicSize() {
  HTMLVideoElement* video = videoElement();

  // Spec text from 4.8.6
  //
  // The intrinsic width of a video element's playback area is the intrinsic
  // width of the video resource, if that is available; otherwise it is the
  // intrinsic width of the poster frame, if that is available; otherwise it is
  // 300 CSS pixels.
  //
  // The intrinsic height of a video element's playback area is the intrinsic
  // height of the video resource, if that is available; otherwise it is the
  // intrinsic height of the poster frame, if that is available; otherwise it is
  // 150 CSS pixels.
  WebMediaPlayer* webMediaPlayer = mediaElement()->webMediaPlayer();
  if (webMediaPlayer &&
      video->getReadyState() >= HTMLVideoElement::kHaveMetadata) {
    IntSize size = webMediaPlayer->naturalSize();
    if (!size.isEmpty())
      return LayoutSize(size);
  }

  if (video->shouldDisplayPosterImage() && !m_cachedImageSize.isEmpty() &&
      !imageResource()->errorOccurred())
    return m_cachedImageSize;

  // <video> in standalone media documents should not use the default 300x150
  // size since they also have audio-only files. By setting the intrinsic
  // size to 300x1 the video will resize itself in these cases, and audio will
  // have the correct height (it needs to be > 0 for controls to layout
  // properly).
  if (video->ownerDocument() && video->ownerDocument()->isMediaDocument())
    return LayoutSize(defaultSize().width(), LayoutUnit(1));

  return defaultSize();
}

void LayoutVideo::imageChanged(WrappedImagePtr newImage, const IntRect* rect) {
  LayoutMedia::imageChanged(newImage, rect);

  // Cache the image intrinsic size so we can continue to use it to draw the
  // image correctly even if we know the video intrinsic size but aren't able to
  // draw video frames yet (we don't want to scale the poster to the video size
  // without keeping aspect ratio).
  if (videoElement()->shouldDisplayPosterImage())
    m_cachedImageSize = intrinsicSize();

  // The intrinsic size is now that of the image, but in case we already had the
  // intrinsic size of the video we call this here to restore the video size.
  updateIntrinsicSize();
}

bool LayoutVideo::shouldDisplayVideo() const {
  return !videoElement()->shouldDisplayPosterImage();
}

void LayoutVideo::paintReplaced(const PaintInfo& paintInfo,
                                const LayoutPoint& paintOffset) const {
  VideoPainter(*this).paintReplaced(paintInfo, paintOffset);
}

void LayoutVideo::layout() {
  updatePlayer();
  LayoutMedia::layout();
}

HTMLVideoElement* LayoutVideo::videoElement() const {
  return toHTMLVideoElement(node());
}

void LayoutVideo::updateFromElement() {
  LayoutMedia::updateFromElement();
  updatePlayer();

  // If the DisplayMode of the video changed, then we need to paint.
  setShouldDoFullPaintInvalidation();
}

void LayoutVideo::updatePlayer() {
  updateIntrinsicSize();

  WebMediaPlayer* mediaPlayer = mediaElement()->webMediaPlayer();
  if (!mediaPlayer)
    return;

  if (!videoElement()->inActiveDocument())
    return;

  videoElement()->setNeedsCompositingUpdate();
}

LayoutUnit LayoutVideo::computeReplacedLogicalWidth(
    ShouldComputePreferred shouldComputePreferred) const {
  return LayoutReplaced::computeReplacedLogicalWidth(shouldComputePreferred);
}

LayoutUnit LayoutVideo::computeReplacedLogicalHeight(
    LayoutUnit estimatedUsedWidth) const {
  return LayoutReplaced::computeReplacedLogicalHeight(estimatedUsedWidth);
}

LayoutUnit LayoutVideo::minimumReplacedHeight() const {
  return LayoutReplaced::minimumReplacedHeight();
}

LayoutRect LayoutVideo::replacedContentRect() const {
  if (shouldDisplayVideo()) {
    // Video codecs may need to restart from an I-frame when the output is
    // resized. Round size in advance to avoid 1px snap difference.
    // TODO(trchen): The way of rounding is different from LayoutPart just to
    // match existing behavior. This is probably a bug and We should unify it
    // with LayoutPart.
    return LayoutRect(pixelSnappedIntRect(computeObjectFit()));
  }
  // If we are displaying the poster image no pre-rounding is needed, but the
  // size of the image should be used for fitting instead.
  return computeObjectFit(&m_cachedImageSize);
}

bool LayoutVideo::supportsAcceleratedRendering() const {
  return !!mediaElement()->platformLayer();
}

static const LayoutBlock* layoutObjectPlaceholder(
    const LayoutObject* layoutObject) {
  LayoutObject* parent = layoutObject->parent();
  if (!parent)
    return nullptr;

  LayoutFullScreen* fullScreen =
      parent->isLayoutFullScreen() ? toLayoutFullScreen(parent) : 0;
  if (!fullScreen)
    return nullptr;

  return fullScreen->placeholder();
}

LayoutUnit LayoutVideo::offsetLeft(const Element* parent) const {
  if (const LayoutBlock* block = layoutObjectPlaceholder(this))
    return block->offsetLeft(parent);
  return LayoutMedia::offsetLeft(parent);
}

LayoutUnit LayoutVideo::offsetTop(const Element* parent) const {
  if (const LayoutBlock* block = layoutObjectPlaceholder(this))
    return block->offsetTop(parent);
  return LayoutMedia::offsetTop(parent);
}

LayoutUnit LayoutVideo::offsetWidth() const {
  if (const LayoutBlock* block = layoutObjectPlaceholder(this))
    return block->offsetWidth();
  return LayoutMedia::offsetWidth();
}

LayoutUnit LayoutVideo::offsetHeight() const {
  if (const LayoutBlock* block = layoutObjectPlaceholder(this))
    return block->offsetHeight();
  return LayoutMedia::offsetHeight();
}

CompositingReasons LayoutVideo::additionalCompositingReasons() const {
  HTMLMediaElement* element = toHTMLMediaElement(node());
  if (element->isFullscreen() && element->usesOverlayFullscreenVideo())
    return CompositingReasonVideo;

  if (shouldDisplayVideo() && supportsAcceleratedRendering())
    return CompositingReasonVideo;

  return CompositingReasonNone;
}

}  // namespace blink
