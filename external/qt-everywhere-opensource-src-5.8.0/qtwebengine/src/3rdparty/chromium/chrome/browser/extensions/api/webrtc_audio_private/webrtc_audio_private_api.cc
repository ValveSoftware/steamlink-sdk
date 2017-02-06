// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webrtc_audio_private/webrtc_audio_private_api.h"

#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/permissions/permissions_data.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_output_controller.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

using content::BrowserThread;
using content::RenderProcessHost;
using media::AudioDeviceNames;
using media::AudioManager;

namespace wap = api::webrtc_audio_private;

using api::webrtc_audio_private::RequestInfo;

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<WebrtcAudioPrivateEventService> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

WebrtcAudioPrivateEventService::WebrtcAudioPrivateEventService(
    content::BrowserContext* context)
    : browser_context_(context) {
  // In unit tests, the SystemMonitor may not be created.
  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  if (system_monitor)
    system_monitor->AddDevicesChangedObserver(this);
}

WebrtcAudioPrivateEventService::~WebrtcAudioPrivateEventService() {
}

void WebrtcAudioPrivateEventService::Shutdown() {
  // In unit tests, the SystemMonitor may not be created.
  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  if (system_monitor)
    system_monitor->RemoveDevicesChangedObserver(this);
}

// static
BrowserContextKeyedAPIFactory<WebrtcAudioPrivateEventService>*
WebrtcAudioPrivateEventService::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
const char* WebrtcAudioPrivateEventService::service_name() {
  return "WebrtcAudioPrivateEventService";
}

void WebrtcAudioPrivateEventService::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  switch (device_type) {
    case base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE:
    case base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE:
      SignalEvent();
      break;

    default:
      // No action needed.
      break;
  }
}

void WebrtcAudioPrivateEventService::SignalEvent() {
  using api::webrtc_audio_private::OnSinksChanged::kEventName;

  EventRouter* router = EventRouter::Get(browser_context_);
  if (!router || !router->HasEventListener(kEventName))
    return;

  for (const scoped_refptr<const extensions::Extension>& extension :
       ExtensionRegistry::Get(browser_context_)->enabled_extensions()) {
    const std::string& extension_id = extension->id();
    if (router->ExtensionHasEventListener(extension_id, kEventName) &&
        extension->permissions_data()->HasAPIPermission("webrtcAudioPrivate")) {
      std::unique_ptr<Event> event(
          new Event(events::WEBRTC_AUDIO_PRIVATE_ON_SINKS_CHANGED, kEventName,
                    base::WrapUnique(new base::ListValue())));
      router->DispatchEventToExtension(extension_id, std::move(event));
    }
  }
}

WebrtcAudioPrivateFunction::WebrtcAudioPrivateFunction() {}

WebrtcAudioPrivateFunction::~WebrtcAudioPrivateFunction() {
}

void WebrtcAudioPrivateFunction::GetOutputDeviceNames() {
  scoped_refptr<base::SingleThreadTaskRunner> audio_manager_runner =
      AudioManager::Get()->GetTaskRunner();
  if (!audio_manager_runner->BelongsToCurrentThread()) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    audio_manager_runner->PostTask(
        FROM_HERE,
        base::Bind(&WebrtcAudioPrivateFunction::GetOutputDeviceNames, this));
    return;
  }

  std::unique_ptr<AudioDeviceNames> device_names(new AudioDeviceNames);
  AudioManager::Get()->GetAudioOutputDeviceNames(device_names.get());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&WebrtcAudioPrivateFunction::OnOutputDeviceNames, this,
                 base::Passed(&device_names)));
}

void WebrtcAudioPrivateFunction::OnOutputDeviceNames(
    std::unique_ptr<AudioDeviceNames> device_names) {
  NOTREACHED();
}

bool WebrtcAudioPrivateFunction::GetControllerList(const RequestInfo& request) {
  content::RenderProcessHost* rph = nullptr;

  // If |guest_process_id| is defined, directly use this id to find the
  // corresponding RenderProcessHost.
  if (request.guest_process_id.get()) {
    rph = content::RenderProcessHost::FromID(*request.guest_process_id.get());
  } else if (request.tab_id.get()) {
    int tab_id = *request.tab_id.get();
    content::WebContents* contents = NULL;
    if (!ExtensionTabUtil::GetTabById(tab_id, GetProfile(), true, NULL, NULL,
                                      &contents, NULL)) {
      error_ = extensions::ErrorUtils::FormatErrorMessage(
          extensions::tabs_constants::kTabNotFoundError,
          base::IntToString(tab_id));
      return false;
    }
    rph = contents->GetRenderProcessHost();
  } else {
    return false;
  }

  if (!rph)
    return false;

  rph->GetAudioOutputControllers(
      base::Bind(&WebrtcAudioPrivateFunction::OnControllerList, this));
  return true;
}

void WebrtcAudioPrivateFunction::OnControllerList(
    const content::RenderProcessHost::AudioOutputControllerList& list) {
  NOTREACHED();
}

void WebrtcAudioPrivateFunction::CalculateHMAC(const std::string& raw_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&WebrtcAudioPrivateFunction::CalculateHMAC, this, raw_id));
    return;
  }

  std::string hmac = CalculateHMACImpl(raw_id);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WebrtcAudioPrivateFunction::OnHMACCalculated, this, hmac));
}

void WebrtcAudioPrivateFunction::OnHMACCalculated(const std::string& hmac) {
  NOTREACHED();
}

std::string WebrtcAudioPrivateFunction::CalculateHMACImpl(
    const std::string& raw_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // We don't hash the default device name, and we always return
  // "default" for the default device. There is code in SetActiveSink
  // that transforms "default" to the empty string, and code in
  // GetActiveSink that ensures we return "default" if we get the
  // empty string as the current device ID.
  if (media::AudioDeviceDescription::IsDefaultDevice(raw_id))
    return media::AudioDeviceDescription::kDefaultDeviceId;

  url::Origin security_origin(source_url().GetOrigin());
  return content::GetHMACForMediaDeviceID(device_id_salt(), security_origin,
                                          raw_id);
}

void WebrtcAudioPrivateFunction::InitDeviceIDSalt() {
  device_id_salt_ = GetProfile()->GetResourceContext()->GetMediaDeviceIDSalt();
}

std::string WebrtcAudioPrivateFunction::device_id_salt() const {
  return device_id_salt_;
}

bool WebrtcAudioPrivateGetSinksFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  InitDeviceIDSalt();
  GetOutputDeviceNames();

  return true;
}

void WebrtcAudioPrivateGetSinksFunction::OnOutputDeviceNames(
    std::unique_ptr<AudioDeviceNames> raw_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<wap::SinkInfo> results;
  for (const media::AudioDeviceName& name : *raw_ids) {
    wap::SinkInfo info;
    info.sink_id = CalculateHMACImpl(name.unique_id);
    info.sink_label = name.device_name;
    // TODO(joi): Add other parameters.
    results.push_back(std::move(info));
  }

  // It's safe to directly set the results here (from a thread other
  // than the UI thread, on which an AsyncExtensionFunction otherwise
  // normally runs) because there is one instance of this object per
  // function call, no actor outside of this object is modifying the
  // results_ member, and the different method invocations on this
  // object run strictly in sequence; first RunAsync on the UI thread,
  // then DoQuery on the audio IO thread, then DoneOnUIThread on the
  // UI thread.
  results_.reset(wap::GetSinks::Results::Create(results).release());

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WebrtcAudioPrivateGetSinksFunction::DoneOnUIThread, this));
}

void WebrtcAudioPrivateGetSinksFunction::DoneOnUIThread() {
  SendResponse(true);
}

bool WebrtcAudioPrivateGetActiveSinkFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InitDeviceIDSalt();

  std::unique_ptr<wap::GetActiveSink::Params> params(
      wap::GetActiveSink::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  return GetControllerList(params->request);
}

void WebrtcAudioPrivateGetActiveSinkFunction::OnControllerList(
    const RenderProcessHost::AudioOutputControllerList& controllers) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (controllers.empty()) {
    // If there is no current audio stream for the rvh, we return an
    // empty string as the sink ID.
    DVLOG(2) << "chrome.webrtcAudioPrivate.getActiveSink: No controllers.";
    results_.reset(
        wap::GetActiveSink::Results::Create(std::string()).release());
    SendResponse(true);
  } else {
    DVLOG(2) << "chrome.webrtcAudioPrivate.getActiveSink: "
             << controllers.size() << " controllers.";
    // TODO(joi): Debug-only, DCHECK that all items have the same ID.

    // Send the raw ID through CalculateHMAC, and send the result in
    // OnHMACCalculated.
    (*controllers.begin())->GetOutputDeviceId(
        base::Bind(&WebrtcAudioPrivateGetActiveSinkFunction::CalculateHMAC,
                   this));
  }
}

void WebrtcAudioPrivateGetActiveSinkFunction::OnHMACCalculated(
    const std::string& hmac_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string result = hmac_id;
  if (result.empty()) {
    DVLOG(2) << "Received empty ID, replacing with default ID.";
    result = media::AudioDeviceDescription::kDefaultDeviceId;
  }
  results_.reset(wap::GetActiveSink::Results::Create(result).release());
  SendResponse(true);
}

WebrtcAudioPrivateSetActiveSinkFunction::
    WebrtcAudioPrivateSetActiveSinkFunction()
    : num_remaining_sink_ids_(0) {
}

WebrtcAudioPrivateSetActiveSinkFunction::
~WebrtcAudioPrivateSetActiveSinkFunction() {
}

bool WebrtcAudioPrivateSetActiveSinkFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<wap::SetActiveSink::Params> params(
      wap::SetActiveSink::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  InitDeviceIDSalt();

  if (params->request.guest_process_id.get()) {
    request_info_.guest_process_id.reset(
        new int(*params->request.guest_process_id.get()));
  } else if (params->request.tab_id.get()) {
    request_info_.tab_id.reset(new int(*params->request.tab_id.get()));
  } else {
    return false;
  }

  sink_id_ = params->sink_id;

  return GetControllerList(request_info_);
}

void WebrtcAudioPrivateSetActiveSinkFunction::OnControllerList(
    const RenderProcessHost::AudioOutputControllerList& controllers) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string requested_process_type;
  int requested_process_id;
  if (request_info_.guest_process_id.get()) {
    requested_process_type = "guestProcessId";
    requested_process_id = *request_info_.guest_process_id.get();
  } else {
    requested_process_type = "tabId";
    requested_process_id = *request_info_.tab_id.get();
  }

  controllers_ = controllers;
  num_remaining_sink_ids_ = controllers_.size();
  if (num_remaining_sink_ids_ == 0) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        "No active stream for " + requested_process_type + " *",
        base::IntToString(requested_process_id));
    SendResponse(false);
  } else {
    // We need to get the output device names, and calculate the HMAC
    // for each, to find the raw ID for the ID provided to this API
    // function call.
    GetOutputDeviceNames();
  }
}

void WebrtcAudioPrivateSetActiveSinkFunction::OnOutputDeviceNames(
    std::unique_ptr<AudioDeviceNames> device_names) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::string raw_sink_id;
  if (sink_id_ == media::AudioDeviceDescription::kDefaultDeviceId) {
    DVLOG(2) << "Received default ID, replacing with empty ID.";
    raw_sink_id = "";
  } else {
    for (AudioDeviceNames::const_iterator it = device_names->begin();
         it != device_names->end();
         ++it) {
      if (sink_id_ == CalculateHMACImpl(it->unique_id)) {
        raw_sink_id = it->unique_id;
        break;
      }
    }

    if (raw_sink_id.empty())
      DVLOG(2) << "Found no matching raw sink ID for HMAC " << sink_id_;
  }

  RenderProcessHost::AudioOutputControllerList::const_iterator it =
      controllers_.begin();
  for (; it != controllers_.end(); ++it) {
    (*it)->SwitchOutputDevice(raw_sink_id, base::Bind(
        &WebrtcAudioPrivateSetActiveSinkFunction::SwitchDone, this));
  }
}

void WebrtcAudioPrivateSetActiveSinkFunction::SwitchDone() {
  if (--num_remaining_sink_ids_ == 0) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&WebrtcAudioPrivateSetActiveSinkFunction::DoneOnUIThread,
                   this));
  }
}

void WebrtcAudioPrivateSetActiveSinkFunction::DoneOnUIThread() {
  SendResponse(true);
}

WebrtcAudioPrivateGetAssociatedSinkFunction::
WebrtcAudioPrivateGetAssociatedSinkFunction() {
}

WebrtcAudioPrivateGetAssociatedSinkFunction::
~WebrtcAudioPrivateGetAssociatedSinkFunction() {
}

bool WebrtcAudioPrivateGetAssociatedSinkFunction::RunAsync() {
  params_ = wap::GetAssociatedSink::Params::Create(*args_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  InitDeviceIDSalt();

  AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&WebrtcAudioPrivateGetAssociatedSinkFunction::
                 GetDevicesOnDeviceThread, this));

  return true;
}

void WebrtcAudioPrivateGetAssociatedSinkFunction::GetDevicesOnDeviceThread() {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  AudioManager::Get()->GetAudioInputDeviceNames(&source_devices_);

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&WebrtcAudioPrivateGetAssociatedSinkFunction::
                 GetRawSourceIDOnIOThread,
                 this));
}

void
WebrtcAudioPrivateGetAssociatedSinkFunction::GetRawSourceIDOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  url::Origin security_origin(GURL(params_->security_origin));
  std::string source_id_in_origin(params_->source_id_in_origin);

  // Find the raw source ID for source_id_in_origin.
  std::string raw_source_id;
  for (AudioDeviceNames::const_iterator it = source_devices_.begin();
       it != source_devices_.end();
       ++it) {
    const std::string& id = it->unique_id;
    if (content::DoesMediaDeviceIDMatchHMAC(device_id_salt(), security_origin,
                                            source_id_in_origin, id)) {
      raw_source_id = id;
      DVLOG(2) << "Found raw ID " << raw_source_id
               << " for source ID in origin " << source_id_in_origin;
      break;
    }
  }

  AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&WebrtcAudioPrivateGetAssociatedSinkFunction::
                 GetAssociatedSinkOnDeviceThread,
                 this,
                 raw_source_id));
}

void
WebrtcAudioPrivateGetAssociatedSinkFunction::GetAssociatedSinkOnDeviceThread(
    const std::string& raw_source_id) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());

  // We return an empty string if there is no associated output device.
  std::string raw_sink_id;
  if (!raw_source_id.empty()) {
    raw_sink_id =
        AudioManager::Get()->GetAssociatedOutputDeviceID(raw_source_id);
  }

  CalculateHMAC(raw_sink_id);
}

void WebrtcAudioPrivateGetAssociatedSinkFunction::OnHMACCalculated(
    const std::string& associated_sink_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (associated_sink_id == media::AudioDeviceDescription::kDefaultDeviceId) {
    DVLOG(2) << "Got default ID, replacing with empty ID.";
    results_.reset(wap::GetAssociatedSink::Results::Create("").release());
  } else {
    results_.reset(
        wap::GetAssociatedSink::Results::Create(associated_sink_id).release());
  }

  SendResponse(true);
}

}  // namespace extensions
