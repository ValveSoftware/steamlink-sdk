// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"

#include <set>
#include <string>
#include <unordered_set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/bluetooth/bluetooth_blacklist.h"
#include "content/browser/bluetooth/bluetooth_metrics.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"

using device::BluetoothUUID;

namespace content {

namespace {
constexpr size_t kMaxLengthForDeviceName =
    29;  // max length of device name in filter.

void LogRequestDeviceOptions(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options) {
  VLOG(1) << "requestDevice called with the following filters: ";
  int i = 0;
  for (const auto& filter : options->filters) {
    VLOG(1) << "Filter #" << ++i;
    if (!filter->name.is_null())
      VLOG(1) << "Name: " << filter->name;

    if (!filter->name_prefix.is_null())
      VLOG(1) << "Name Prefix: " << filter->name_prefix;

    if (!filter->services.is_null()) {
      VLOG(1) << "Services: ";
      VLOG(1) << "\t[";
      for (const auto& service : filter->services)
        VLOG(1) << "\t\t" << service->canonical_value();
      VLOG(1) << "\t]";
    }
  }
}

bool IsEmptyOrInvalidFilter(
    const blink::mojom::WebBluetoothScanFilterPtr& filter) {
  // At least one member needs to be present.
  if (filter->name.is_null() && filter->name_prefix.is_null() &&
      filter->services.is_null())
    return true;

  // The renderer will never send a name or a name_prefix longer than
  // kMaxLengthForDeviceName.
  if (!filter->name.is_null() && filter->name.size() > kMaxLengthForDeviceName)
    return true;
  if (!filter->name_prefix.is_null() &&
      filter->name_prefix.size() > kMaxLengthForDeviceName)
    return true;

  return false;
}

bool HasEmptyOrInvalidFilter(
    const mojo::Array<blink::mojom::WebBluetoothScanFilterPtr>& filters) {
  return filters.empty()
             ? true
             : filters.end() != std::find_if(filters.begin(), filters.end(),
                                             IsEmptyOrInvalidFilter);
}

bool MatchesFilter(const device::BluetoothDevice& device,
                   const blink::mojom::WebBluetoothScanFilterPtr& filter) {
  DCHECK(!IsEmptyOrInvalidFilter(filter));

  // TODO(615720): Use the upcoming GetName (was GetDeviceName).
  const std::string device_name = base::UTF16ToUTF8(device.GetNameForDisplay());

  if (!filter->name.is_null() && (device_name != filter->name)) {
    return false;
  }

  if (!filter->name_prefix.is_null() &&
      (!base::StartsWith(device_name, filter->name_prefix.get(),
                         base::CompareCase::SENSITIVE))) {
    return false;
  }

  if (!filter->services.is_null()) {
    const auto& device_uuid_list = device.GetUUIDs();
    const std::unordered_set<BluetoothUUID, device::BluetoothUUIDHash>
        device_uuids(device_uuid_list.begin(), device_uuid_list.end());
    for (const base::Optional<BluetoothUUID>& service : filter->services) {
      if (!ContainsKey(device_uuids, service.value())) {
        return false;
      }
    }
  }

  return true;
}

bool MatchesFilters(
    const device::BluetoothDevice& device,
    const mojo::Array<blink::mojom::WebBluetoothScanFilterPtr>& filters) {
  DCHECK(!HasEmptyOrInvalidFilter(filters));
  for (const auto& filter : filters) {
    if (MatchesFilter(device, filter)) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<device::BluetoothDiscoveryFilter> ComputeScanFilter(
    const mojo::Array<blink::mojom::WebBluetoothScanFilterPtr>& filters) {
  std::unordered_set<BluetoothUUID, device::BluetoothUUIDHash> services;
  for (const auto& filter : filters) {
    for (const base::Optional<BluetoothUUID>& service : filter->services) {
      services.insert(service.value());
    }
  }
  auto discovery_filter = base::MakeUnique<device::BluetoothDiscoveryFilter>(
      device::BLUETOOTH_TRANSPORT_DUAL);
  for (const BluetoothUUID& service : services) {
    discovery_filter->AddUUID(service);
  }
  return discovery_filter;
}

void StopDiscoverySession(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  // Nothing goes wrong if the discovery session fails to stop, and we don't
  // need to wait for it before letting the user's script proceed, so we ignore
  // the results here.
  discovery_session->Stop(base::Bind(&base::DoNothing),
                          base::Bind(&base::DoNothing));
}

UMARequestDeviceOutcome OutcomeFromChooserEvent(BluetoothChooser::Event event) {
  switch (event) {
    case BluetoothChooser::Event::DENIED_PERMISSION:
      return UMARequestDeviceOutcome::BLUETOOTH_CHOOSER_DENIED_PERMISSION;
    case BluetoothChooser::Event::CANCELLED:
      return UMARequestDeviceOutcome::BLUETOOTH_CHOOSER_CANCELLED;
    case BluetoothChooser::Event::SHOW_OVERVIEW_HELP:
      return UMARequestDeviceOutcome::BLUETOOTH_OVERVIEW_HELP_LINK_PRESSED;
    case BluetoothChooser::Event::SHOW_ADAPTER_OFF_HELP:
      return UMARequestDeviceOutcome::ADAPTER_OFF_HELP_LINK_PRESSED;
    case BluetoothChooser::Event::SHOW_NEED_LOCATION_HELP:
      return UMARequestDeviceOutcome::NEED_LOCATION_HELP_LINK_PRESSED;
    case BluetoothChooser::Event::SELECTED:
      // We can't know if we are going to send a success message yet because
      // the device could have vanished. This event should be histogramed
      // manually after checking if the device is still around.
      NOTREACHED();
      return UMARequestDeviceOutcome::SUCCESS;
    case BluetoothChooser::Event::RESCAN:
      // Rescanning doesn't result in a IPC message for the request being sent
      // so no need to histogram it.
      NOTREACHED();
      return UMARequestDeviceOutcome::SUCCESS;
  }
  NOTREACHED();
  return UMARequestDeviceOutcome::SUCCESS;
}

}  // namespace

BluetoothDeviceChooserController::BluetoothDeviceChooserController(
    WebBluetoothServiceImpl* web_bluetooth_service,
    RenderFrameHost* render_frame_host,
    device::BluetoothAdapter* adapter,
    base::TimeDelta scan_duration)
    : adapter_(adapter),
      web_bluetooth_service_(web_bluetooth_service),
      render_frame_host_(render_frame_host),
      web_contents_(WebContents::FromRenderFrameHost(render_frame_host_)),
      discovery_session_timer_(
          FROM_HERE,
          // TODO(jyasskin): Add a way for tests to control the dialog
          // directly, and change this to a reasonable discovery timeout.
          scan_duration,
          base::Bind(&BluetoothDeviceChooserController::StopDeviceDiscovery,
                     // base::Timer guarantees it won't call back after its
                     // destructor starts.
                     base::Unretained(this)),
          /*is_repeating=*/false),
      weak_ptr_factory_(this) {
  CHECK(adapter_);
}

BluetoothDeviceChooserController::~BluetoothDeviceChooserController() {
  if (chooser_) {
    DCHECK(!error_callback_.is_null());
    error_callback_.Run(blink::mojom::WebBluetoothError::CHOOSER_CANCELLED);
  }
}

void BluetoothDeviceChooserController::GetDevice(
    blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
    const SuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // GetDevice should only be called once.
  DCHECK(success_callback_.is_null());
  DCHECK(error_callback_.is_null());

  success_callback_ = success_callback;
  error_callback_ = error_callback;

  // The renderer should never send empty filters.
  if (HasEmptyOrInvalidFilter(options->filters)) {
    web_bluetooth_service_->CrashRendererAndClosePipe(
        bad_message::BDH_EMPTY_OR_INVALID_FILTERS);
    return;
  }
  options_ = std::move(options);
  LogRequestDeviceOptions(options_);

  // Check blacklist to reject invalid filters and adjust optional_services.
  if (BluetoothBlacklist::Get().IsExcluded(options_->filters)) {
    RecordRequestDeviceOutcome(
        UMARequestDeviceOutcome::BLACKLISTED_SERVICE_IN_FILTER);
    PostErrorCallback(
        blink::mojom::WebBluetoothError::REQUEST_DEVICE_WITH_BLACKLISTED_UUID);
    return;
  }
  BluetoothBlacklist::Get().RemoveExcludedUUIDs(options_.get());

  const url::Origin requesting_origin =
      render_frame_host_->GetLastCommittedOrigin();
  const url::Origin embedding_origin =
      web_contents_->GetMainFrame()->GetLastCommittedOrigin();

  // TODO(crbug.com/518042): Enforce correctly-delegated permissions instead of
  // matching origins. When relaxing this, take care to handle non-sandboxed
  // unique origins.
  if (!embedding_origin.IsSameOriginWith(requesting_origin)) {
    PostErrorCallback(blink::mojom::WebBluetoothError::
                          REQUEST_DEVICE_FROM_CROSS_ORIGIN_IFRAME);
    return;
  }
  // The above also excludes unique origins, which are not even same-origin with
  // themselves.
  DCHECK(!requesting_origin.unique());

  if (!adapter_->IsPresent()) {
    VLOG(1) << "Bluetooth Adapter not present. Can't serve requestDevice.";
    RecordRequestDeviceOutcome(
        UMARequestDeviceOutcome::BLUETOOTH_ADAPTER_NOT_PRESENT);
    PostErrorCallback(blink::mojom::WebBluetoothError::NO_BLUETOOTH_ADAPTER);
    return;
  }

  switch (GetContentClient()->browser()->AllowWebBluetooth(
      web_contents_->GetBrowserContext(), requesting_origin,
      embedding_origin)) {
    case ContentBrowserClient::AllowWebBluetoothResult::BLOCK_POLICY: {
      RecordRequestDeviceOutcome(
          UMARequestDeviceOutcome::BLUETOOTH_CHOOSER_POLICY_DISABLED);
      PostErrorCallback(blink::mojom::WebBluetoothError::
                            CHOOSER_NOT_SHOWN_API_LOCALLY_DISABLED);
      return;
    }
    case ContentBrowserClient::AllowWebBluetoothResult::
        BLOCK_GLOBALLY_DISABLED: {
      // Log to the developer console.
      web_contents_->GetMainFrame()->AddMessageToConsole(
          content::CONSOLE_MESSAGE_LEVEL_LOG,
          "Bluetooth permission has been blocked.");
      // Block requests.
      RecordRequestDeviceOutcome(
          UMARequestDeviceOutcome::BLUETOOTH_GLOBALLY_DISABLED);
      PostErrorCallback(blink::mojom::WebBluetoothError::
                            CHOOSER_NOT_SHOWN_API_GLOBALLY_DISABLED);
      return;
    }
    case ContentBrowserClient::AllowWebBluetoothResult::ALLOW:
      break;
  }

  BluetoothChooser::EventHandler chooser_event_handler =
      base::Bind(&BluetoothDeviceChooserController::OnBluetoothChooserEvent,
                 base::Unretained(this));

  if (WebContentsDelegate* delegate = web_contents_->GetDelegate()) {
    chooser_ = delegate->RunBluetoothChooser(render_frame_host_,
                                             chooser_event_handler);
  }

  if (!chooser_.get()) {
    PostErrorCallback(
        blink::mojom::WebBluetoothError::WEB_BLUETOOTH_NOT_SUPPORTED);
  }

  if (!chooser_->CanAskForScanningPermission()) {
    VLOG(1) << "Closing immediately because Chooser cannot obtain permission.";
    OnBluetoothChooserEvent(BluetoothChooser::Event::DENIED_PERMISSION,
                            "" /* device_address */);
    return;
  }

  PopulateFoundDevices();
  if (!chooser_.get()) {
    // If the dialog's closing, no need to do any of the rest of this.
    return;
  }

  if (!adapter_->IsPowered()) {
    chooser_->SetAdapterPresence(
        BluetoothChooser::AdapterPresence::POWERED_OFF);
    return;
  }

  StartDeviceDiscovery();
}

void BluetoothDeviceChooserController::AddFilteredDevice(
    const device::BluetoothDevice& device) {
  if (chooser_.get() && MatchesFilters(device, options_->filters)) {
    VLOG(1) << "Adding device to chooser: " << device.GetAddress();
    chooser_->AddDevice(device.GetAddress(), device.GetNameForDisplay());
  }
}

void BluetoothDeviceChooserController::AdapterPoweredChanged(bool powered) {
  if (!powered && discovery_session_.get()) {
    StopDiscoverySession(std::move(discovery_session_));
  }

  if (chooser_.get()) {
    chooser_->SetAdapterPresence(
        powered ? BluetoothChooser::AdapterPresence::POWERED_ON
                : BluetoothChooser::AdapterPresence::POWERED_OFF);
    if (powered) {
      OnBluetoothChooserEvent(BluetoothChooser::Event::RESCAN,
                              "" /* device_address */);
    }
  }

  if (!powered) {
    discovery_session_timer_.Stop();
  }
}

void BluetoothDeviceChooserController::PopulateFoundDevices() {
  VLOG(1) << "Populating " << adapter_->GetDevices().size()
          << " devices in chooser.";
  for (const device::BluetoothDevice* device : adapter_->GetDevices()) {
    AddFilteredDevice(*device);
  }
}

void BluetoothDeviceChooserController::StartDeviceDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (discovery_session_.get() && discovery_session_->IsActive()) {
    // Already running; just increase the timeout.
    discovery_session_timer_.Reset();
    return;
  }

  chooser_->ShowDiscoveryState(BluetoothChooser::DiscoveryState::DISCOVERING);
  adapter_->StartDiscoverySessionWithFilter(
      ComputeScanFilter(options_->filters),
      base::Bind(
          &BluetoothDeviceChooserController::OnStartDiscoverySessionSuccess,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(
          &BluetoothDeviceChooserController::OnStartDiscoverySessionFailed,
          weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothDeviceChooserController::StopDeviceDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StopDiscoverySession(std::move(discovery_session_));
  if (chooser_) {
    chooser_->ShowDiscoveryState(BluetoothChooser::DiscoveryState::IDLE);
  }
}

void BluetoothDeviceChooserController::OnStartDiscoverySessionSuccess(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  VLOG(1) << "Started discovery session.";
  if (chooser_.get()) {
    discovery_session_ = std::move(discovery_session);
    discovery_session_timer_.Reset();
  } else {
    StopDiscoverySession(std::move(discovery_session));
  }
}

void BluetoothDeviceChooserController::OnStartDiscoverySessionFailed() {
  if (chooser_.get()) {
    chooser_->ShowDiscoveryState(
        BluetoothChooser::DiscoveryState::FAILED_TO_START);
  }
}

void BluetoothDeviceChooserController::OnBluetoothChooserEvent(
    BluetoothChooser::Event event,
    const std::string& device_address) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Shouldn't recieve an event from a closed chooser.
  DCHECK(chooser_.get());

  switch (event) {
    case BluetoothChooser::Event::RESCAN:
      PopulateFoundDevices();
      DCHECK(chooser_);
      StartDeviceDiscovery();
      // No need to close the chooser so we return.
      return;
    case BluetoothChooser::Event::DENIED_PERMISSION:
      RecordRequestDeviceOutcome(OutcomeFromChooserEvent(event));
      PostErrorCallback(blink::mojom::WebBluetoothError::
                            CHOOSER_NOT_SHOWN_USER_DENIED_PERMISSION_TO_SCAN);
      break;
    case BluetoothChooser::Event::CANCELLED:
      RecordRequestDeviceOutcome(OutcomeFromChooserEvent(event));
      PostErrorCallback(blink::mojom::WebBluetoothError::CHOOSER_CANCELLED);
      break;
    case BluetoothChooser::Event::SHOW_OVERVIEW_HELP:
      VLOG(1) << "Overview Help link pressed.";
      RecordRequestDeviceOutcome(OutcomeFromChooserEvent(event));
      PostErrorCallback(blink::mojom::WebBluetoothError::CHOOSER_CANCELLED);
      break;
    case BluetoothChooser::Event::SHOW_ADAPTER_OFF_HELP:
      VLOG(1) << "Adapter Off Help link pressed.";
      RecordRequestDeviceOutcome(OutcomeFromChooserEvent(event));
      PostErrorCallback(blink::mojom::WebBluetoothError::CHOOSER_CANCELLED);
      break;
    case BluetoothChooser::Event::SHOW_NEED_LOCATION_HELP:
      VLOG(1) << "Need Location Help link pressed.";
      RecordRequestDeviceOutcome(OutcomeFromChooserEvent(event));
      PostErrorCallback(blink::mojom::WebBluetoothError::CHOOSER_CANCELLED);
      break;
    case BluetoothChooser::Event::SELECTED:
      PostSuccessCallback(device_address);
      break;
  }
  // Close chooser.
  chooser_.reset();
}

void BluetoothDeviceChooserController::PostSuccessCallback(
    const std::string& device_address) {
  if (!base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(success_callback_, base::Passed(std::move(options_)),
                     device_address))) {
    LOG(WARNING) << "No TaskRunner.";
  }
}

void BluetoothDeviceChooserController::PostErrorCallback(
    blink::mojom::WebBluetoothError error) {
  if (!base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(error_callback_, error))) {
    LOG(WARNING) << "No TaskRunner.";
  }
}

}  // namespace content
