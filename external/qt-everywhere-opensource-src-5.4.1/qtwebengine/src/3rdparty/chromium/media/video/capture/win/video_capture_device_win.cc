// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/win/video_capture_device_win.h"

#include <ks.h>
#include <ksmedia.h>

#include <algorithm>
#include <list>

#include "base/strings/sys_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "media/video/capture/win/video_capture_device_mf_win.h"

using base::win::ScopedCoMem;
using base::win::ScopedComPtr;
using base::win::ScopedVariant;

namespace media {

// Finds and creates a DirectShow Video Capture filter matching the device_name.
// static
HRESULT VideoCaptureDeviceWin::GetDeviceFilter(
    const VideoCaptureDevice::Name& device_name,
    IBaseFilter** filter) {
  DCHECK(filter);

  ScopedComPtr<ICreateDevEnum> dev_enum;
  HRESULT hr = dev_enum.CreateInstance(CLSID_SystemDeviceEnum, NULL,
                                       CLSCTX_INPROC);
  if (FAILED(hr))
    return hr;

  ScopedComPtr<IEnumMoniker> enum_moniker;
  hr = dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                       enum_moniker.Receive(), 0);
  // CreateClassEnumerator returns S_FALSE on some Windows OS
  // when no camera exist. Therefore the FAILED macro can't be used.
  if (hr != S_OK)
    return NULL;

  ScopedComPtr<IMoniker> moniker;
  ScopedComPtr<IBaseFilter> capture_filter;
  DWORD fetched = 0;
  while (enum_moniker->Next(1, moniker.Receive(), &fetched) == S_OK) {
    ScopedComPtr<IPropertyBag> prop_bag;
    hr = moniker->BindToStorage(0, 0, IID_IPropertyBag, prop_bag.ReceiveVoid());
    if (FAILED(hr)) {
      moniker.Release();
      continue;
    }

    // Find the description or friendly name.
    static const wchar_t* kPropertyNames[] = {
      L"DevicePath", L"Description", L"FriendlyName"
    };
    ScopedVariant name;
    for (size_t i = 0;
         i < arraysize(kPropertyNames) && name.type() != VT_BSTR; ++i) {
      prop_bag->Read(kPropertyNames[i], name.Receive(), 0);
    }
    if (name.type() == VT_BSTR) {
      std::string device_path(base::SysWideToUTF8(V_BSTR(&name)));
      if (device_path.compare(device_name.id()) == 0) {
        // We have found the requested device
        hr = moniker->BindToObject(0, 0, IID_IBaseFilter,
                                   capture_filter.ReceiveVoid());
        DVPLOG_IF(2, FAILED(hr)) << "Failed to bind camera filter.";
        break;
      }
    }
    moniker.Release();
  }

  *filter = capture_filter.Detach();
  if (!*filter && SUCCEEDED(hr))
    hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

  return hr;
}

// Check if a Pin matches a category.
// static
bool VideoCaptureDeviceWin::PinMatchesCategory(IPin* pin, REFGUID category) {
  DCHECK(pin);
  bool found = false;
  ScopedComPtr<IKsPropertySet> ks_property;
  HRESULT hr = ks_property.QueryFrom(pin);
  if (SUCCEEDED(hr)) {
    GUID pin_category;
    DWORD return_value;
    hr = ks_property->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0,
                          &pin_category, sizeof(pin_category), &return_value);
    if (SUCCEEDED(hr) && (return_value == sizeof(pin_category))) {
      found = (pin_category == category);
    }
  }
  return found;
}

// Finds a IPin on a IBaseFilter given the direction an category.
// static
ScopedComPtr<IPin> VideoCaptureDeviceWin::GetPin(IBaseFilter* filter,
                                                 PIN_DIRECTION pin_dir,
                                                 REFGUID category) {
  ScopedComPtr<IPin> pin;
  ScopedComPtr<IEnumPins> pin_emum;
  HRESULT hr = filter->EnumPins(pin_emum.Receive());
  if (pin_emum == NULL)
    return pin;

  // Get first unconnected pin.
  hr = pin_emum->Reset();  // set to first pin
  while ((hr = pin_emum->Next(1, pin.Receive(), NULL)) == S_OK) {
    PIN_DIRECTION this_pin_dir = static_cast<PIN_DIRECTION>(-1);
    hr = pin->QueryDirection(&this_pin_dir);
    if (pin_dir == this_pin_dir) {
      if (category == GUID_NULL || PinMatchesCategory(pin, category))
        return pin;
    }
    pin.Release();
  }

  DCHECK(!pin);
  return pin;
}

// static
VideoPixelFormat VideoCaptureDeviceWin::TranslateMediaSubtypeToPixelFormat(
    const GUID& sub_type) {
  static struct {
    const GUID& sub_type;
    VideoPixelFormat format;
  } pixel_formats[] = {
    { kMediaSubTypeI420, PIXEL_FORMAT_I420 },
    { MEDIASUBTYPE_IYUV, PIXEL_FORMAT_I420 },
    { MEDIASUBTYPE_RGB24, PIXEL_FORMAT_RGB24 },
    { MEDIASUBTYPE_YUY2, PIXEL_FORMAT_YUY2 },
    { MEDIASUBTYPE_MJPG, PIXEL_FORMAT_MJPEG },
    { MEDIASUBTYPE_UYVY, PIXEL_FORMAT_UYVY },
    { MEDIASUBTYPE_ARGB32, PIXEL_FORMAT_ARGB },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(pixel_formats); ++i) {
    if (sub_type == pixel_formats[i].sub_type)
      return pixel_formats[i].format;
  }
#ifndef NDEBUG
  WCHAR guid_str[128];
  StringFromGUID2(sub_type, guid_str, arraysize(guid_str));
  DVLOG(2) << "Device (also) supports an unknown media type " << guid_str;
#endif
  return PIXEL_FORMAT_UNKNOWN;
}

void VideoCaptureDeviceWin::ScopedMediaType::Free() {
  if (!media_type_)
    return;

  DeleteMediaType(media_type_);
  media_type_= NULL;
}

AM_MEDIA_TYPE** VideoCaptureDeviceWin::ScopedMediaType::Receive() {
  DCHECK(!media_type_);
  return &media_type_;
}

// Release the format block for a media type.
// http://msdn.microsoft.com/en-us/library/dd375432(VS.85).aspx
void VideoCaptureDeviceWin::ScopedMediaType::FreeMediaType(AM_MEDIA_TYPE* mt) {
  if (mt->cbFormat != 0) {
    CoTaskMemFree(mt->pbFormat);
    mt->cbFormat = 0;
    mt->pbFormat = NULL;
  }
  if (mt->pUnk != NULL) {
    NOTREACHED();
    // pUnk should not be used.
    mt->pUnk->Release();
    mt->pUnk = NULL;
  }
}

// Delete a media type structure that was allocated on the heap.
// http://msdn.microsoft.com/en-us/library/dd375432(VS.85).aspx
void VideoCaptureDeviceWin::ScopedMediaType::DeleteMediaType(
    AM_MEDIA_TYPE* mt) {
  if (mt != NULL) {
    FreeMediaType(mt);
    CoTaskMemFree(mt);
  }
}

VideoCaptureDeviceWin::VideoCaptureDeviceWin(const Name& device_name)
    : device_name_(device_name),
      state_(kIdle) {
  DetachFromThread();
}

VideoCaptureDeviceWin::~VideoCaptureDeviceWin() {
  DCHECK(CalledOnValidThread());
  if (media_control_)
    media_control_->Stop();

  if (graph_builder_) {
    if (sink_filter_) {
      graph_builder_->RemoveFilter(sink_filter_);
      sink_filter_ = NULL;
    }

    if (capture_filter_)
      graph_builder_->RemoveFilter(capture_filter_);

    if (mjpg_filter_)
      graph_builder_->RemoveFilter(mjpg_filter_);
  }
}

bool VideoCaptureDeviceWin::Init() {
  DCHECK(CalledOnValidThread());
  HRESULT hr = GetDeviceFilter(device_name_, capture_filter_.Receive());
  if (!capture_filter_) {
    DVLOG(2) << "Failed to create capture filter.";
    return false;
  }

  output_capture_pin_ =
      GetPin(capture_filter_, PINDIR_OUTPUT, PIN_CATEGORY_CAPTURE);
  if (!output_capture_pin_) {
    DVLOG(2) << "Failed to get capture output pin";
    return false;
  }

  // Create the sink filter used for receiving Captured frames.
  sink_filter_ = new SinkFilter(this);
  if (sink_filter_ == NULL) {
    DVLOG(2) << "Failed to create send filter";
    return false;
  }

  input_sink_pin_ = sink_filter_->GetPin(0);

  hr = graph_builder_.CreateInstance(CLSID_FilterGraph, NULL,
                                     CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to create graph builder.";
    return false;
  }

  hr = graph_builder_.QueryInterface(media_control_.Receive());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to create media control builder.";
    return false;
  }

  hr = graph_builder_->AddFilter(capture_filter_, NULL);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to add the capture device to the graph.";
    return false;
  }

  hr = graph_builder_->AddFilter(sink_filter_, NULL);
  if (FAILED(hr)) {
    DVLOG(2)<< "Failed to add the send filter to the graph.";
    return false;
  }

  return CreateCapabilityMap();
}

void VideoCaptureDeviceWin::AllocateAndStart(
    const VideoCaptureParams& params,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(CalledOnValidThread());
  if (state_ != kIdle)
    return;

  client_ = client.Pass();

  // Get the camera capability that best match the requested resolution.
  const VideoCaptureCapabilityWin& found_capability =
      capabilities_.GetBestMatchedFormat(
          params.requested_format.frame_size.width(),
          params.requested_format.frame_size.height(),
          params.requested_format.frame_rate);
  VideoCaptureFormat format = found_capability.supported_format;

  // Reduce the frame rate if the requested frame rate is lower
  // than the capability.
  if (format.frame_rate > params.requested_format.frame_rate)
    format.frame_rate = params.requested_format.frame_rate;

  ScopedComPtr<IAMStreamConfig> stream_config;
  HRESULT hr = output_capture_pin_.QueryInterface(stream_config.Receive());
  if (FAILED(hr)) {
    SetErrorState("Can't get the Capture format settings");
    return;
  }

  int count = 0, size = 0;
  hr = stream_config->GetNumberOfCapabilities(&count, &size);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to GetNumberOfCapabilities";
    return;
  }

  scoped_ptr<BYTE[]> caps(new BYTE[size]);
  ScopedMediaType media_type;

  // Get the windows capability from the capture device.
  hr = stream_config->GetStreamCaps(
      found_capability.stream_index, media_type.Receive(), caps.get());
  if (SUCCEEDED(hr)) {
    if (media_type->formattype == FORMAT_VideoInfo) {
      VIDEOINFOHEADER* h =
          reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);
      if (format.frame_rate > 0)
        h->AvgTimePerFrame = kSecondsToReferenceTime / format.frame_rate;
    }
    // Set the sink filter to request this format.
    sink_filter_->SetRequestedMediaFormat(format);
    // Order the capture device to use this format.
    hr = stream_config->SetFormat(media_type.get());
  }

  if (FAILED(hr))
    SetErrorState("Failed to set capture device output format");

  if (format.pixel_format == PIXEL_FORMAT_MJPEG && !mjpg_filter_.get()) {
    // Create MJPG filter if we need it.
    hr = mjpg_filter_.CreateInstance(CLSID_MjpegDec, NULL, CLSCTX_INPROC);

    if (SUCCEEDED(hr)) {
      input_mjpg_pin_ = GetPin(mjpg_filter_, PINDIR_INPUT, GUID_NULL);
      output_mjpg_pin_ = GetPin(mjpg_filter_, PINDIR_OUTPUT, GUID_NULL);
      hr = graph_builder_->AddFilter(mjpg_filter_, NULL);
    }

    if (FAILED(hr)) {
      mjpg_filter_.Release();
      input_mjpg_pin_.Release();
      output_mjpg_pin_.Release();
    }
  }

  SetAntiFlickerInCaptureFilter();

  if (format.pixel_format == PIXEL_FORMAT_MJPEG && mjpg_filter_.get()) {
    // Connect the camera to the MJPEG decoder.
    hr = graph_builder_->ConnectDirect(output_capture_pin_, input_mjpg_pin_,
                                       NULL);
    // Connect the MJPEG filter to the Capture filter.
    hr += graph_builder_->ConnectDirect(output_mjpg_pin_, input_sink_pin_,
                                        NULL);
  } else {
    hr = graph_builder_->ConnectDirect(output_capture_pin_, input_sink_pin_,
                                       NULL);
  }

  if (FAILED(hr)) {
    SetErrorState("Failed to connect the Capture graph.");
    return;
  }

  hr = media_control_->Pause();
  if (FAILED(hr)) {
    SetErrorState("Failed to Pause the Capture device. "
                  "Is it already occupied?");
    return;
  }

  // Get the format back from the sink filter after the filter have been
  // connected.
  capture_format_ = sink_filter_->ResultingFormat();

  // Start capturing.
  hr = media_control_->Run();
  if (FAILED(hr)) {
    SetErrorState("Failed to start the Capture device.");
    return;
  }

  state_ = kCapturing;
}

void VideoCaptureDeviceWin::StopAndDeAllocate() {
  DCHECK(CalledOnValidThread());
  if (state_ != kCapturing)
    return;

  HRESULT hr = media_control_->Stop();
  if (FAILED(hr)) {
    SetErrorState("Failed to stop the capture graph.");
    return;
  }

  graph_builder_->Disconnect(output_capture_pin_);
  graph_builder_->Disconnect(input_sink_pin_);

  // If the _mjpg filter exist disconnect it even if it has not been used.
  if (mjpg_filter_) {
    graph_builder_->Disconnect(input_mjpg_pin_);
    graph_builder_->Disconnect(output_mjpg_pin_);
  }

  if (FAILED(hr)) {
    SetErrorState("Failed to Stop the Capture device");
    return;
  }
  client_.reset();
  state_ = kIdle;
}

// Implements SinkFilterObserver::SinkFilterObserver.
void VideoCaptureDeviceWin::FrameReceived(const uint8* buffer,
                                          int length) {
  client_->OnIncomingCapturedData(
      buffer, length, capture_format_, 0, base::TimeTicks::Now());
}

bool VideoCaptureDeviceWin::CreateCapabilityMap() {
  DCHECK(CalledOnValidThread());
  ScopedComPtr<IAMStreamConfig> stream_config;
  HRESULT hr = output_capture_pin_.QueryInterface(stream_config.Receive());
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to get IAMStreamConfig interface from "
                "capture device";
    return false;
  }

  // Get interface used for getting the frame rate.
  ScopedComPtr<IAMVideoControl> video_control;
  hr = capture_filter_.QueryInterface(video_control.Receive());
  DVLOG_IF(2, FAILED(hr)) << "IAMVideoControl Interface NOT SUPPORTED";

  int count = 0, size = 0;
  hr = stream_config->GetNumberOfCapabilities(&count, &size);
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to GetNumberOfCapabilities";
    return false;
  }

  scoped_ptr<BYTE[]> caps(new BYTE[size]);
  for (int i = 0; i < count; ++i) {
    ScopedMediaType media_type;
    hr = stream_config->GetStreamCaps(i, media_type.Receive(), caps.get());
    // GetStreamCaps() may return S_FALSE, so don't use FAILED() or SUCCEED()
    // macros here since they'll trigger incorrectly.
    if (hr != S_OK) {
      DVLOG(2) << "Failed to GetStreamCaps";
      return false;
    }

    if (media_type->majortype == MEDIATYPE_Video &&
        media_type->formattype == FORMAT_VideoInfo) {
      VideoCaptureCapabilityWin capability(i);
      capability.supported_format.pixel_format =
          TranslateMediaSubtypeToPixelFormat(media_type->subtype);
      if (capability.supported_format.pixel_format == PIXEL_FORMAT_UNKNOWN)
        continue;

      VIDEOINFOHEADER* h =
          reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);
      capability.supported_format.frame_size.SetSize(h->bmiHeader.biWidth,
                                                     h->bmiHeader.biHeight);

      // Try to get a better |time_per_frame| from IAMVideoControl.  If not, use
      // the value from VIDEOINFOHEADER.
      REFERENCE_TIME time_per_frame = h->AvgTimePerFrame;
      if (video_control) {
        ScopedCoMem<LONGLONG> max_fps;
        LONG list_size = 0;
        SIZE size = {capability.supported_format.frame_size.width(),
                     capability.supported_format.frame_size.height()};

        // GetFrameRateList doesn't return max frame rate always
        // eg: Logitech Notebook. This may be due to a bug in that API
        // because GetFrameRateList array is reversed in the above camera. So
        // a util method written. Can't assume the first value will return
        // the max fps.
        hr = video_control->GetFrameRateList(output_capture_pin_, i, size,
                                             &list_size, &max_fps);
        // Sometimes |list_size| will be > 0, but max_fps will be NULL.  Some
        // drivers may return an HRESULT of S_FALSE which SUCCEEDED() translates
        // into success, so explicitly check S_OK.  See http://crbug.com/306237.
        if (hr == S_OK && list_size > 0 && max_fps) {
          time_per_frame = *std::min_element(max_fps.get(),
                                             max_fps.get() + list_size);
        }
      }

      capability.supported_format.frame_rate =
          (time_per_frame > 0)
              ? (kSecondsToReferenceTime / static_cast<float>(time_per_frame))
              : 0.0;

      // DirectShow works at the moment only on integer frame_rate but the
      // best capability matching class works on rational frame rates.
      capability.frame_rate_numerator = capability.supported_format.frame_rate;
      capability.frame_rate_denominator = 1;

      capabilities_.Add(capability);
    }
  }

  return !capabilities_.empty();
}

// Set the power line frequency removal in |capture_filter_| if available.
void VideoCaptureDeviceWin::SetAntiFlickerInCaptureFilter() {
  const int power_line_frequency = GetPowerLineFrequencyForLocation();
  if (power_line_frequency != kPowerLine50Hz &&
      power_line_frequency != kPowerLine60Hz) {
    return;
  }
  ScopedComPtr<IKsPropertySet> ks_propset;
  DWORD type_support = 0;
  HRESULT hr;
  if (SUCCEEDED(hr = ks_propset.QueryFrom(capture_filter_)) &&
      SUCCEEDED(hr = ks_propset->QuerySupported(PROPSETID_VIDCAP_VIDEOPROCAMP,
          KSPROPERTY_VIDEOPROCAMP_POWERLINE_FREQUENCY, &type_support)) &&
      (type_support & KSPROPERTY_SUPPORT_SET)) {
    KSPROPERTY_VIDEOPROCAMP_S data = {};
    data.Property.Set = PROPSETID_VIDCAP_VIDEOPROCAMP;
    data.Property.Id = KSPROPERTY_VIDEOPROCAMP_POWERLINE_FREQUENCY;
    data.Property.Flags = KSPROPERTY_TYPE_SET;
    data.Value = (power_line_frequency == kPowerLine50Hz) ? 1 : 2;
    data.Flags = KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL;
    hr = ks_propset->Set(PROPSETID_VIDCAP_VIDEOPROCAMP,
                         KSPROPERTY_VIDEOPROCAMP_POWERLINE_FREQUENCY,
                         &data, sizeof(data), &data, sizeof(data));
    DVLOG_IF(ERROR, FAILED(hr)) << "Anti-flicker setting failed.";
    DVLOG_IF(2, SUCCEEDED(hr)) << "Anti-flicker set correctly.";
  } else {
    DVLOG(2) << "Anti-flicker setting not supported.";
  }
}

void VideoCaptureDeviceWin::SetErrorState(const std::string& reason) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << reason;
  state_ = kError;
  client_->OnError(reason);
}
}  // namespace media
