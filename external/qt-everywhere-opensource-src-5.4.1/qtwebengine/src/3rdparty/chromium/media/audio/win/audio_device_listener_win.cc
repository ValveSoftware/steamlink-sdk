// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_device_listener_win.h"

#include <Audioclient.h>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system_monitor/system_monitor.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "media/audio/win/core_audio_util_win.h"

using base::win::ScopedCoMem;

namespace media {

static std::string FlowToString(EDataFlow flow) {
  return (flow == eRender) ? "eRender" : "eConsole";
}

static std::string RoleToString(ERole role) {
  switch (role) {
    case eConsole: return "eConsole";
    case eMultimedia: return "eMultimedia";
    case eCommunications: return "eCommunications";
    default: return "undefined";
  }
}

static std::string GetDeviceId(EDataFlow flow,
                               ERole role) {
  ScopedComPtr<IMMDevice> device =
      CoreAudioUtil::CreateDefaultDevice(flow, role);
  if (!device) {
    // Most probable reason for ending up here is that all audio devices are
    // disabled or unplugged.
    DVLOG(1) << "CoreAudioUtil::CreateDefaultDevice failed. No device?";
    return std::string();
  }

  AudioDeviceName device_name;
  HRESULT hr = CoreAudioUtil::GetDeviceName(device, &device_name);
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to retrieve the device id: " << std::hex << hr;
    return std::string();
  }

  return device_name.unique_id;
}

AudioDeviceListenerWin::AudioDeviceListenerWin(const base::Closure& listener_cb)
    : listener_cb_(listener_cb) {
  CHECK(CoreAudioUtil::IsSupported());

  ScopedComPtr<IMMDeviceEnumerator> device_enumerator(
      CoreAudioUtil::CreateDeviceEnumerator());
  if (!device_enumerator)
    return;

  HRESULT hr = device_enumerator->RegisterEndpointNotificationCallback(this);
  if (FAILED(hr)) {
    LOG(ERROR)  << "RegisterEndpointNotificationCallback failed: "
                << std::hex << hr;
    return;
  }

  device_enumerator_ = device_enumerator;

  default_render_device_id_ = GetDeviceId(eRender, eConsole);
  default_capture_device_id_ = GetDeviceId(eCapture, eConsole);
  default_communications_render_device_id_ =
      GetDeviceId(eRender, eCommunications);
  default_communications_capture_device_id_ =
      GetDeviceId(eCapture, eCommunications);
}

AudioDeviceListenerWin::~AudioDeviceListenerWin() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_enumerator_) {
    HRESULT hr =
        device_enumerator_->UnregisterEndpointNotificationCallback(this);
    LOG_IF(ERROR, FAILED(hr)) << "UnregisterEndpointNotificationCallback() "
                              << "failed: " << std::hex << hr;
  }
}

STDMETHODIMP_(ULONG) AudioDeviceListenerWin::AddRef() {
  return 1;
}

STDMETHODIMP_(ULONG) AudioDeviceListenerWin::Release() {
  return 1;
}

STDMETHODIMP AudioDeviceListenerWin::QueryInterface(REFIID iid, void** object) {
  if (iid == IID_IUnknown || iid == __uuidof(IMMNotificationClient)) {
    *object = static_cast<IMMNotificationClient*>(this);
    return S_OK;
  }

  *object = NULL;
  return E_NOINTERFACE;
}

STDMETHODIMP AudioDeviceListenerWin::OnPropertyValueChanged(
    LPCWSTR device_id, const PROPERTYKEY key) {
  // TODO(dalecurtis): We need to handle changes for the current default device
  // here.  It's tricky because this method may be called many (20+) times for
  // a single change like sample rate.  http://crbug.com/153056
  return S_OK;
}

STDMETHODIMP AudioDeviceListenerWin::OnDeviceAdded(LPCWSTR device_id) {
  // We don't care when devices are added.
  return S_OK;
}

STDMETHODIMP AudioDeviceListenerWin::OnDeviceRemoved(LPCWSTR device_id) {
  // We don't care when devices are removed.
  return S_OK;
}

STDMETHODIMP AudioDeviceListenerWin::OnDeviceStateChanged(LPCWSTR device_id,
                                                          DWORD new_state) {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->ProcessDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE);

  return S_OK;
}

STDMETHODIMP AudioDeviceListenerWin::OnDefaultDeviceChanged(
    EDataFlow flow, ERole role, LPCWSTR new_default_device_id) {
  // Only listen for console and communication device changes.
  if ((role != eConsole && role != eCommunications) ||
      (flow != eRender && flow != eCapture)) {
    return S_OK;
  }

  // Grab a pointer to the appropriate ID member.
  // Note that there are three "?:"'s here to select the right ID.
  std::string* current_device_id =
      role == eRender ? (
          flow == eConsole ?
              &default_render_device_id_ :
              &default_communications_render_device_id_
      ) : (
          flow == eConsole ?
              &default_capture_device_id_ :
              &default_communications_capture_device_id_
      );

  // If no device is now available, |new_default_device_id| will be NULL.
  std::string new_device_id;
  if (new_default_device_id)
    new_device_id = base::WideToUTF8(new_default_device_id);

  VLOG(1) << "OnDefaultDeviceChanged() "
          << "new_default_device: "
          << (new_default_device_id ?
              CoreAudioUtil::GetFriendlyName(new_device_id) : "No device")
          << ", flow: " << FlowToString(flow)
          << ", role: " << RoleToString(role);

  // Only fire a state change event if the device has actually changed.
  // TODO(dalecurtis): This still seems to fire an extra event on my machine for
  // an unplug event (probably others too); e.g., we get two transitions to a
  // new default device id.
  if (new_device_id.compare(*current_device_id) == 0)
    return S_OK;

  // Store the new id in the member variable (that current_device_id points to).
  *current_device_id = new_device_id;
  listener_cb_.Run();

  return S_OK;
}

}  // namespace media
