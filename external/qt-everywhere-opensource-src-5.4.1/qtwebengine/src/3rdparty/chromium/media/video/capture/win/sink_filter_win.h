// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implement a DirectShow sink filter used for receiving captured frames from
// a DirectShow Capture filter.

#ifndef MEDIA_VIDEO_CAPTURE_WIN_SINK_FILTER_WIN_H_
#define MEDIA_VIDEO_CAPTURE_WIN_SINK_FILTER_WIN_H_

#include <windows.h>

#include "base/memory/ref_counted.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"
#include "media/video/capture/win/filter_base_win.h"
#include "media/video/capture/win/sink_filter_observer_win.h"

// Define GUID for I420. This is the color format we would like to support but
// it is not defined in the DirectShow SDK.
// http://msdn.microsoft.com/en-us/library/dd757532.aspx
// 30323449-0000-0010-8000-00AA00389B71.
extern GUID kMediaSubTypeI420;

namespace media {

class SinkInputPin;

class __declspec(uuid("88cdbbdc-a73b-4afa-acbf-15d5e2ce12c3"))
    SinkFilter : public FilterBase {
 public:
  explicit SinkFilter(SinkFilterObserver* observer);
  virtual ~SinkFilter();

  void SetRequestedMediaFormat(const VideoCaptureFormat& format);
  // Returns the format that is negotiated when this
  // filter is connected to a media filter.
  const VideoCaptureFormat& ResultingFormat();

  // Implement FilterBase.
  virtual size_t NoOfPins();
  virtual IPin* GetPin(int index);

  STDMETHOD(GetClassID)(CLSID* clsid);

 private:
  scoped_refptr<SinkInputPin> input_pin_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SinkFilter);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_WIN_SINK_FILTER_WIN_H_
