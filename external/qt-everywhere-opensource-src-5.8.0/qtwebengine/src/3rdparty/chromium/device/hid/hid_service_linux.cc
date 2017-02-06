// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service_linux.h"

#include <fcntl.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/core/device_monitor_linux.h"
#include "device/hid/hid_connection_linux.h"
#include "device/hid/hid_device_info_linux.h"
#include "device/udev_linux/scoped_udev.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#endif  // defined(OS_CHROMEOS)

namespace device {

namespace {

const char kHidrawSubsystem[] = "hidraw";
const char kHIDID[] = "HID_ID";
const char kHIDName[] = "HID_NAME";
const char kHIDUnique[] = "HID_UNIQ";
const char kSysfsReportDescriptorKey[] = "report_descriptor";

}  // namespace

struct HidServiceLinux::ConnectParams {
  ConnectParams(scoped_refptr<HidDeviceInfoLinux> device_info,
                const ConnectCallback& callback,
                scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
      : device_info(device_info),
        callback(callback),
        task_runner(task_runner),
        file_task_runner(file_task_runner) {}
  ~ConnectParams() {}

  scoped_refptr<HidDeviceInfoLinux> device_info;
  ConnectCallback callback;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner;
  base::File device_file;
};

class HidServiceLinux::FileThreadHelper : public DeviceMonitorLinux::Observer {
 public:
  FileThreadHelper(base::WeakPtr<HidServiceLinux> service,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : observer_(this), service_(service), task_runner_(task_runner) {}

  ~FileThreadHelper() override {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  static void Start(std::unique_ptr<FileThreadHelper> self) {
    base::ThreadRestrictions::AssertIOAllowed();
    self->thread_checker_.DetachFromThread();

    DeviceMonitorLinux* monitor = DeviceMonitorLinux::GetInstance();
    self->observer_.Add(monitor);
    monitor->Enumerate(base::Bind(&FileThreadHelper::OnDeviceAdded,
                                  base::Unretained(self.get())));
    self->task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&HidServiceLinux::FirstEnumerationComplete, self->service_));

    // |self| is now owned by the current message loop.
    ignore_result(self.release());
  }

 private:
  // DeviceMonitorLinux::Observer:
  void OnDeviceAdded(udev_device* device) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    const char* device_path = udev_device_get_syspath(device);
    if (!device_path) {
      return;
    }
    HidDeviceId device_id = device_path;

    const char* subsystem = udev_device_get_subsystem(device);
    if (!subsystem || strcmp(subsystem, kHidrawSubsystem) != 0) {
      return;
    }

    const char* str_property = udev_device_get_devnode(device);
    if (!str_property) {
      return;
    }
    std::string device_node = str_property;

    udev_device* parent = udev_device_get_parent(device);
    if (!parent) {
      return;
    }

    const char* hid_id = udev_device_get_property_value(parent, kHIDID);
    if (!hid_id) {
      return;
    }

    std::vector<std::string> parts = base::SplitString(
        hid_id, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (parts.size() != 3) {
      return;
    }

    uint32_t int_property = 0;
    if (!HexStringToUInt(base::StringPiece(parts[1]), &int_property) ||
        int_property > std::numeric_limits<uint16_t>::max()) {
      return;
    }
    uint16_t vendor_id = int_property;

    if (!HexStringToUInt(base::StringPiece(parts[2]), &int_property) ||
        int_property > std::numeric_limits<uint16_t>::max()) {
      return;
    }
    uint16_t product_id = int_property;

    std::string serial_number;
    str_property = udev_device_get_property_value(parent, kHIDUnique);
    if (str_property != NULL) {
      serial_number = str_property;
    }

    std::string product_name;
    str_property = udev_device_get_property_value(parent, kHIDName);
    if (str_property != NULL) {
      product_name = str_property;
    }

    const char* parent_sysfs_path = udev_device_get_syspath(parent);
    if (!parent_sysfs_path) {
      return;
    }
    base::FilePath report_descriptor_path =
        base::FilePath(parent_sysfs_path).Append(kSysfsReportDescriptorKey);
    std::string report_descriptor_str;
    if (!base::ReadFileToString(report_descriptor_path,
                                &report_descriptor_str)) {
      return;
    }

    scoped_refptr<HidDeviceInfo> device_info(new HidDeviceInfoLinux(
        device_id, device_node, vendor_id, product_id, product_name,
        serial_number,
        kHIDBusTypeUSB,  // TODO(reillyg): Detect Bluetooth. crbug.com/443335
        std::vector<uint8_t>(report_descriptor_str.begin(),
                             report_descriptor_str.end())));

    task_runner_->PostTask(FROM_HERE, base::Bind(&HidServiceLinux::AddDevice,
                                                 service_, device_info));
  }

  void OnDeviceRemoved(udev_device* device) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    const char* device_path = udev_device_get_syspath(device);
    if (device_path) {
      task_runner_->PostTask(
          FROM_HERE, base::Bind(&HidServiceLinux::RemoveDevice, service_,
                                std::string(device_path)));
    }
  }

  void WillDestroyMonitorMessageLoop() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    delete this;
  }

  base::ThreadChecker thread_checker_;
  ScopedObserver<DeviceMonitorLinux, DeviceMonitorLinux::Observer> observer_;

  // This weak pointer is only valid when checked on this task runner.
  base::WeakPtr<HidServiceLinux> service_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(FileThreadHelper);
};

HidServiceLinux::HidServiceLinux(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : file_task_runner_(file_task_runner), weak_factory_(this) {
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  std::unique_ptr<FileThreadHelper> helper(
      new FileThreadHelper(weak_factory_.GetWeakPtr(), task_runner_));
  helper_ = helper.get();
  file_task_runner_->PostTask(
      FROM_HERE, base::Bind(&FileThreadHelper::Start, base::Passed(&helper)));
}

HidServiceLinux::~HidServiceLinux() {
  file_task_runner_->DeleteSoon(FROM_HERE, helper_);
}

void HidServiceLinux::Connect(const HidDeviceId& device_id,
                              const ConnectCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto& map_entry = devices().find(device_id);
  if (map_entry == devices().end()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
    return;
  }
  scoped_refptr<HidDeviceInfoLinux> device_info =
      static_cast<HidDeviceInfoLinux*>(map_entry->second.get());

  std::unique_ptr<ConnectParams> params(new ConnectParams(
      device_info, callback, task_runner_, file_task_runner_));

#if defined(OS_CHROMEOS)
  chromeos::PermissionBrokerClient* client =
      chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
  DCHECK(client) << "Could not get permission broker client.";
  chromeos::PermissionBrokerClient::ErrorCallback error_callback =
      base::Bind(&HidServiceLinux::OnPathOpenError,
                 params->device_info->device_node(), params->callback);
  client->OpenPath(
      device_info->device_node(),
      base::Bind(&HidServiceLinux::OnPathOpenComplete, base::Passed(&params)),
      error_callback);
#else
  file_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&HidServiceLinux::OpenOnBlockingThread,
                                         base::Passed(&params)));
#endif  // defined(OS_CHROMEOS)
}

#if defined(OS_CHROMEOS)

// static
void HidServiceLinux::OnPathOpenComplete(std::unique_ptr<ConnectParams> params,
                                         dbus::FileDescriptor fd) {
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner =
      params->file_task_runner;
  file_task_runner->PostTask(
      FROM_HERE, base::Bind(&HidServiceLinux::ValidateFdOnBlockingThread,
                            base::Passed(&params), base::Passed(&fd)));
}

// static
void HidServiceLinux::OnPathOpenError(const std::string& device_path,
                                      const ConnectCallback& callback,
                                      const std::string& error_name,
                                      const std::string& error_message) {
  HID_LOG(EVENT) << "Permission broker failed to open '" << device_path
                 << "': " << error_name << ": " << error_message;
  callback.Run(nullptr);
}

// static
void HidServiceLinux::ValidateFdOnBlockingThread(
    std::unique_ptr<ConnectParams> params,
    dbus::FileDescriptor fd) {
  base::ThreadRestrictions::AssertIOAllowed();
  fd.CheckValidity();
  DCHECK(fd.is_valid());
  params->device_file = base::File(fd.TakeValue());
  FinishOpen(std::move(params));
}

#else

// static
void HidServiceLinux::OpenOnBlockingThread(
    std::unique_ptr<ConnectParams> params) {
  base::ThreadRestrictions::AssertIOAllowed();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner = params->task_runner;

  base::FilePath device_path(params->device_info->device_node());
  base::File& device_file = params->device_file;
  int flags =
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE;
  device_file.Initialize(device_path, flags);
  if (!device_file.IsValid()) {
    base::File::Error file_error = device_file.error_details();

    if (file_error == base::File::FILE_ERROR_ACCESS_DENIED) {
      HID_LOG(EVENT)
          << "Access denied opening device read-write, trying read-only.";
      flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
      device_file.Initialize(device_path, flags);
    }
  }
  if (!device_file.IsValid()) {
    HID_LOG(EVENT) << "Failed to open '" << params->device_info->device_node()
                   << "': "
                   << base::File::ErrorToString(device_file.error_details());
    task_runner->PostTask(FROM_HERE, base::Bind(params->callback, nullptr));
    return;
  }

  FinishOpen(std::move(params));
}

#endif  // defined(OS_CHROMEOS)

// static
void HidServiceLinux::FinishOpen(std::unique_ptr<ConnectParams> params) {
  base::ThreadRestrictions::AssertIOAllowed();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner = params->task_runner;

  if (!base::SetNonBlocking(params->device_file.GetPlatformFile())) {
    HID_PLOG(ERROR) << "Failed to set the non-blocking flag on the device fd";
    task_runner->PostTask(FROM_HERE, base::Bind(params->callback, nullptr));
    return;
  }

  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&HidServiceLinux::CreateConnection, base::Passed(&params)));
}

// static
void HidServiceLinux::CreateConnection(std::unique_ptr<ConnectParams> params) {
  DCHECK(params->device_file.IsValid());
  params->callback.Run(make_scoped_refptr(new HidConnectionLinux(
      params->device_info, std::move(params->device_file),
      params->file_task_runner)));
}

}  // namespace device
