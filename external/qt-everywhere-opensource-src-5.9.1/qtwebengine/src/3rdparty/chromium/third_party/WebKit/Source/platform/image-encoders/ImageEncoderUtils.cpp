// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/image-encoders/ImageEncoderUtils.h"

#include "platform/Histogram.h"
#include "platform/MIMETypeRegistry.h"
#include "wtf/Threading.h"

namespace blink {

const char ImageEncoderUtils::DefaultMimeType[] = "image/png";

// This enum is used in a UMA histogram; the values should not be changed.
enum RequestedImageMimeType {
  RequestedImageMimeTypePng = 0,
  RequestedImageMimeTypeJpeg = 1,
  RequestedImageMimeTypeWebp = 2,
  RequestedImageMimeTypeGif = 3,
  RequestedImageMimeTypeBmp = 4,
  RequestedImageMimeTypeIco = 5,
  RequestedImageMimeTypeTiff = 6,
  RequestedImageMimeTypeUnknown = 7,
  NumberOfRequestedImageMimeTypes
};

String ImageEncoderUtils::toEncodingMimeType(const String& mimeType,
                                             const EncodeReason encodeReason) {
  String lowercaseMimeType = mimeType.lower();

  if (mimeType.isNull())
    lowercaseMimeType = DefaultMimeType;

  RequestedImageMimeType imageFormat;
  if (lowercaseMimeType == "image/png") {
    imageFormat = RequestedImageMimeTypePng;
  } else if (lowercaseMimeType == "image/jpeg") {
    imageFormat = RequestedImageMimeTypeJpeg;
  } else if (lowercaseMimeType == "image/webp") {
    imageFormat = RequestedImageMimeTypeWebp;
  } else if (lowercaseMimeType == "image/gif") {
    imageFormat = RequestedImageMimeTypeGif;
  } else if (lowercaseMimeType == "image/bmp" ||
             lowercaseMimeType == "image/x-windows-bmp") {
    imageFormat = RequestedImageMimeTypeBmp;
  } else if (lowercaseMimeType == "image/x-icon") {
    imageFormat = RequestedImageMimeTypeIco;
  } else if (lowercaseMimeType == "image/tiff" ||
             lowercaseMimeType == "image/x-tiff") {
    imageFormat = RequestedImageMimeTypeTiff;
  } else {
    imageFormat = RequestedImageMimeTypeUnknown;
  }

  if (encodeReason == EncodeReasonToDataURL) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        EnumerationHistogram, toDataURLImageFormatHistogram,
        new EnumerationHistogram("Canvas.RequestedImageMimeTypes_toDataURL",
                                 NumberOfRequestedImageMimeTypes));
    toDataURLImageFormatHistogram.count(imageFormat);
  } else if (encodeReason == EncodeReasonToBlobCallback) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        EnumerationHistogram, toBlobCallbackImageFormatHistogram,
        new EnumerationHistogram(
            "Canvas.RequestedImageMimeTypes_toBlobCallback",
            NumberOfRequestedImageMimeTypes));
    toBlobCallbackImageFormatHistogram.count(imageFormat);
  } else if (encodeReason == EncodeReasonConvertToBlobPromise) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        EnumerationHistogram, convertToBlobPromiseImageFormatHistogram,
        new EnumerationHistogram(
            "Canvas.RequestedImageMimeTypes_convertToBlobPromise",
            NumberOfRequestedImageMimeTypes));
    convertToBlobPromiseImageFormatHistogram.count(imageFormat);
  }

  // FIXME: Make isSupportedImageMIMETypeForEncoding threadsafe (to allow this
  // method to be used on a worker thread).
  if (!MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(lowercaseMimeType))
    lowercaseMimeType = DefaultMimeType;
  return lowercaseMimeType;
}

}  // namespace blink
