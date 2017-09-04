// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/system_info_handler.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_switches.h"

namespace content {
namespace devtools {
namespace system_info {

namespace {

using Response = DevToolsProtocolClient::Response;

// Give the GPU process a few seconds to provide GPU info.
const int kGPUInfoWatchdogTimeoutMs = 5000;

class AuxGPUInfoEnumerator : public gpu::GPUInfo::Enumerator {
 public:
  AuxGPUInfoEnumerator(base::DictionaryValue* dictionary)
      : dictionary_(dictionary),
        in_aux_attributes_(false) { }

  void AddInt64(const char* name, int64_t value) override {
    if (in_aux_attributes_)
      dictionary_->SetDouble(name, value);
  }

  void AddInt(const char* name, int value) override {
    if (in_aux_attributes_)
      dictionary_->SetInteger(name, value);
  }

  void AddString(const char* name, const std::string& value) override {
    if (in_aux_attributes_)
      dictionary_->SetString(name, value);
  }

  void AddBool(const char* name, bool value) override {
    if (in_aux_attributes_)
      dictionary_->SetBoolean(name, value);
  }

  void AddTimeDeltaInSecondsF(const char* name,
                              const base::TimeDelta& value) override {
    if (in_aux_attributes_)
      dictionary_->SetDouble(name, value.InSecondsF());
  }

  void BeginGPUDevice() override {}

  void EndGPUDevice() override {}

  void BeginVideoDecodeAcceleratorSupportedProfile() override {}

  void EndVideoDecodeAcceleratorSupportedProfile() override {}

  void BeginVideoEncodeAcceleratorSupportedProfile() override {}

  void EndVideoEncodeAcceleratorSupportedProfile() override {}

  void BeginAuxAttributes() override {
    in_aux_attributes_ = true;
  }

  void EndAuxAttributes() override {
    in_aux_attributes_ = false;
  }

 private:
  base::DictionaryValue* dictionary_;
  bool in_aux_attributes_;
};

scoped_refptr<GPUDevice> GPUDeviceToProtocol(
    const gpu::GPUInfo::GPUDevice& device) {
  return GPUDevice::Create()->set_vendor_id(device.vendor_id)
                            ->set_device_id(device.device_id)
                            ->set_vendor_string(device.vendor_string)
                            ->set_device_string(device.device_string);
}

}  // namespace

class SystemInfoHandlerGpuObserver : public content::GpuDataManagerObserver {
 public:
  SystemInfoHandlerGpuObserver(base::WeakPtr<SystemInfoHandler> handler,
                               DevToolsCommandId command_id)
      : handler_(handler),
        command_id_(command_id),
        observer_id_(++next_observer_id_)
  {
    if (handler_) {
      handler_->AddActiveObserverId(observer_id_);
    }
  }

  void OnGpuInfoUpdate() override {
    UnregisterAndSendResponse();
  }

  void OnGpuProcessCrashed(base::TerminationStatus exit_code) override {
    UnregisterAndSendResponse();
  }

  void UnregisterAndSendResponse() {
    GpuDataManager::GetInstance()->RemoveObserver(this);
    if (handler_.get()) {
      if (handler_->RemoveActiveObserverId(observer_id_)) {
        handler_->SendGetInfoResponse(command_id_);
      }
    }
    delete this;
  }

  int GetObserverId() {
    return observer_id_;
  }

 private:
  base::WeakPtr<SystemInfoHandler> handler_;
  DevToolsCommandId command_id_;
  int observer_id_;

  static int next_observer_id_;
};

int SystemInfoHandlerGpuObserver::next_observer_id_ = 0;

SystemInfoHandler::SystemInfoHandler()
    : weak_factory_(this) {
}

SystemInfoHandler::~SystemInfoHandler() {
}

void SystemInfoHandler::SetClient(std::unique_ptr<Client> client) {
  client_.swap(client);
}

Response SystemInfoHandler::GetInfo(DevToolsCommandId command_id) {
  std::string reason;
  if (!GpuDataManager::GetInstance()->GpuAccessAllowed(&reason) ||
      GpuDataManager::GetInstance()->IsEssentialGpuInfoAvailable() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGpuTestingNoCompleteInfoCollection)) {
    // The GpuDataManager already has all of the information needed to make
    // GPU-based blacklisting decisions. Post a task to give it to the
    // client asynchronously.
    //
    // Waiting for complete GPU info in the if-test above seems to
    // frequently hit internal timeouts in the launching of the unsandboxed
    // GPU process in debug builds on Windows.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SystemInfoHandler::SendGetInfoResponse,
                   weak_factory_.GetWeakPtr(),
                   command_id));
  } else {
    // We will be able to get more information from the GpuDataManager.
    // Register a transient observer with it to call us back when the
    // information is available.
    SystemInfoHandlerGpuObserver* observer = new SystemInfoHandlerGpuObserver(
        weak_factory_.GetWeakPtr(), command_id);
    BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SystemInfoHandler::ObserverWatchdogCallback,
                   weak_factory_.GetWeakPtr(),
                   observer->GetObserverId(),
                   command_id),
        base::TimeDelta::FromMilliseconds(kGPUInfoWatchdogTimeoutMs));
    GpuDataManager::GetInstance()->AddObserver(observer);
    // There's no other method available to request just essential GPU info.
    GpuDataManager::GetInstance()->RequestCompleteGpuInfoIfNeeded();
  }

  return Response::OK();
}

void SystemInfoHandler::SendGetInfoResponse(DevToolsCommandId command_id) {
  gpu::GPUInfo gpu_info = GpuDataManager::GetInstance()->GetGPUInfo();
  std::vector<scoped_refptr<GPUDevice>> devices;
  devices.push_back(GPUDeviceToProtocol(gpu_info.gpu));
  for (const auto& device : gpu_info.secondary_gpus)
    devices.push_back(GPUDeviceToProtocol(device));

  std::unique_ptr<base::DictionaryValue> aux_attributes(
      new base::DictionaryValue);
  AuxGPUInfoEnumerator enumerator(aux_attributes.get());
  gpu_info.EnumerateFields(&enumerator);

  scoped_refptr<GPUInfo> gpu =
      GPUInfo::Create()
          ->set_devices(devices)
          ->set_aux_attributes(std::move(aux_attributes))
          ->set_feature_status(base::WrapUnique(GetFeatureStatus()))
          ->set_driver_bug_workarounds(GetDriverBugWorkarounds());

  client_->SendGetInfoResponse(
      command_id,
      GetInfoResponse::Create()->set_gpu(gpu)
      ->set_model_name(gpu_info.machine_model_name)
      ->set_model_version(gpu_info.machine_model_version));
}

void SystemInfoHandler::AddActiveObserverId(int observer_id) {
  base::AutoLock auto_lock(lock_);
  active_observers_.insert(observer_id);
}

bool SystemInfoHandler::RemoveActiveObserverId(int observer_id) {
  base::AutoLock auto_lock(lock_);
  int num_removed = active_observers_.erase(observer_id);
  return (num_removed != 0);
}

void SystemInfoHandler::ObserverWatchdogCallback(int observer_id,
                                                 DevToolsCommandId command_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (RemoveActiveObserverId(observer_id)) {
    SendGetInfoResponse(command_id);
    // For the time being we want to know about this event in the test logs.
    LOG(ERROR) << "SystemInfoHandler: request for GPU info timed out!"
               << " Most recent info sent.";
  }
}

}  // namespace system_info
}  // namespace devtools
}  // namespace content
