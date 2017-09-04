/*
 * Copyright (C) 2009 Apple Inc.
 * Copyright (C) 2009 Google Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/paint/MediaControlsPainter.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/TimeRanges.h"
#include "core/html/shadow/MediaControlElementTypes.h"
#include "core/layout/LayoutBox.h"
#include "core/paint/PaintInfo.h"
#include "core/style/ComputedStyle.h"
#include "platform/graphics/Gradient.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

static const double kCurrentTimeBufferedDelta = 1.0;

typedef WTF::HashMap<const char*, Image*> MediaControlImageMap;
static MediaControlImageMap* gMediaControlImageMap = 0;

// Slider thumb sizes, shard between time and volume.
static const int mediaSliderThumbTouchWidth = 36;  // Touch zone size.
static const int mediaSliderThumbTouchHeight = 48;
static const int mediaSliderThumbPaintWidth = 12;  // Painted area.
static const int mediaSliderThumbPaintHeight = 12;

// Overlay play button size.
static const int mediaOverlayPlayButtonWidth = 48;
static const int mediaOverlayPlayButtonHeight = 48;

// Alpha for disabled elements.
static const float kDisabledAlpha = 0.4;

static Image* platformResource(const char* name) {
  if (!gMediaControlImageMap)
    gMediaControlImageMap = new MediaControlImageMap();
  if (Image* image = gMediaControlImageMap->get(name))
    return image;
  if (Image* image = Image::loadPlatformResource(name).leakRef()) {
    gMediaControlImageMap->set(name, image);
    return image;
  }
  ASSERT_NOT_REACHED();
  return 0;
}

static bool hasSource(const HTMLMediaElement* mediaElement) {
  return mediaElement->getNetworkState() != HTMLMediaElement::kNetworkEmpty &&
         mediaElement->getNetworkState() != HTMLMediaElement::kNetworkNoSource;
}

static FloatRect adjustRectForPadding(IntRect rect,
                                      const LayoutObject* object) {
  FloatRect adjustedRect(rect);

  if (!object)
    return adjustedRect;

  // TODO(liberato): make this more elegant, crbug.com/598861 .
  if (const ComputedStyle* style = object->style()) {
    const float paddingLeft = style->paddingLeft().getFloatValue();
    const float paddingTop = style->paddingTop().getFloatValue();
    const float paddingRight = style->paddingRight().getFloatValue();
    const float paddingBottom = style->paddingBottom().getFloatValue();
    adjustedRect = FloatRect(rect.x() + paddingLeft, rect.y() + paddingTop,
                             rect.width() - paddingLeft - paddingRight,
                             rect.height() - paddingTop - paddingBottom);
  }

  return adjustedRect;
}

static bool paintMediaButton(GraphicsContext& context,
                             const IntRect& rect,
                             Image* image,
                             const LayoutObject* object,
                             bool isEnabled) {
  FloatRect drawRect = adjustRectForPadding(rect, object);

  if (!isEnabled)
    context.beginLayer(kDisabledAlpha);

  context.drawImage(image, drawRect);

  if (!isEnabled)
    context.endLayer();

  return true;
}

static bool paintMediaButton(GraphicsContext& context,
                             const IntRect& rect,
                             Image* image,
                             bool isEnabled = true) {
  return paintMediaButton(context, rect, image, 0, isEnabled);
}

bool MediaControlsPainter::paintMediaMuteButton(const LayoutObject& object,
                                                const PaintInfo& paintInfo,
                                                const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return false;

  static Image* soundNotMuted = platformResource("mediaplayerSoundNotMuted");
  static Image* soundMuted = platformResource("mediaplayerSoundMuted");

  if (!hasSource(mediaElement) || !mediaElement->hasAudio()) {
    return paintMediaButton(paintInfo.context, rect, soundMuted, &object,
                            false);
  }

  if (mediaElement->muted() || mediaElement->volume() <= 0)
    return paintMediaButton(paintInfo.context, rect, soundMuted, &object, true);

  return paintMediaButton(paintInfo.context, rect, soundNotMuted, &object,
                          true);
}

bool MediaControlsPainter::paintMediaPlayButton(const LayoutObject& object,
                                                const PaintInfo& paintInfo,
                                                const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return false;

  static Image* mediaPlay = platformResource("mediaplayerPlay");
  static Image* mediaPause = platformResource("mediaplayerPause");

  // Draw the regular play button grayed out.
  if (!hasSource(mediaElement))
    return paintMediaButton(paintInfo.context, rect, mediaPlay, &object, false);

  Image* image =
      !object.node()->isMediaControlElement() ||
              mediaControlElementType(object.node()) == MediaPlayButton
          ? mediaPlay
          : mediaPause;
  return paintMediaButton(paintInfo.context, rect, image, &object, true);
}

bool MediaControlsPainter::paintMediaOverlayPlayButton(
    const LayoutObject& object,
    const PaintInfo& paintInfo,
    const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return false;

  if (!hasSource(mediaElement) || !mediaElement->paused())
    return false;

  static Image* mediaOverlayPlay = platformResource("mediaplayerOverlayPlay");

  IntRect buttonRect(rect);

  // Overlay play button covers the entire player, so center and draw a
  // smaller button.  Center in the entire element.
  // TODO(liberato): object.enclosingBox()?
  const LayoutBox* box = mediaElement->layoutObject()->enclosingBox();
  if (!box)
    return false;
  int mediaHeight = box->pixelSnappedHeight();
  buttonRect.setX(rect.center().x() - mediaOverlayPlayButtonWidth / 2);
  buttonRect.setY(rect.center().y() - mediaOverlayPlayButtonHeight / 2 +
                  (mediaHeight - rect.height()) / 2);
  buttonRect.setWidth(mediaOverlayPlayButtonWidth);
  buttonRect.setHeight(mediaOverlayPlayButtonHeight);

  return paintMediaButton(paintInfo.context, buttonRect, mediaOverlayPlay);
}

static void paintRoundedSliderBackground(const IntRect& rect,
                                         const ComputedStyle& style,
                                         GraphicsContext& context,
                                         Color sliderBackgroundColor) {
  float borderRadius = rect.height() / 2;
  FloatSize radii(borderRadius, borderRadius);

  context.fillRoundedRect(FloatRoundedRect(rect, radii, radii, radii, radii),
                          sliderBackgroundColor);
}

static void paintSliderRangeHighlight(const IntRect& rect,
                                      const ComputedStyle& style,
                                      GraphicsContext& context,
                                      int startPosition,
                                      int endPosition,
                                      Color startColor,
                                      Color endColor) {
  // Calculate border radius; need to avoid being smaller than half the slider
  // height because of https://bugs.webkit.org/show_bug.cgi?id=30143.
  float borderRadius = rect.height() / 2.0f;
  FloatSize radii(borderRadius, borderRadius);

  // Calculate highlight rectangle and edge dimensions.
  int startOffset = startPosition;
  int endOffset = rect.width() - endPosition;
  int rangeWidth = endPosition - startPosition;

  if (rangeWidth <= 0)
    return;

  // Make sure the range width is bigger than border radius at the edges to
  // retain rounded corners.
  if (startOffset < borderRadius && rangeWidth < borderRadius)
    rangeWidth = borderRadius;
  if (endOffset < borderRadius && rangeWidth < borderRadius)
    rangeWidth = borderRadius;

  // Set rectangle to highlight range.
  IntRect highlightRect = rect;
  highlightRect.move(startOffset, 0);
  highlightRect.setWidth(rangeWidth);

  // Don't bother drawing an empty area.
  if (highlightRect.isEmpty())
    return;

  // Calculate white-grey gradient.
  FloatPoint sliderTopLeft = highlightRect.location();
  FloatPoint sliderBottomLeft = sliderTopLeft;
  sliderBottomLeft.move(0, highlightRect.height());
  RefPtr<Gradient> gradient = Gradient::create(sliderTopLeft, sliderBottomLeft);
  gradient->addColorStop(0.0, startColor);
  gradient->addColorStop(1.0, endColor);

  // Fill highlight rectangle with gradient, potentially rounded if on left or
  // right edge.
  SkPaint gradientPaint(context.fillPaint());
  gradient->applyToPaint(gradientPaint, SkMatrix::I());

  if (startOffset < borderRadius && endOffset < borderRadius)
    context.drawRRect(
        FloatRoundedRect(highlightRect, radii, radii, radii, radii),
        gradientPaint);
  else if (startOffset < borderRadius)
    context.drawRRect(FloatRoundedRect(highlightRect, radii, FloatSize(0, 0),
                                       radii, FloatSize(0, 0)),
                      gradientPaint);
  else if (endOffset < borderRadius)
    context.drawRRect(FloatRoundedRect(highlightRect, FloatSize(0, 0), radii,
                                       FloatSize(0, 0), radii),
                      gradientPaint);
  else
    context.drawRect(highlightRect, gradientPaint);
}

bool MediaControlsPainter::paintMediaSlider(const LayoutObject& object,
                                            const PaintInfo& paintInfo,
                                            const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return false;

  GraphicsContext& context = paintInfo.context;

  // Should we paint the slider partially transparent?
  bool drawUiGrayed = !hasSource(mediaElement);
  if (drawUiGrayed)
    context.beginLayer(kDisabledAlpha);

  paintMediaSliderInternal(object, paintInfo, rect);

  if (drawUiGrayed)
    context.endLayer();

  return true;
}

void MediaControlsPainter::paintMediaSliderInternal(const LayoutObject& object,
                                                    const PaintInfo& paintInfo,
                                                    const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return;

  const ComputedStyle& style = object.styleRef();
  GraphicsContext& context = paintInfo.context;

  // Paint the slider bar in the "no data buffered" state.
  paintRoundedSliderBackground(rect, style, context, Color(0xda, 0xda, 0xda));

  // Draw the buffered range. Since the element may have multiple buffered
  // ranges and it'd be distracting/'busy' to show all of them, show only the
  // buffered range containing the current play head.
  TimeRanges* bufferedTimeRanges = mediaElement->buffered();
  float duration = mediaElement->duration();
  float currentTime = mediaElement->currentTime();
  if (std::isnan(duration) || std::isinf(duration) || !duration ||
      std::isnan(currentTime))
    return;

  for (unsigned i = 0; i < bufferedTimeRanges->length(); ++i) {
    float start = bufferedTimeRanges->start(i, ASSERT_NO_EXCEPTION);
    float end = bufferedTimeRanges->end(i, ASSERT_NO_EXCEPTION);
    // The delta is there to avoid corner cases when buffered
    // ranges is out of sync with current time because of
    // asynchronous media pipeline and current time caching in
    // HTMLMediaElement.
    // This is related to https://www.w3.org/Bugs/Public/show_bug.cgi?id=28125
    // FIXME: Remove this workaround when WebMediaPlayer
    // has an asynchronous pause interface.
    if (std::isnan(start) || std::isnan(end) ||
        start > currentTime + kCurrentTimeBufferedDelta || end < currentTime)
      continue;
    int startPosition = int(start * rect.width() / duration);
    int currentPosition = int(currentTime * rect.width() / duration);
    int endPosition = int(end * rect.width() / duration);

    // Draw highlight before current time.
    Color startColor = Color(0x42, 0x85, 0xf4);
    Color endColor = Color(0x42, 0x85, 0xf4);

    if (currentPosition > startPosition) {
      paintSliderRangeHighlight(rect, style, context, startPosition,
                                currentPosition, startColor, endColor);
    }

    // Draw dark grey highlight after current time.
    startColor = endColor = Color(0x5a, 0x5a, 0x5a);

    if (endPosition > currentPosition) {
      paintSliderRangeHighlight(rect, style, context, currentPosition,
                                endPosition, startColor, endColor);
    }
    return;
  }
}

void MediaControlsPainter::adjustMediaSliderThumbPaintSize(
    const IntRect& rect,
    const ComputedStyle& style,
    IntRect& rectOut) {
  // Adjust the rectangle to be centered, the right size for the image.
  // We do this because it's quite hard to get the thumb touch target
  // to match.  So, we provide the touch target size with
  // adjustMediaSliderThumbSize(), and scale it back when we paint.
  rectOut = rect;

  const float zoomLevel = style.effectiveZoom();
  const float zoomedPaintWidth = mediaSliderThumbPaintWidth * zoomLevel;
  const float zoomedPaintHeight = mediaSliderThumbPaintHeight * zoomLevel;

  rectOut.setX(rect.center().x() - zoomedPaintWidth / 2);
  rectOut.setY(rect.center().y() - zoomedPaintHeight / 2);
  rectOut.setWidth(zoomedPaintWidth);
  rectOut.setHeight(zoomedPaintHeight);
}

bool MediaControlsPainter::paintMediaSliderThumb(const LayoutObject& object,
                                                 const PaintInfo& paintInfo,
                                                 const IntRect& rect) {
  if (!object.node())
    return false;

  const HTMLMediaElement* mediaElement =
      toParentMediaElement(object.node()->ownerShadowHost());
  if (!mediaElement)
    return false;

  if (!hasSource(mediaElement))
    return true;

  static Image* mediaSliderThumb = platformResource("mediaplayerSliderThumb");
  IntRect paintRect;
  const ComputedStyle& style = object.styleRef();
  adjustMediaSliderThumbPaintSize(rect, style, paintRect);
  return paintMediaButton(paintInfo.context, paintRect, mediaSliderThumb);
}

bool MediaControlsPainter::paintMediaVolumeSlider(const LayoutObject& object,
                                                  const PaintInfo& paintInfo,
                                                  const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return false;

  GraphicsContext& context = paintInfo.context;
  const ComputedStyle& style = object.styleRef();

  // Paint the slider bar.
  paintRoundedSliderBackground(rect, style, context, Color(0x5a, 0x5a, 0x5a));

  // Calculate volume position for white background rectangle.
  float volume = mediaElement->volume();
  if (std::isnan(volume) || volume < 0)
    return true;
  if (volume > 1)
    volume = 1;
  if (!hasSource(mediaElement) || !mediaElement->hasAudio() ||
      mediaElement->muted()) {
    volume = 0;
  }

  // Calculate the position relative to the center of the thumb.
  const float fillWidth = volume * rect.width();
  static const Color color = Color(0x42, 0x85, 0xf4);  // blue
  paintSliderRangeHighlight(rect, style, context, 0.0, fillWidth, color, color);

  return true;
}

bool MediaControlsPainter::paintMediaVolumeSliderThumb(
    const LayoutObject& object,
    const PaintInfo& paintInfo,
    const IntRect& rect) {
  if (!object.node())
    return false;

  const HTMLMediaElement* mediaElement =
      toParentMediaElement(object.node()->ownerShadowHost());
  if (!mediaElement)
    return false;

  if (!hasSource(mediaElement) || !mediaElement->hasAudio())
    return true;

  static Image* mediaVolumeSliderThumb =
      platformResource("mediaplayerVolumeSliderThumb");

  IntRect paintRect;
  const ComputedStyle& style = object.styleRef();
  adjustMediaSliderThumbPaintSize(rect, style, paintRect);
  return paintMediaButton(paintInfo.context, paintRect, mediaVolumeSliderThumb);
}

bool MediaControlsPainter::paintMediaFullscreenButton(
    const LayoutObject& object,
    const PaintInfo& paintInfo,
    const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return false;

  static Image* mediaEnterFullscreenButton =
      platformResource("mediaplayerEnterFullscreen");
  static Image* mediaExitFullscreenButton =
      platformResource("mediaplayerExitFullscreen");

  Image* image =
      (mediaControlElementType(object.node()) == MediaExitFullscreenButton)
          ? mediaExitFullscreenButton
          : mediaEnterFullscreenButton;
  const bool isEnabled = hasSource(mediaElement);
  return paintMediaButton(paintInfo.context, rect, image, &object, isEnabled);
}

bool MediaControlsPainter::paintMediaToggleClosedCaptionsButton(
    const LayoutObject& object,
    const PaintInfo& paintInfo,
    const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return false;

  static Image* mediaClosedCaptionButton =
      platformResource("mediaplayerClosedCaption");
  static Image* mediaClosedCaptionButtonDisabled =
      platformResource("mediaplayerClosedCaptionDisabled");

  Image* image = mediaElement->textTracksVisible()
                     ? mediaClosedCaptionButton
                     : mediaClosedCaptionButtonDisabled;
  const bool isEnabled = mediaElement->hasClosedCaptions();
  return paintMediaButton(paintInfo.context, rect, image, &object, isEnabled);
}

bool MediaControlsPainter::paintMediaCastButton(const LayoutObject& object,
                                                const PaintInfo& paintInfo,
                                                const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return false;

  static Image* mediaCastOn = platformResource("mediaplayerCastOn");
  static Image* mediaCastOff = platformResource("mediaplayerCastOff");
  // To ensure that the overlaid cast button is visible when overlaid on pale
  // videos we use a different version of it for the overlaid case with a
  // semi-opaque background.
  static Image* mediaOverlayCastOff =
      platformResource("mediaplayerOverlayCastOff");

  bool isEnabled = mediaElement->hasRemoteRoutes();

  switch (mediaControlElementType(object.node())) {
    case MediaCastOnButton:
      return paintMediaButton(paintInfo.context, rect, mediaCastOn, &object,
                              isEnabled);
    case MediaOverlayCastOnButton:
      return paintMediaButton(paintInfo.context, rect, mediaCastOn);
    case MediaCastOffButton:
      return paintMediaButton(paintInfo.context, rect, mediaCastOff, &object,
                              isEnabled);
    case MediaOverlayCastOffButton:
      return paintMediaButton(paintInfo.context, rect, mediaOverlayCastOff);
    default:
      ASSERT_NOT_REACHED();
      return false;
  }
}

bool MediaControlsPainter::paintMediaTrackSelectionCheckmark(
    const LayoutObject& object,
    const PaintInfo& paintInfo,
    const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return false;

  static Image* mediaTrackSelectionCheckmark =
      platformResource("mediaplayerTrackSelectionCheckmark");
  return paintMediaButton(paintInfo.context, rect,
                          mediaTrackSelectionCheckmark);
}

bool MediaControlsPainter::paintMediaClosedCaptionsIcon(
    const LayoutObject& object,
    const PaintInfo& paintInfo,
    const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return false;

  static Image* mediaClosedCaptionsIcon =
      platformResource("mediaplayerClosedCaptionsIcon");
  return paintMediaButton(paintInfo.context, rect, mediaClosedCaptionsIcon);
}

bool MediaControlsPainter::paintMediaSubtitlesIcon(const LayoutObject& object,
                                                   const PaintInfo& paintInfo,
                                                   const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return false;

  static Image* mediaSubtitlesIcon =
      platformResource("mediaplayerSubtitlesIcon");
  return paintMediaButton(paintInfo.context, rect, mediaSubtitlesIcon);
}

bool MediaControlsPainter::paintMediaOverflowMenu(const LayoutObject& object,
                                                  const PaintInfo& paintInfo,
                                                  const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return false;

  static Image* mediaOverflowButton =
      platformResource("mediaplayerOverflowMenu");
  return paintMediaButton(paintInfo.context, rect, mediaOverflowButton, &object,
                          true);
}

bool MediaControlsPainter::paintMediaDownloadIcon(const LayoutObject& object,
                                                  const PaintInfo& paintInfo,
                                                  const IntRect& rect) {
  const HTMLMediaElement* mediaElement = toParentMediaElement(object);
  if (!mediaElement)
    return false;

  bool isEnabled = hasSource(mediaElement);

  static Image* mediaDownloadIcon = platformResource("mediaplayerDownloadIcon");
  return paintMediaButton(paintInfo.context, rect, mediaDownloadIcon, &object,
                          isEnabled);
}

void MediaControlsPainter::adjustMediaSliderThumbSize(ComputedStyle& style) {
  const float zoomLevel = style.effectiveZoom();

  style.setWidth(
      Length(static_cast<int>(mediaSliderThumbTouchWidth * zoomLevel), Fixed));
  style.setHeight(
      Length(static_cast<int>(mediaSliderThumbTouchHeight * zoomLevel), Fixed));
}

}  // namespace blink
