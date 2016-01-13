// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/win/sink_filter_win.h"

#include "base/logging.h"
#include "media/video/capture/win/sink_input_pin_win.h"

// Define GUID for I420. This is the color format we would like to support but
// it is not defined in the DirectShow SDK.
// http://msdn.microsoft.com/en-us/library/dd757532.aspx
// 30323449-0000-0010-8000-00AA00389B71.
GUID kMediaSubTypeI420 = {
  0x30323449, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}
};

namespace media {

SinkFilterObserver::~SinkFilterObserver() {}

SinkFilter::SinkFilter(SinkFilterObserver* observer)
    : input_pin_(NULL) {
  input_pin_ = new SinkInputPin(this, observer);
}

SinkFilter::~SinkFilter() {
  input_pin_->SetOwner(NULL);
}

void SinkFilter::SetRequestedMediaFormat(const VideoCaptureFormat& format) {
  input_pin_->SetRequestedMediaFormat(format);
}

const VideoCaptureFormat& SinkFilter::ResultingFormat() {
  return input_pin_->ResultingFormat();
}

size_t SinkFilter::NoOfPins() {
  return 1;
}

IPin* SinkFilter::GetPin(int index) {
  return index == 0 ? input_pin_ : NULL;
}

STDMETHODIMP SinkFilter::GetClassID(CLSID* clsid) {
  *clsid = __uuidof(SinkFilter);
  return S_OK;
}

}  // namespace media
