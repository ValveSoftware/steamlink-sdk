// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/BitmapImageMetrics.h"

#include "platform/Histogram.h"
#include "wtf/Threading.h"
#include "wtf/text/WTFString.h"

namespace blink {

void BitmapImageMetrics::countDecodedImageType(const String& type) {
  DecodedImageType decodedImageType =
      type == "jpg"
          ? ImageJPEG
          : type == "png"
                ? ImagePNG
                : type == "gif"
                      ? ImageGIF
                      : type == "webp"
                            ? ImageWebP
                            : type == "ico"
                                  ? ImageICO
                                  : type == "bmp"
                                        ? ImageBMP
                                        : DecodedImageType::ImageUnknown;

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, decodedImageTypeHistogram,
      new EnumerationHistogram("Blink.DecodedImageType",
                               DecodedImageTypeEnumEnd));
  decodedImageTypeHistogram.count(decodedImageType);
}

void BitmapImageMetrics::countImageOrientation(
    const ImageOrientationEnum orientation) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, orientationHistogram,
      new EnumerationHistogram("Blink.DecodedImage.Orientation",
                               ImageOrientationEnumEnd));
  orientationHistogram.count(orientation);
}

void BitmapImageMetrics::countImageGamma(SkColorSpace* colorSpace) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, gammaNamedHistogram,
      new EnumerationHistogram("Blink.ColorSpace.Source", GammaEnd));
  gammaNamedHistogram.count(getColorSpaceGamma(colorSpace));
}

void BitmapImageMetrics::countOutputGamma(SkColorSpace* colorSpace) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, gammaNamedHistogram,
      new EnumerationHistogram("Blink.ColorSpace.Destination", GammaEnd));
  gammaNamedHistogram.count(getColorSpaceGamma(colorSpace));
}

BitmapImageMetrics::Gamma BitmapImageMetrics::getColorSpaceGamma(
    SkColorSpace* colorSpace) {
  Gamma gamma = GammaNull;
  if (colorSpace) {
    if (colorSpace->gammaCloseToSRGB()) {
      gamma = GammaSRGB;
    } else if (colorSpace->gammaIsLinear()) {
      gamma = GammaLinear;
    } else {
      gamma = GammaNonStandard;
    }
  }
  return gamma;
}

}  // namespace blink
