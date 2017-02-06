// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/battery/battery_status_manager_linux.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "dbus/values_util.h"
#include "device/battery/battery_status_manager.h"

namespace device {

namespace {

const char kUPowerServiceName[] = "org.freedesktop.UPower";
const char kUPowerDeviceName[] = "org.freedesktop.UPower.Device";
const char kUPowerPath[] = "/org/freedesktop/UPower";
const char kUPowerDeviceSignalChanged[] = "Changed";
const char kUPowerEnumerateDevices[] = "EnumerateDevices";
const char kBatteryNotifierThreadName[] = "BatteryStatusNotifier";

// UPowerDeviceType reflects the possible UPower.Device.Type values,
// see upower.freedesktop.org/docs/Device.html#Device:Type.
enum UPowerDeviceType {
  UPOWER_DEVICE_TYPE_UNKNOWN = 0,
  UPOWER_DEVICE_TYPE_LINE_POWER = 1,
  UPOWER_DEVICE_TYPE_BATTERY = 2,
  UPOWER_DEVICE_TYPE_UPS = 3,
  UPOWER_DEVICE_TYPE_MONITOR = 4,
  UPOWER_DEVICE_TYPE_MOUSE = 5,
  UPOWER_DEVICE_TYPE_KEYBOARD = 6,
  UPOWER_DEVICE_TYPE_PDA = 7,
  UPOWER_DEVICE_TYPE_PHONE = 8,
};

typedef std::vector<dbus::ObjectPath> PathsVector;

double GetPropertyAsDouble(const base::DictionaryValue& dictionary,
                           const std::string& property_name,
                           double default_value) {
  double value = default_value;
  return dictionary.GetDouble(property_name, &value) ? value : default_value;
}

bool GetPropertyAsBoolean(const base::DictionaryValue& dictionary,
                          const std::string& property_name,
                          bool default_value) {
  bool value = default_value;
  return dictionary.GetBoolean(property_name, &value) ? value : default_value;
}

std::unique_ptr<base::DictionaryValue> GetPropertiesAsDictionary(
    dbus::ObjectProxy* proxy) {
  dbus::MethodCall method_call(dbus::kPropertiesInterface,
                               dbus::kPropertiesGetAll);
  dbus::MessageWriter builder(&method_call);
  builder.AppendString(kUPowerDeviceName);

  std::unique_ptr<dbus::Response> response(proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (response) {
    dbus::MessageReader reader(response.get());
    std::unique_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
    base::DictionaryValue* dictionary_value = NULL;
    if (value && value->GetAsDictionary(&dictionary_value)) {
      ignore_result(value.release());
      return std::unique_ptr<base::DictionaryValue>(dictionary_value);
    }
  }
  return std::unique_ptr<base::DictionaryValue>();
}

std::unique_ptr<PathsVector> GetPowerSourcesPaths(dbus::ObjectProxy* proxy) {
  std::unique_ptr<PathsVector> paths(new PathsVector());
  if (!proxy)
    return paths;

  dbus::MethodCall method_call(kUPowerServiceName, kUPowerEnumerateDevices);
  std::unique_ptr<dbus::Response> response(proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));

  if (response) {
    dbus::MessageReader reader(response.get());
    reader.PopArrayOfObjectPaths(paths.get());
  }
  return paths;
}

void UpdateNumberBatteriesHistogram(int count) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "BatteryStatus.NumberBatteriesLinux", count, 1, 5, 6);
}

// Class that represents a dedicated thread which communicates with DBus to
// obtain battery information and receives battery change notifications.
class BatteryStatusNotificationThread : public base::Thread {
 public:
  BatteryStatusNotificationThread(
      const BatteryStatusService::BatteryUpdateCallback& callback)
      : base::Thread(kBatteryNotifierThreadName),
        callback_(callback),
        battery_proxy_(NULL) {}

  ~BatteryStatusNotificationThread() override {
    // Make sure to shutdown the dbus connection if it is still open in the very
    // end. It needs to happen on the BatteryStatusNotificationThread.
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&BatteryStatusNotificationThread::ShutdownDBusConnection,
                   base::Unretained(this)));

    // Drain the message queue of the BatteryStatusNotificationThread and stop.
    Stop();
  }

  void StartListening() {
    DCHECK(OnWatcherThread());

    if (system_bus_.get())
      return;

    InitDBus();
    dbus::ObjectProxy* power_proxy =
        system_bus_->GetObjectProxy(kUPowerServiceName,
                                    dbus::ObjectPath(kUPowerPath));
    std::unique_ptr<PathsVector> device_paths =
        GetPowerSourcesPaths(power_proxy);
    int num_batteries = 0;

    for (size_t i = 0; i < device_paths->size(); ++i) {
      const dbus::ObjectPath& device_path = device_paths->at(i);
      dbus::ObjectProxy* device_proxy = system_bus_->GetObjectProxy(
          kUPowerServiceName, device_path);
      std::unique_ptr<base::DictionaryValue> dictionary =
          GetPropertiesAsDictionary(device_proxy);

      if (!dictionary)
        continue;

      bool is_present = GetPropertyAsBoolean(*dictionary, "IsPresent", false);
      uint32_t type = static_cast<uint32_t>(
          GetPropertyAsDouble(*dictionary, "Type", UPOWER_DEVICE_TYPE_UNKNOWN));

      if (!is_present || type != UPOWER_DEVICE_TYPE_BATTERY) {
        system_bus_->RemoveObjectProxy(kUPowerServiceName,
                                       device_path,
                                       base::Bind(&base::DoNothing));
        continue;
      }

      if (battery_proxy_) {
        // TODO(timvolodine): add support for multiple batteries. Currently we
        // only collect information from the first battery we encounter
        // (crbug.com/400780).
        LOG(WARNING) << "multiple batteries found, "
                     << "using status data of the first battery only.";
      } else {
        battery_proxy_ = device_proxy;
      }
      num_batteries++;
    }

    UpdateNumberBatteriesHistogram(num_batteries);

    if (!battery_proxy_) {
      callback_.Run(BatteryStatus());
      return;
    }

    battery_proxy_->ConnectToSignal(
        kUPowerDeviceName,
        kUPowerDeviceSignalChanged,
        base::Bind(&BatteryStatusNotificationThread::BatteryChanged,
                   base::Unretained(this)),
        base::Bind(&BatteryStatusNotificationThread::OnSignalConnected,
                   base::Unretained(this)));
  }

  void StopListening() {
    DCHECK(OnWatcherThread());
    ShutdownDBusConnection();
  }

 private:
  bool OnWatcherThread() {
    return task_runner()->BelongsToCurrentThread();
  }

  void InitDBus() {
    DCHECK(OnWatcherThread());

    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    options.connection_type = dbus::Bus::PRIVATE;
    system_bus_ = new dbus::Bus(options);
  }

  void ShutdownDBusConnection() {
    DCHECK(OnWatcherThread());

    if (!system_bus_.get())
      return;

    // Shutdown DBus connection later because there may be pending tasks on
    // this thread.
    message_loop()->PostTask(FROM_HERE,
                             base::Bind(&dbus::Bus::ShutdownAndBlock,
                                        system_bus_));
    system_bus_ = NULL;
    battery_proxy_ = NULL;
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success) {
    DCHECK(OnWatcherThread());

    if (interface_name != kUPowerDeviceName ||
        signal_name != kUPowerDeviceSignalChanged) {
      return;
    }

    if (!system_bus_.get())
      return;

    if (success) {
      BatteryChanged(NULL);
    } else {
      // Failed to register for "Changed" signal, execute callback with the
      // default values.
      callback_.Run(BatteryStatus());
    }
  }

  void BatteryChanged(dbus::Signal* signal /* unsused */) {
    DCHECK(OnWatcherThread());

    if (!system_bus_.get())
      return;

    std::unique_ptr<base::DictionaryValue> dictionary =
        GetPropertiesAsDictionary(battery_proxy_);
    if (dictionary)
      callback_.Run(ComputeWebBatteryStatus(*dictionary));
    else
      callback_.Run(BatteryStatus());
  }

  BatteryStatusService::BatteryUpdateCallback callback_;
  scoped_refptr<dbus::Bus> system_bus_;
  dbus::ObjectProxy* battery_proxy_;  // owned by the bus

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusNotificationThread);
};

// Creates a notification thread and delegates Start/Stop calls to it.
class BatteryStatusManagerLinux : public BatteryStatusManager {
 public:
  explicit BatteryStatusManagerLinux(
      const BatteryStatusService::BatteryUpdateCallback& callback)
      : callback_(callback) {}

  ~BatteryStatusManagerLinux() override {}

 private:
  // BatteryStatusManager:
  bool StartListeningBatteryChange() override {
    if (!StartNotifierThreadIfNecessary())
      return false;

    notifier_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&BatteryStatusNotificationThread::StartListening,
                   base::Unretained(notifier_thread_.get())));
    return true;
  }

  void StopListeningBatteryChange() override {
    if (!notifier_thread_)
      return;

    notifier_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&BatteryStatusNotificationThread::StopListening,
                   base::Unretained(notifier_thread_.get())));
  }

  // Starts the notifier thread if not already started and returns true on
  // success.
  bool StartNotifierThreadIfNecessary() {
    if (notifier_thread_)
      return true;

    base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
    notifier_thread_.reset(new BatteryStatusNotificationThread(callback_));
    if (!notifier_thread_->StartWithOptions(thread_options)) {
      notifier_thread_.reset();
      LOG(ERROR) << "Could not start the " << kBatteryNotifierThreadName
                 << " thread";
      return false;
    }
    return true;
  }

  BatteryStatusService::BatteryUpdateCallback callback_;
  std::unique_ptr<BatteryStatusNotificationThread> notifier_thread_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusManagerLinux);
};

}  // namespace

BatteryStatus ComputeWebBatteryStatus(const base::DictionaryValue& dictionary) {
  BatteryStatus status;
  if (!dictionary.HasKey("State"))
    return status;

  uint32_t state = static_cast<uint32_t>(
      GetPropertyAsDouble(dictionary, "State", UPOWER_DEVICE_STATE_UNKNOWN));
  status.charging = state != UPOWER_DEVICE_STATE_DISCHARGING &&
                    state != UPOWER_DEVICE_STATE_EMPTY;
  double percentage = GetPropertyAsDouble(dictionary, "Percentage", 100);
  // Convert percentage to a value between 0 and 1 with 2 digits of precision.
  // This is to bring it in line with other platforms like Mac and Android where
  // we report level with 1% granularity. It also serves the purpose of reducing
  // the possibility of fingerprinting and triggers less level change events on
  // the blink side.
  // TODO(timvolodine): consider moving this rounding to the blink side.
  status.level = round(percentage) / 100.f;

  switch (state) {
    case UPOWER_DEVICE_STATE_CHARGING : {
      double time_to_full = GetPropertyAsDouble(dictionary, "TimeToFull", 0);
      status.charging_time =
          (time_to_full > 0) ? time_to_full
                             : std::numeric_limits<double>::infinity();
      break;
    }
    case UPOWER_DEVICE_STATE_DISCHARGING : {
      double time_to_empty = GetPropertyAsDouble(dictionary, "TimeToEmpty", 0);
      // Set dischargingTime if it's available. Otherwise leave the default
      // value which is +infinity.
      if (time_to_empty > 0)
        status.discharging_time = time_to_empty;
      status.charging_time = std::numeric_limits<double>::infinity();
      break;
    }
    case UPOWER_DEVICE_STATE_FULL : {
      break;
    }
    default: {
      status.charging_time = std::numeric_limits<double>::infinity();
    }
  }
  return status;
}

// static
std::unique_ptr<BatteryStatusManager> BatteryStatusManager::Create(
    const BatteryStatusService::BatteryUpdateCallback& callback) {
  return std::unique_ptr<BatteryStatusManager>(
      new BatteryStatusManagerLinux(callback));
}

}  // namespace device
