// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_device_win.h"

#include <ks.h>
#include <ksmedia.h>

#include <algorithm>
#include <list>
#include <utility>

#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "media/base/timestamp_constants.h"
#include "media/capture/video/blob_utils.h"

using base::win::ScopedCoMem;
using base::win::ScopedComPtr;
using base::win::ScopedVariant;

namespace media {

#if DCHECK_IS_ON()
#define DLOG_IF_FAILED_WITH_HRESULT(message, hr)                        \
  {                                                                     \
    DLOG_IF(ERROR, FAILED(hr)) << (message) << ": "                     \
                               << logging::SystemErrorCodeToString(hr); \
  }
#else
#define DLOG_IF_FAILED_WITH_HRESULT(message, hr) \
  {}
#endif

// Check if a Pin matches a category.
bool PinMatchesCategory(IPin* pin, REFGUID category) {
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

// Check if a Pin's MediaType matches a given |major_type|.
bool PinMatchesMajorType(IPin* pin, REFGUID major_type) {
  DCHECK(pin);
  AM_MEDIA_TYPE connection_media_type;
  const HRESULT hr = pin->ConnectionMediaType(&connection_media_type);
  return SUCCEEDED(hr) && connection_media_type.majortype == major_type;
}

// Finds and creates a DirectShow Video Capture filter matching the |device_id|.
// static
HRESULT VideoCaptureDeviceWin::GetDeviceFilter(const std::string& device_id,
                                               IBaseFilter** filter) {
  DCHECK(filter);

  ScopedComPtr<ICreateDevEnum> dev_enum;
  HRESULT hr =
      dev_enum.CreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC);
  if (FAILED(hr))
    return hr;

  ScopedComPtr<IEnumMoniker> enum_moniker;
  hr = dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                       enum_moniker.Receive(), 0);
  // CreateClassEnumerator returns S_FALSE on some Windows OS
  // when no camera exist. Therefore the FAILED macro can't be used.
  if (hr != S_OK)
    return hr;

  ScopedComPtr<IBaseFilter> capture_filter;
  for (ScopedComPtr<IMoniker> moniker;
       enum_moniker->Next(1, moniker.Receive(), NULL) == S_OK;
       moniker.Release()) {
    ScopedComPtr<IPropertyBag> prop_bag;
    hr = moniker->BindToStorage(0, 0, IID_IPropertyBag, prop_bag.ReceiveVoid());
    if (FAILED(hr))
      continue;

    // Find |device_id| via DevicePath, Description or FriendlyName, whichever
    // is available first and is a VT_BSTR (i.e. String) type.
    static const wchar_t* kPropertyNames[] = {
        L"DevicePath", L"Description", L"FriendlyName"};

    ScopedVariant name;
    for (const auto* property_name : kPropertyNames) {
      prop_bag->Read(property_name, name.Receive(), 0);
      if (name.type() == VT_BSTR)
        break;
    }

    if (name.type() == VT_BSTR) {
      const std::string device_path(base::SysWideToUTF8(V_BSTR(name.ptr())));
      if (device_path.compare(device_id) == 0) {
        // We have found the requested device
        hr = moniker->BindToObject(0, 0, IID_IBaseFilter,
                                   capture_filter.ReceiveVoid());
        DLOG_IF(ERROR, FAILED(hr)) << "Failed to bind camera filter: "
                                   << logging::SystemErrorCodeToString(hr);
        break;
      }
    }
  }

  *filter = capture_filter.Detach();
  if (!*filter && SUCCEEDED(hr))
    hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

  return hr;
}

// Finds an IPin on an IBaseFilter given the direction, Category and/or Major
// Type. If either |category| or |major_type| are GUID_NULL, they are ignored.
// static
ScopedComPtr<IPin> VideoCaptureDeviceWin::GetPin(IBaseFilter* filter,
                                                 PIN_DIRECTION pin_dir,
                                                 REFGUID category,
                                                 REFGUID major_type) {
  ScopedComPtr<IPin> pin;
  ScopedComPtr<IEnumPins> pin_enum;
  HRESULT hr = filter->EnumPins(pin_enum.Receive());
  if (pin_enum.get() == NULL)
    return pin;

  // Get first unconnected pin.
  hr = pin_enum->Reset();  // set to first pin
  while ((hr = pin_enum->Next(1, pin.Receive(), NULL)) == S_OK) {
    PIN_DIRECTION this_pin_dir = static_cast<PIN_DIRECTION>(-1);
    hr = pin->QueryDirection(&this_pin_dir);
    if (pin_dir == this_pin_dir) {
      if ((category == GUID_NULL || PinMatchesCategory(pin.get(), category)) &&
          (major_type == GUID_NULL ||
           PinMatchesMajorType(pin.get(), major_type))) {
        return pin;
      }
    }
    pin.Release();
  }

  DCHECK(!pin.get());
  return pin;
}

// static
VideoPixelFormat
VideoCaptureDeviceWin::TranslateMediaSubtypeToPixelFormat(
    const GUID& sub_type) {
  static struct {
    const GUID& sub_type;
    VideoPixelFormat format;
  } const kMediaSubtypeToPixelFormatCorrespondence[] = {
      {kMediaSubTypeI420, PIXEL_FORMAT_I420},
      {MEDIASUBTYPE_IYUV, PIXEL_FORMAT_I420},
      {MEDIASUBTYPE_RGB24, PIXEL_FORMAT_RGB24},
      {MEDIASUBTYPE_YUY2, PIXEL_FORMAT_YUY2},
      {MEDIASUBTYPE_MJPG, PIXEL_FORMAT_MJPEG},
      {MEDIASUBTYPE_UYVY, PIXEL_FORMAT_UYVY},
      {MEDIASUBTYPE_ARGB32, PIXEL_FORMAT_ARGB},
      {kMediaSubTypeHDYC, PIXEL_FORMAT_UYVY},
      {kMediaSubTypeY16, PIXEL_FORMAT_Y16},
      {kMediaSubTypeZ16, PIXEL_FORMAT_Y16},
      {kMediaSubTypeINVZ, PIXEL_FORMAT_Y16},
  };
  for (const auto& pixel_format : kMediaSubtypeToPixelFormatCorrespondence) {
    if (sub_type == pixel_format.sub_type)
      return pixel_format.format;
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
  media_type_ = NULL;
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

VideoCaptureDeviceWin::VideoCaptureDeviceWin(
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : device_descriptor_(device_descriptor), state_(kIdle) {
  // TODO(mcasas): Check that CoInitializeEx() has been called with the
  // appropriate Apartment model, i.e., Single Threaded.
}

VideoCaptureDeviceWin::~VideoCaptureDeviceWin() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (media_control_.get())
    media_control_->Stop();

  if (graph_builder_.get()) {
    if (sink_filter_.get()) {
      graph_builder_->RemoveFilter(sink_filter_.get());
      sink_filter_ = NULL;
    }

    if (capture_filter_.get())
      graph_builder_->RemoveFilter(capture_filter_.get());
  }

  if (capture_graph_builder_.get())
    capture_graph_builder_.Release();
}

bool VideoCaptureDeviceWin::Init() {
  DCHECK(thread_checker_.CalledOnValidThread());
  HRESULT hr;

  hr = GetDeviceFilter(device_descriptor_.device_id, capture_filter_.Receive());
  DLOG_IF_FAILED_WITH_HRESULT("Failed to create capture filter", hr);
  if (!capture_filter_.get())
    return false;

  output_capture_pin_ = GetPin(capture_filter_.get(), PINDIR_OUTPUT,
                               PIN_CATEGORY_CAPTURE, GUID_NULL);
  if (!output_capture_pin_.get()) {
    DLOG(ERROR) << "Failed to get capture output pin";
    return false;
  }

  // Create the sink filter used for receiving Captured frames.
  sink_filter_ = new SinkFilter(this);
  if (sink_filter_.get() == NULL) {
    DLOG(ERROR) << "Failed to create sink filter";
    return false;
  }

  input_sink_pin_ = sink_filter_->GetPin(0);

  hr = graph_builder_.CreateInstance(CLSID_FilterGraph, NULL,
                                     CLSCTX_INPROC_SERVER);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to create capture filter", hr);
  if (FAILED(hr))
    return false;

  hr = capture_graph_builder_.CreateInstance(CLSID_CaptureGraphBuilder2, NULL,
                                             CLSCTX_INPROC);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to create the Capture Graph Builder", hr);
  if (FAILED(hr))
    return false;

  hr = capture_graph_builder_->SetFiltergraph(graph_builder_.get());
  DLOG_IF_FAILED_WITH_HRESULT("Failed to give graph to capture graph builder",
                              hr);
  if (FAILED(hr))
    return false;

  hr = graph_builder_.QueryInterface(media_control_.Receive());
  DLOG_IF_FAILED_WITH_HRESULT("Failed to create media control builder", hr);
  if (FAILED(hr))
    return false;

  hr = graph_builder_->AddFilter(capture_filter_.get(), NULL);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to add the capture device to the graph",
                              hr);
  if (FAILED(hr))
    return false;

  hr = graph_builder_->AddFilter(sink_filter_.get(), NULL);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to add the sink filter to the graph", hr);
  if (FAILED(hr))
    return false;

  // The following code builds the upstream portions of the graph, for example
  // if a capture device uses a Windows Driver Model (WDM) driver, the graph may
  // require certain filters upstream from the WDM Video Capture filter, such as
  // a TV Tuner filter or an Analog Video Crossbar filter. We try using the more
  // prevalent MEDIATYPE_Interleaved first.
  base::win::ScopedComPtr<IAMStreamConfig> stream_config;

  hr = capture_graph_builder_->FindInterface(
      &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, capture_filter_.get(),
      IID_IAMStreamConfig, (void**)stream_config.Receive());
  if (FAILED(hr)) {
    hr = capture_graph_builder_->FindInterface(
        &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, capture_filter_.get(),
        IID_IAMStreamConfig, (void**)stream_config.Receive());
    DLOG_IF_FAILED_WITH_HRESULT("Failed to find CapFilter:IAMStreamConfig", hr);
  }

  return CreateCapabilityMap();
}

void VideoCaptureDeviceWin::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ != kIdle)
    return;

  client_ = std::move(client);

  // Get the camera capability that best match the requested format.
  const CapabilityWin found_capability =
      GetBestMatchedCapability(params.requested_format, capabilities_);

  // Reduce the frame rate if the requested frame rate is lower
  // than the capability.
  const float frame_rate =
      std::min(params.requested_format.frame_rate,
               found_capability.supported_format.frame_rate);

  ScopedComPtr<IAMStreamConfig> stream_config;
  HRESULT hr = output_capture_pin_.QueryInterface(stream_config.Receive());
  if (FAILED(hr)) {
    SetErrorState(FROM_HERE, "Can't get the Capture format settings", hr);
    return;
  }

  int count = 0, size = 0;
  hr = stream_config->GetNumberOfCapabilities(&count, &size);
  if (FAILED(hr)) {
    SetErrorState(FROM_HERE, "Failed to GetNumberOfCapabilities", hr);
    return;
  }

  std::unique_ptr<BYTE[]> caps(new BYTE[size]);
  ScopedMediaType media_type;

  // Get the windows capability from the capture device.
  // GetStreamCaps can return S_FALSE which we consider an error. Therefore the
  // FAILED macro can't be used.
  hr = stream_config->GetStreamCaps(found_capability.stream_index,
                                    media_type.Receive(), caps.get());
  if (hr != S_OK) {
    SetErrorState(FROM_HERE, "Failed to get capture device capabilities", hr);
    return;
  }
  if (media_type->formattype == FORMAT_VideoInfo) {
    VIDEOINFOHEADER* h =
        reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);
    if (frame_rate > 0)
      h->AvgTimePerFrame = kSecondsToReferenceTime / frame_rate;
  }
  // Set the sink filter to request this format.
  sink_filter_->SetRequestedMediaFormat(
      found_capability.supported_format.pixel_format, frame_rate,
      found_capability.info_header);
  // Order the capture device to use this format.
  hr = stream_config->SetFormat(media_type.get());
  if (FAILED(hr)) {
    SetErrorState(FROM_HERE, "Failed to set capture device output format", hr);
    return;
  }

  SetAntiFlickerInCaptureFilter(params);

  if (media_type->subtype == kMediaSubTypeHDYC) {
    // HDYC pixel format, used by the DeckLink capture card, needs an AVI
    // decompressor filter after source, let |graph_builder_| add it.
    hr = graph_builder_->Connect(output_capture_pin_.get(),
                                 input_sink_pin_.get());
  } else {
    hr = graph_builder_->ConnectDirect(output_capture_pin_.get(),
                                       input_sink_pin_.get(), NULL);
  }

  if (FAILED(hr)) {
    SetErrorState(FROM_HERE, "Failed to connect the Capture graph.", hr);
    return;
  }

  hr = media_control_->Pause();
  if (FAILED(hr)) {
    SetErrorState(FROM_HERE, "Failed to pause the Capture device", hr);
    return;
  }

  // Start capturing.
  hr = media_control_->Run();
  if (FAILED(hr)) {
    SetErrorState(FROM_HERE, "Failed to start the Capture device.", hr);
    return;
  }

  state_ = kCapturing;
}

void VideoCaptureDeviceWin::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ != kCapturing)
    return;

  HRESULT hr = media_control_->Stop();
  if (FAILED(hr)) {
    SetErrorState(FROM_HERE, "Failed to stop the capture graph.", hr);
    return;
  }

  graph_builder_->Disconnect(output_capture_pin_.get());
  graph_builder_->Disconnect(input_sink_pin_.get());

  client_.reset();
  state_ = kIdle;
}

void VideoCaptureDeviceWin::TakePhoto(TakePhotoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // DirectShow has other means of capturing still pictures, e.g. connecting a
  // SampleGrabber filter to a PIN_CATEGORY_STILL of |capture_filter_|. This
  // way, however, is not widespread and proves too cumbersome, so we just grab
  // the next captured frame instead.
  take_photo_callbacks_.push(std::move(callback));
}

// Implements SinkFilterObserver::SinkFilterObserver.
void VideoCaptureDeviceWin::FrameReceived(const uint8_t* buffer,
                                          int length,
                                          const VideoCaptureFormat& format,
                                          base::TimeDelta timestamp) {
  if (first_ref_time_.is_null())
    first_ref_time_ = base::TimeTicks::Now();

  // There is a chance that the platform does not provide us with the timestamp,
  // in which case, we use reference time to calculate a timestamp.
  if (timestamp == media::kNoTimestamp)
    timestamp = base::TimeTicks::Now() - first_ref_time_;

  client_->OnIncomingCapturedData(buffer, length, format, 0,
                                  base::TimeTicks::Now(), timestamp);

  while (!take_photo_callbacks_.empty()) {
    TakePhotoCallback cb = std::move(take_photo_callbacks_.front());
    take_photo_callbacks_.pop();

    mojom::BlobPtr blob = Blobify(buffer, length, format);
    if (blob)
      cb.Run(std::move(blob));
  }
}

bool VideoCaptureDeviceWin::CreateCapabilityMap() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScopedComPtr<IAMStreamConfig> stream_config;
  HRESULT hr = output_capture_pin_.QueryInterface(stream_config.Receive());
  DLOG_IF_FAILED_WITH_HRESULT(
      "Failed to get IAMStreamConfig from capture device", hr);
  if (FAILED(hr))
    return false;

  // Get interface used for getting the frame rate.
  ScopedComPtr<IAMVideoControl> video_control;
  hr = capture_filter_.QueryInterface(video_control.Receive());

  int count = 0, size = 0;
  hr = stream_config->GetNumberOfCapabilities(&count, &size);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to GetNumberOfCapabilities", hr);
  if (FAILED(hr))
    return false;

  std::unique_ptr<BYTE[]> caps(new BYTE[size]);
  for (int stream_index = 0; stream_index < count; ++stream_index) {
    ScopedMediaType media_type;
    hr = stream_config->GetStreamCaps(stream_index, media_type.Receive(),
                                      caps.get());
    // GetStreamCaps() may return S_FALSE, so don't use FAILED() or SUCCEED()
    // macros here since they'll trigger incorrectly.
    if (hr != S_OK) {
      DLOG(ERROR) << "Failed to GetStreamCaps";
      return false;
    }

    if (media_type->majortype == MEDIATYPE_Video &&
        media_type->formattype == FORMAT_VideoInfo) {
      VideoCaptureFormat format;
      format.pixel_format =
          TranslateMediaSubtypeToPixelFormat(media_type->subtype);
      if (format.pixel_format == PIXEL_FORMAT_UNKNOWN)
        continue;

      VIDEOINFOHEADER* h =
          reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);
      format.frame_size.SetSize(h->bmiHeader.biWidth, h->bmiHeader.biHeight);

      // Try to get a better |time_per_frame| from IAMVideoControl. If not, use
      // the value from VIDEOINFOHEADER.
      REFERENCE_TIME time_per_frame = h->AvgTimePerFrame;
      if (video_control.get()) {
        ScopedCoMem<LONGLONG> max_fps;
        LONG list_size = 0;
        const SIZE size = {format.frame_size.width(),
                           format.frame_size.height()};
        hr = video_control->GetFrameRateList(output_capture_pin_.get(),
                                             stream_index, size, &list_size,
                                             &max_fps);
        // Can't assume the first value will return the max fps.
        // Sometimes |list_size| will be > 0, but max_fps will be NULL. Some
        // drivers may return an HRESULT of S_FALSE which SUCCEEDED() translates
        // into success, so explicitly check S_OK. See http://crbug.com/306237.
        if (hr == S_OK && list_size > 0 && max_fps) {
          time_per_frame =
              *std::min_element(max_fps.get(), max_fps.get() + list_size);
        }
      }

      format.frame_rate =
          (time_per_frame > 0)
              ? (kSecondsToReferenceTime / static_cast<float>(time_per_frame))
              : 0.0;

      capabilities_.emplace_back(stream_index, format, h->bmiHeader);
    }
  }

  return !capabilities_.empty();
}

// Set the power line frequency removal in |capture_filter_| if available.
void VideoCaptureDeviceWin::SetAntiFlickerInCaptureFilter(
    const VideoCaptureParams& params) {
  const PowerLineFrequency power_line_frequency = GetPowerLineFrequency(params);
  if (power_line_frequency != media::PowerLineFrequency::FREQUENCY_50HZ &&
      power_line_frequency != media::PowerLineFrequency::FREQUENCY_60HZ) {
    return;
  }
  ScopedComPtr<IKsPropertySet> ks_propset;
  DWORD type_support = 0;
  HRESULT hr;
  if (SUCCEEDED(hr = ks_propset.QueryFrom(capture_filter_.get())) &&
      SUCCEEDED(hr = ks_propset->QuerySupported(
                    PROPSETID_VIDCAP_VIDEOPROCAMP,
                    KSPROPERTY_VIDEOPROCAMP_POWERLINE_FREQUENCY,
                    &type_support)) &&
      (type_support & KSPROPERTY_SUPPORT_SET)) {
    KSPROPERTY_VIDEOPROCAMP_S data = {};
    data.Property.Set = PROPSETID_VIDCAP_VIDEOPROCAMP;
    data.Property.Id = KSPROPERTY_VIDEOPROCAMP_POWERLINE_FREQUENCY;
    data.Property.Flags = KSPROPERTY_TYPE_SET;
    data.Value =
        (power_line_frequency == media::PowerLineFrequency::FREQUENCY_50HZ) ? 1
                                                                            : 2;
    data.Flags = KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL;
    hr = ks_propset->Set(PROPSETID_VIDCAP_VIDEOPROCAMP,
                         KSPROPERTY_VIDEOPROCAMP_POWERLINE_FREQUENCY, &data,
                         sizeof(data), &data, sizeof(data));
    DLOG_IF_FAILED_WITH_HRESULT("Anti-flicker setting failed", hr);
  }
}

void VideoCaptureDeviceWin::SetErrorState(
    const tracked_objects::Location& from_here,
    const std::string& reason,
    HRESULT hr) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DLOG_IF_FAILED_WITH_HRESULT(reason, hr);
  state_ = kError;
  client_->OnError(from_here, reason);
}
}  // namespace media
