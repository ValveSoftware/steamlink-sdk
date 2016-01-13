// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/win/sink_input_pin_win.h"

#include <cstring>

// Avoid including strsafe.h via dshow as it will cause build warnings.
#define NO_DSHOW_STRSAFE
#include <dshow.h>

#include "base/logging.h"

namespace media {

const REFERENCE_TIME kSecondsToReferenceTime = 10000000;

SinkInputPin::SinkInputPin(IBaseFilter* filter,
                           SinkFilterObserver* observer)
    : observer_(observer),
      PinBase(filter) {
}

SinkInputPin::~SinkInputPin() {}

bool SinkInputPin::GetValidMediaType(int index, AM_MEDIA_TYPE* media_type) {
  if (media_type->cbFormat < sizeof(VIDEOINFOHEADER))
    return false;

  VIDEOINFOHEADER* pvi =
      reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);

  ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));
  pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  pvi->bmiHeader.biPlanes = 1;
  pvi->bmiHeader.biClrImportant = 0;
  pvi->bmiHeader.biClrUsed = 0;
  if (requested_format_.frame_rate > 0) {
    pvi->AvgTimePerFrame =
        kSecondsToReferenceTime / requested_format_.frame_rate;
  }

  media_type->majortype = MEDIATYPE_Video;
  media_type->formattype = FORMAT_VideoInfo;
  media_type->bTemporalCompression = FALSE;

  switch (index) {
    case 0: {
      pvi->bmiHeader.biCompression = MAKEFOURCC('I', '4', '2', '0');
      pvi->bmiHeader.biBitCount = 12;  // bit per pixel
      pvi->bmiHeader.biWidth = requested_format_.frame_size.width();
      pvi->bmiHeader.biHeight = requested_format_.frame_size.height();
      pvi->bmiHeader.biSizeImage =
          requested_format_.frame_size.GetArea() * 3 / 2;
      media_type->subtype = kMediaSubTypeI420;
      break;
    }
    case 1: {
      pvi->bmiHeader.biCompression = MAKEFOURCC('Y', 'U', 'Y', '2');
      pvi->bmiHeader.biBitCount = 16;
      pvi->bmiHeader.biWidth = requested_format_.frame_size.width();
      pvi->bmiHeader.biHeight = requested_format_.frame_size.height();
      pvi->bmiHeader.biSizeImage = requested_format_.frame_size.GetArea() * 2;
      media_type->subtype = MEDIASUBTYPE_YUY2;
      break;
    }
    case 2: {
      pvi->bmiHeader.biCompression = BI_RGB;
      pvi->bmiHeader.biBitCount = 24;
      pvi->bmiHeader.biWidth = requested_format_.frame_size.width();
      pvi->bmiHeader.biHeight = requested_format_.frame_size.height();
      pvi->bmiHeader.biSizeImage = requested_format_.frame_size.GetArea() * 3;
      media_type->subtype = MEDIASUBTYPE_RGB24;
      break;
    }
    default:
      return false;
  }

  media_type->bFixedSizeSamples = TRUE;
  media_type->lSampleSize = pvi->bmiHeader.biSizeImage;
  return true;
}

bool SinkInputPin::IsMediaTypeValid(const AM_MEDIA_TYPE* media_type) {
  GUID type = media_type->majortype;
  if (type != MEDIATYPE_Video)
    return false;

  GUID format_type = media_type->formattype;
  if (format_type != FORMAT_VideoInfo)
    return false;

  // Check for the sub types we support.
  GUID sub_type = media_type->subtype;
  VIDEOINFOHEADER* pvi =
      reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);
  if (pvi == NULL)
    return false;

  // Store the incoming width and height.
  resulting_format_.frame_size.SetSize(pvi->bmiHeader.biWidth,
                                       abs(pvi->bmiHeader.biHeight));
  if (pvi->AvgTimePerFrame > 0) {
    resulting_format_.frame_rate =
        static_cast<int>(kSecondsToReferenceTime / pvi->AvgTimePerFrame);
  } else {
    resulting_format_.frame_rate = requested_format_.frame_rate;
  }
  if (sub_type == kMediaSubTypeI420 &&
      pvi->bmiHeader.biCompression == MAKEFOURCC('I', '4', '2', '0')) {
    resulting_format_.pixel_format = PIXEL_FORMAT_I420;
    return true;  // This format is acceptable.
  }
  if (sub_type == MEDIASUBTYPE_YUY2 &&
      pvi->bmiHeader.biCompression == MAKEFOURCC('Y', 'U', 'Y', '2')) {
    resulting_format_.pixel_format = PIXEL_FORMAT_YUY2;
    return true;  // This format is acceptable.
  }
  if (sub_type == MEDIASUBTYPE_RGB24 &&
      pvi->bmiHeader.biCompression == BI_RGB) {
    resulting_format_.pixel_format = PIXEL_FORMAT_RGB24;
    return true;  // This format is acceptable.
  }
  return false;
}

HRESULT SinkInputPin::Receive(IMediaSample* sample) {
  const int length = sample->GetActualDataLength();
  uint8* buffer = NULL;
  if (FAILED(sample->GetPointer(&buffer)))
    return S_FALSE;

  observer_->FrameReceived(buffer, length);
  return S_OK;
}

void SinkInputPin::SetRequestedMediaFormat(const VideoCaptureFormat& format) {
  requested_format_ = format;
  resulting_format_.frame_size.SetSize(0, 0);
  resulting_format_.frame_rate = 0;
  resulting_format_.pixel_format = PIXEL_FORMAT_UNKNOWN;
}

const VideoCaptureFormat& SinkInputPin::ResultingFormat() {
  return resulting_format_;
}

}  // namespace media
