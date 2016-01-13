// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/win/video_capture_device_factory_win.h"

#include <mfapi.h>
#include <mferror.h>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/metro.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "media/base/media_switches.h"
#include "media/video/capture/win/video_capture_device_mf_win.h"
#include "media/video/capture/win/video_capture_device_win.h"

using base::win::ScopedCoMem;
using base::win::ScopedComPtr;
using base::win::ScopedVariant;

namespace media {

// Lazy Instance to initialize the MediaFoundation Library.
class MFInitializerSingleton {
 public:
  MFInitializerSingleton() { MFStartup(MF_VERSION, MFSTARTUP_LITE); }
  ~MFInitializerSingleton() { MFShutdown(); }
};

static base::LazyInstance<MFInitializerSingleton> g_mf_initialize =
    LAZY_INSTANCE_INITIALIZER;

static void EnsureMediaFoundationInit() {
  g_mf_initialize.Get();
}

static bool LoadMediaFoundationDlls() {
  static const wchar_t* const kMfDLLs[] = {
    L"%WINDIR%\\system32\\mf.dll",
    L"%WINDIR%\\system32\\mfplat.dll",
    L"%WINDIR%\\system32\\mfreadwrite.dll",
  };

  for (int i = 0; i < arraysize(kMfDLLs); ++i) {
    wchar_t path[MAX_PATH] = {0};
    ExpandEnvironmentStringsW(kMfDLLs[i], path, arraysize(path));
    if (!LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH))
      return false;
  }
  return true;
}

static bool PrepareVideoCaptureAttributesMediaFoundation(
    IMFAttributes** attributes,
    int count) {
  EnsureMediaFoundationInit();

  if (FAILED(MFCreateAttributes(attributes, count)))
    return false;

  return SUCCEEDED((*attributes)->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));
}

static bool CreateVideoCaptureDeviceMediaFoundation(const char* sym_link,
                                                    IMFMediaSource** source) {
  ScopedComPtr<IMFAttributes> attributes;
  if (!PrepareVideoCaptureAttributesMediaFoundation(attributes.Receive(), 2))
    return false;

  attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
                        base::SysUTF8ToWide(sym_link).c_str());

  return SUCCEEDED(MFCreateDeviceSource(attributes, source));
}

static bool EnumerateVideoDevicesMediaFoundation(IMFActivate*** devices,
                                                 UINT32* count) {
  ScopedComPtr<IMFAttributes> attributes;
  if (!PrepareVideoCaptureAttributesMediaFoundation(attributes.Receive(), 1))
    return false;

  return SUCCEEDED(MFEnumDeviceSources(attributes, devices, count));
}

static void GetDeviceNamesDirectShow(VideoCaptureDevice::Names* device_names) {
  DCHECK(device_names);
  DVLOG(1) << " GetDeviceNamesDirectShow";

  ScopedComPtr<ICreateDevEnum> dev_enum;
  HRESULT hr = dev_enum.CreateInstance(CLSID_SystemDeviceEnum, NULL,
                                       CLSCTX_INPROC);
  if (FAILED(hr))
    return;

  ScopedComPtr<IEnumMoniker> enum_moniker;
  hr = dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                       enum_moniker.Receive(), 0);
  // CreateClassEnumerator returns S_FALSE on some Windows OS
  // when no camera exist. Therefore the FAILED macro can't be used.
  if (hr != S_OK)
    return;

  device_names->clear();

  // Name of a fake DirectShow filter that exist on computers with
  // GTalk installed.
  static const char kGoogleCameraAdapter[] = "google camera adapter";

  // Enumerate all video capture devices.
  ScopedComPtr<IMoniker> moniker;
  int index = 0;
  while (enum_moniker->Next(1, moniker.Receive(), NULL) == S_OK) {
    ScopedComPtr<IPropertyBag> prop_bag;
    hr = moniker->BindToStorage(0, 0, IID_IPropertyBag, prop_bag.ReceiveVoid());
    if (FAILED(hr)) {
      moniker.Release();
      continue;
    }

    // Find the description or friendly name.
    ScopedVariant name;
    hr = prop_bag->Read(L"Description", name.Receive(), 0);
    if (FAILED(hr))
      hr = prop_bag->Read(L"FriendlyName", name.Receive(), 0);

    if (SUCCEEDED(hr) && name.type() == VT_BSTR) {
      // Ignore all VFW drivers and the special Google Camera Adapter.
      // Google Camera Adapter is not a real DirectShow camera device.
      // VFW are very old Video for Windows drivers that can not be used.
      const wchar_t* str_ptr = V_BSTR(&name);
      const int name_length = arraysize(kGoogleCameraAdapter) - 1;

      if ((wcsstr(str_ptr, L"(VFW)") == NULL) &&
          lstrlenW(str_ptr) < name_length ||
          (!(LowerCaseEqualsASCII(str_ptr, str_ptr + name_length,
                                  kGoogleCameraAdapter)))) {
        std::string id;
        std::string device_name(base::SysWideToUTF8(str_ptr));
        name.Reset();
        hr = prop_bag->Read(L"DevicePath", name.Receive(), 0);
        if (FAILED(hr) || name.type() != VT_BSTR) {
          id = device_name;
        } else {
          DCHECK_EQ(name.type(), VT_BSTR);
          id = base::SysWideToUTF8(V_BSTR(&name));
        }

        device_names->push_back(VideoCaptureDevice::Name(device_name, id,
            VideoCaptureDevice::Name::DIRECT_SHOW));
      }
    }
    moniker.Release();
  }
}

static void GetDeviceNamesMediaFoundation(
    VideoCaptureDevice::Names* device_names) {
  DVLOG(1) << " GetDeviceNamesMediaFoundation";
  ScopedCoMem<IMFActivate*> devices;
  UINT32 count;
  if (!EnumerateVideoDevicesMediaFoundation(&devices, &count))
    return;

  HRESULT hr;
  for (UINT32 i = 0; i < count; ++i) {
    UINT32 name_size, id_size;
    ScopedCoMem<wchar_t> name, id;
    if (SUCCEEDED(hr = devices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &name_size)) &&
        SUCCEEDED(hr = devices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &id,
            &id_size))) {
      std::wstring name_w(name, name_size), id_w(id, id_size);
      VideoCaptureDevice::Name device(base::SysWideToUTF8(name_w),
          base::SysWideToUTF8(id_w),
          VideoCaptureDevice::Name::MEDIA_FOUNDATION);
      device_names->push_back(device);
    } else {
      DLOG(WARNING) << "GetAllocatedString failed: " << std::hex << hr;
    }
    devices[i]->Release();
  }
}

static void GetDeviceSupportedFormatsDirectShow(
    const VideoCaptureDevice::Name& device,
    VideoCaptureFormats* formats) {
  DVLOG(1) << "GetDeviceSupportedFormatsDirectShow for " << device.name();
  ScopedComPtr<ICreateDevEnum> dev_enum;
  HRESULT hr = dev_enum.CreateInstance(CLSID_SystemDeviceEnum, NULL,
                                       CLSCTX_INPROC);
  if (FAILED(hr))
    return;

  ScopedComPtr<IEnumMoniker> enum_moniker;
  hr = dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                       enum_moniker.Receive(), 0);
  // CreateClassEnumerator returns S_FALSE on some Windows OS when no camera
  // exists. Therefore the FAILED macro can't be used.
  if (hr != S_OK)
    return;

  // Walk the capture devices. No need to check for "google camera adapter",
  // since this is already skipped in the enumeration of GetDeviceNames().
  ScopedComPtr<IMoniker> moniker;
  int index = 0;
  ScopedVariant device_id;
  while (enum_moniker->Next(1, moniker.Receive(), NULL) == S_OK) {
    ScopedComPtr<IPropertyBag> prop_bag;
    hr = moniker->BindToStorage(0, 0, IID_IPropertyBag, prop_bag.ReceiveVoid());
    if (FAILED(hr)) {
      moniker.Release();
      continue;
    }

    device_id.Reset();
    hr = prop_bag->Read(L"DevicePath", device_id.Receive(), 0);
    if (FAILED(hr)) {
      DVLOG(1) << "Couldn't read a device's DevicePath.";
      return;
    }
    if (device.id() == base::SysWideToUTF8(V_BSTR(&device_id)))
      break;
    moniker.Release();
  }

  if (moniker.get()) {
    base::win::ScopedComPtr<IBaseFilter> capture_filter;
    hr = VideoCaptureDeviceWin::GetDeviceFilter(device,
                                                capture_filter.Receive());
    if (!capture_filter) {
      DVLOG(2) << "Failed to create capture filter.";
      return;
    }

    base::win::ScopedComPtr<IPin> output_capture_pin(
        VideoCaptureDeviceWin::GetPin(capture_filter,
                                      PINDIR_OUTPUT,
                                      PIN_CATEGORY_CAPTURE));
    if (!output_capture_pin) {
      DVLOG(2) << "Failed to get capture output pin";
      return;
    }

    ScopedComPtr<IAMStreamConfig> stream_config;
    hr = output_capture_pin.QueryInterface(stream_config.Receive());
    if (FAILED(hr)) {
      DVLOG(2) << "Failed to get IAMStreamConfig interface from "
                  "capture device";
      return;
    }

    int count = 0, size = 0;
    hr = stream_config->GetNumberOfCapabilities(&count, &size);
    if (FAILED(hr)) {
      DVLOG(2) << "Failed to GetNumberOfCapabilities";
      return;
    }

    scoped_ptr<BYTE[]> caps(new BYTE[size]);
    for (int i = 0; i < count; ++i) {
      VideoCaptureDeviceWin::ScopedMediaType media_type;
      hr = stream_config->GetStreamCaps(i, media_type.Receive(), caps.get());
      // GetStreamCaps() may return S_FALSE, so don't use FAILED() or SUCCEED()
      // macros here since they'll trigger incorrectly.
      if (hr != S_OK) {
        DVLOG(2) << "Failed to GetStreamCaps";
        return;
      }

      if (media_type->majortype == MEDIATYPE_Video &&
          media_type->formattype == FORMAT_VideoInfo) {
        VideoCaptureFormat format;
        format.pixel_format =
            VideoCaptureDeviceWin::TranslateMediaSubtypeToPixelFormat(
                media_type->subtype);
        if (format.pixel_format == PIXEL_FORMAT_UNKNOWN)
          continue;
        VIDEOINFOHEADER* h =
            reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);
        format.frame_size.SetSize(h->bmiHeader.biWidth,
                                  h->bmiHeader.biHeight);
        // Trust the frame rate from the VIDEOINFOHEADER.
        format.frame_rate = (h->AvgTimePerFrame > 0) ?
            static_cast<int>(kSecondsToReferenceTime / h->AvgTimePerFrame) :
            0;
        formats->push_back(format);
        DVLOG(1) << device.name() << " resolution: "
             << format.frame_size.ToString() << ", fps: " << format.frame_rate
             << ", pixel format: " << format.pixel_format;
      }
    }
  }
}

static void GetDeviceSupportedFormatsMediaFoundation(
    const VideoCaptureDevice::Name& device,
    VideoCaptureFormats* formats) {
  DVLOG(1) << "GetDeviceSupportedFormatsMediaFoundation for " << device.name();
  ScopedComPtr<IMFMediaSource> source;
  if (!CreateVideoCaptureDeviceMediaFoundation(device.id().c_str(),
                                               source.Receive())) {
    return;
  }

  HRESULT hr;
  base::win::ScopedComPtr<IMFSourceReader> reader;
  if (FAILED(hr = MFCreateSourceReaderFromMediaSource(source, NULL,
                                                      reader.Receive()))) {
    DLOG(ERROR) << "MFCreateSourceReaderFromMediaSource: " << std::hex << hr;
    return;
  }

  DWORD stream_index = 0;
  ScopedComPtr<IMFMediaType> type;
  while (SUCCEEDED(hr = reader->GetNativeMediaType(
         MF_SOURCE_READER_FIRST_VIDEO_STREAM, stream_index, type.Receive()))) {
    UINT32 width, height;
    hr = MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr)) {
      DLOG(ERROR) << "MFGetAttributeSize: " << std::hex << hr;
      return;
    }
    VideoCaptureFormat capture_format;
    capture_format.frame_size.SetSize(width, height);

    UINT32 numerator, denominator;
    hr = MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &numerator, &denominator);
    if (FAILED(hr)) {
      DLOG(ERROR) << "MFGetAttributeSize: " << std::hex << hr;
      return;
    }
    capture_format.frame_rate = denominator ? numerator / denominator : 0;

    GUID type_guid;
    hr = type->GetGUID(MF_MT_SUBTYPE, &type_guid);
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetGUID: " << std::hex << hr;
      return;
    }
    VideoCaptureDeviceMFWin::FormatFromGuid(type_guid,
                                            &capture_format.pixel_format);
    type.Release();
    formats->push_back(capture_format);
    ++stream_index;

    DVLOG(1) << device.name() << " resolution: "
             << capture_format.frame_size.ToString() << ", fps: "
             << capture_format.frame_rate << ", pixel format: "
             << capture_format.pixel_format;
  }
}

// Returns true iff the current platform supports the Media Foundation API
// and that the DLLs are available.  On Vista this API is an optional download
// but the API is advertised as a part of Windows 7 and onwards.  However,
// we've seen that the required DLLs are not available in some Win7
// distributions such as Windows 7 N and Windows 7 KN.
// static
bool VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation() {
  // Even though the DLLs might be available on Vista, we get crashes
  // when running our tests on the build bots.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  static bool g_dlls_available = LoadMediaFoundationDlls();
  return g_dlls_available;
}

VideoCaptureDeviceFactoryWin::VideoCaptureDeviceFactoryWin() {
  // Use Media Foundation for Metro processes (after and including Win8) and
  // DirectShow for any other versions, unless forced via flag. Media Foundation
  // can also be forced if appropriate flag is set and we are in Windows 7 or
  // 8 in non-Metro mode.
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  use_media_foundation_ = (base::win::IsMetroProcess() &&
      !cmd_line->HasSwitch(switches::kForceDirectShowVideoCapture)) ||
     (base::win::GetVersion() >= base::win::VERSION_WIN7 &&
      cmd_line->HasSwitch(switches::kForceMediaFoundationVideoCapture));
}


scoped_ptr<VideoCaptureDevice> VideoCaptureDeviceFactoryWin::Create(
    const VideoCaptureDevice::Name& device_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_ptr<VideoCaptureDevice> device;
  if (device_name.capture_api_type() ==
      VideoCaptureDevice::Name::MEDIA_FOUNDATION) {
    DCHECK(PlatformSupportsMediaFoundation());
    device.reset(new VideoCaptureDeviceMFWin(device_name));
    DVLOG(1) << " MediaFoundation Device: " << device_name.name();
    ScopedComPtr<IMFMediaSource> source;
    if (!CreateVideoCaptureDeviceMediaFoundation(device_name.id().c_str(),
                                                 source.Receive())) {
      return scoped_ptr<VideoCaptureDevice>();
    }
    if (!static_cast<VideoCaptureDeviceMFWin*>(device.get())->Init(source))
      device.reset();
  } else if (device_name.capture_api_type() ==
             VideoCaptureDevice::Name::DIRECT_SHOW) {
    device.reset(new VideoCaptureDeviceWin(device_name));
    DVLOG(1) << " DirectShow Device: " << device_name.name();
    if (!static_cast<VideoCaptureDeviceWin*>(device.get())->Init())
      device.reset();
  } else {
    NOTREACHED() << " Couldn't recognize VideoCaptureDevice type";
  }
  return device.Pass();
}

void VideoCaptureDeviceFactoryWin::GetDeviceNames(
    VideoCaptureDevice::Names* device_names) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (use_media_foundation_)
    GetDeviceNamesMediaFoundation(device_names);
  else
    GetDeviceNamesDirectShow(device_names);
}

void VideoCaptureDeviceFactoryWin::GetDeviceSupportedFormats(
    const VideoCaptureDevice::Name& device,
    VideoCaptureFormats* formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (use_media_foundation_)
    GetDeviceSupportedFormatsMediaFoundation(device, formats);
  else
    GetDeviceSupportedFormatsDirectShow(device, formats);
}

}  // namespace media
