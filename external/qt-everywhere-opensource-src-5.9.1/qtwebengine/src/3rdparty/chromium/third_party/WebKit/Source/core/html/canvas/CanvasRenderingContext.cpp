/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "core/html/canvas/CanvasRenderingContext.h"

#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/SecurityOrigin.h"

const char* const kLinearRGBCanvasColorSpaceName = "linear-rgb";
const char* const kSRGBCanvasColorSpaceName = "srgb";
const char* const kLegacyCanvasColorSpaceName = "legacy-srgb";

namespace blink {

CanvasRenderingContext::CanvasRenderingContext(
    HTMLCanvasElement* canvas,
    OffscreenCanvas* offscreenCanvas,
    const CanvasContextCreationAttributes& attrs)
    : m_canvas(canvas),
      m_offscreenCanvas(offscreenCanvas),
      m_colorSpace(kLegacyCanvasColorSpace),
      m_creationAttributes(attrs) {
  if (RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled()) {
    if (m_creationAttributes.colorSpace() == kSRGBCanvasColorSpaceName)
      m_colorSpace = kSRGBCanvasColorSpace;
    else if (m_creationAttributes.colorSpace() ==
             kLinearRGBCanvasColorSpaceName)
      m_colorSpace = kLinearRGBCanvasColorSpace;
  }
  // Make m_creationAttributes reflect the effective colorSpace rather than the
  // requested one
  m_creationAttributes.setColorSpace(colorSpaceAsString());
}

WTF::String CanvasRenderingContext::colorSpaceAsString() const {
  switch (m_colorSpace) {
    case kSRGBCanvasColorSpace:
      return kSRGBCanvasColorSpaceName;
    case kLinearRGBCanvasColorSpace:
      return kLinearRGBCanvasColorSpaceName;
    case kLegacyCanvasColorSpace:
      return kLegacyCanvasColorSpaceName;
  };
  CHECK(false);
  return "";
}

sk_sp<SkColorSpace> CanvasRenderingContext::skColorSpace() const {
  switch (m_colorSpace) {
    case kSRGBCanvasColorSpace:
      return SkColorSpace::MakeNamed(SkColorSpace::kSRGB_Named);
    case kLinearRGBCanvasColorSpace:
      return SkColorSpace::MakeNamed(SkColorSpace::kSRGBLinear_Named);
    case kLegacyCanvasColorSpace:
      if (RuntimeEnabledFeatures::colorCorrectRenderingEnabled()) {
        // Legacy colorspace ensures color matching with CSS is preserved.
        // So if CSS is color corrected from sRGB to display space, then
        // canvas must do the same
        return SkColorSpace::MakeNamed(SkColorSpace::kSRGB_Named);
      }
      return nullptr;
  };
  CHECK(false);
  return nullptr;
}

SkColorType CanvasRenderingContext::colorType() const {
  switch (m_colorSpace) {
    case kLinearRGBCanvasColorSpace:
      return kRGBA_F16_SkColorType;
    default:
      return kN32_SkColorType;
  }
}

void CanvasRenderingContext::dispose() {
  // HTMLCanvasElement and CanvasRenderingContext have a circular reference.
  // When the pair is no longer reachable, their destruction order is non-
  // deterministic, so the first of the two to be destroyed needs to notify
  // the other in order to break the circular reference.  This is to avoid
  // an error when CanvasRenderingContext2D::didProcessTask() is invoked
  // after the HTMLCanvasElement is destroyed.
  if (canvas()) {
    canvas()->detachContext();
    m_canvas = nullptr;
  }
}

CanvasRenderingContext::ContextType CanvasRenderingContext::contextTypeFromId(
    const String& id) {
  if (id == "2d")
    return Context2d;
  if (id == "experimental-webgl")
    return ContextExperimentalWebgl;
  if (id == "webgl")
    return ContextWebgl;
  if (id == "webgl2")
    return ContextWebgl2;
  if (id == "bitmaprenderer" &&
      RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled()) {
    return ContextImageBitmap;
  }
  return ContextTypeCount;
}

CanvasRenderingContext::ContextType
CanvasRenderingContext::resolveContextTypeAliases(
    CanvasRenderingContext::ContextType type) {
  if (type == ContextExperimentalWebgl)
    return ContextWebgl;
  return type;
}

bool CanvasRenderingContext::wouldTaintOrigin(
    CanvasImageSource* imageSource,
    SecurityOrigin* destinationSecurityOrigin) {
  const KURL& sourceURL = imageSource->sourceURL();
  bool hasURL = (sourceURL.isValid() && !sourceURL.isAboutBlankURL());

  if (hasURL) {
    if (sourceURL.protocolIsData() ||
        m_cleanURLs.contains(sourceURL.getString()))
      return false;
    if (m_dirtyURLs.contains(sourceURL.getString()))
      return true;
  }

  DCHECK_EQ(!canvas(),
            !!destinationSecurityOrigin);  // Must have one or the other
  bool taintOrigin = imageSource->wouldTaintOrigin(
      destinationSecurityOrigin ? destinationSecurityOrigin
                                : canvas()->getSecurityOrigin());

  if (hasURL) {
    if (taintOrigin)
      m_dirtyURLs.add(sourceURL.getString());
    else
      m_cleanURLs.add(sourceURL.getString());
  }
  return taintOrigin;
}

DEFINE_TRACE(CanvasRenderingContext) {
  visitor->trace(m_canvas);
  visitor->trace(m_offscreenCanvas);
}

}  // namespace blink
