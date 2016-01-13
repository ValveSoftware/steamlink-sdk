// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_monitor_mac.h"

#import <QTKit/QTKit.h>

#include <set>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/threading/thread_checker.h"
#include "content/public/browser/browser_thread.h"
#import "media/video/capture/mac/avfoundation_glue.h"

namespace {

// This class is used to keep track of system devices names and their types.
class DeviceInfo {
 public:
  enum DeviceType {
    kAudio,
    kVideo,
    kMuxed,
    kUnknown,
    kInvalid
  };

  DeviceInfo(std::string unique_id, DeviceType type)
      : unique_id_(unique_id), type_(type) {}

  // Operator== is needed here to use this class in a std::find. A given
  // |unique_id_| always has the same |type_| so for comparison purposes the
  // latter can be safely ignored.
  bool operator==(const DeviceInfo& device) const {
    return unique_id_ == device.unique_id_;
  }

  const std::string& unique_id() const { return unique_id_; }
  DeviceType type() const { return type_; }

 private:
  std::string unique_id_;
  DeviceType type_;
  // Allow generated copy constructor and assignment.
};

// Base abstract class used by DeviceMonitorMac to interact with either a QTKit
// or an AVFoundation implementation of events and notifications.
class DeviceMonitorMacImpl {
 public:
  explicit DeviceMonitorMacImpl(content::DeviceMonitorMac* monitor)
      : monitor_(monitor),
        cached_devices_(),
        device_arrival_(nil),
        device_removal_(nil) {
    DCHECK(monitor);
    // Initialise the devices_cache_ with a not-valid entry. For the case in
    // which there is one single device in the system and we get notified when
    // it gets removed, this will prevent the system from thinking that no
    // devices were added nor removed and not notifying the |monitor_|.
    cached_devices_.push_back(DeviceInfo("invalid", DeviceInfo::kInvalid));
  }
  virtual ~DeviceMonitorMacImpl() {}

  virtual void OnDeviceChanged() = 0;

  // Method called by the default notification center when a device is removed
  // or added to the system. It will compare the |cached_devices_| with the
  // current situation, update it, and, if there's an update, signal to
  // |monitor_| with the appropriate device type.
  void ConsolidateDevicesListAndNotify(
      const std::vector<DeviceInfo>& snapshot_devices);

 protected:
  content::DeviceMonitorMac* monitor_;
  std::vector<DeviceInfo> cached_devices_;

  // Handles to NSNotificationCenter block observers.
  id device_arrival_;
  id device_removal_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceMonitorMacImpl);
};

void DeviceMonitorMacImpl::ConsolidateDevicesListAndNotify(
    const std::vector<DeviceInfo>& snapshot_devices) {
  bool video_device_added = false;
  bool audio_device_added = false;
  bool video_device_removed = false;
  bool audio_device_removed = false;

  // Compare the current system devices snapshot with the ones cached to detect
  // additions, present in the former but not in the latter. If we find a device
  // in snapshot_devices entry also present in cached_devices, we remove it from
  // the latter vector.
  std::vector<DeviceInfo>::const_iterator it;
  for (it = snapshot_devices.begin(); it != snapshot_devices.end(); ++it) {
    std::vector<DeviceInfo>::iterator cached_devices_iterator =
        std::find(cached_devices_.begin(), cached_devices_.end(), *it);
    if (cached_devices_iterator == cached_devices_.end()) {
      video_device_added |= ((it->type() == DeviceInfo::kVideo) ||
                             (it->type() == DeviceInfo::kMuxed));
      audio_device_added |= ((it->type() == DeviceInfo::kAudio) ||
                             (it->type() == DeviceInfo::kMuxed));
      DVLOG(1) << "Device has been added, id: " << it->unique_id();
    } else {
      cached_devices_.erase(cached_devices_iterator);
    }
  }
  // All the remaining entries in cached_devices are removed devices.
  for (it = cached_devices_.begin(); it != cached_devices_.end(); ++it) {
    video_device_removed |= ((it->type() == DeviceInfo::kVideo) ||
                             (it->type() == DeviceInfo::kMuxed) ||
                             (it->type() == DeviceInfo::kInvalid));
    audio_device_removed |= ((it->type() == DeviceInfo::kAudio) ||
                             (it->type() == DeviceInfo::kMuxed) ||
                             (it->type() == DeviceInfo::kInvalid));
    DVLOG(1) << "Device has been removed, id: " << it->unique_id();
  }
  // Update the cached devices with the current system snapshot.
  cached_devices_ = snapshot_devices;

  if (video_device_added || video_device_removed)
    monitor_->NotifyDeviceChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
  if (audio_device_added || audio_device_removed)
    monitor_->NotifyDeviceChanged(base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE);
}

class QTKitMonitorImpl : public DeviceMonitorMacImpl {
 public:
  explicit QTKitMonitorImpl(content::DeviceMonitorMac* monitor);
  virtual ~QTKitMonitorImpl();

  virtual void OnDeviceChanged() OVERRIDE;
 private:
  void CountDevices();
  void OnAttributeChanged(NSNotification* notification);

  id device_change_;
};

QTKitMonitorImpl::QTKitMonitorImpl(content::DeviceMonitorMac* monitor)
    : DeviceMonitorMacImpl(monitor) {
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  device_arrival_ =
      [nc addObserverForName:QTCaptureDeviceWasConnectedNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                      OnDeviceChanged();}];
  device_removal_ =
      [nc addObserverForName:QTCaptureDeviceWasDisconnectedNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                      OnDeviceChanged();}];
  device_change_ =
      [nc addObserverForName:QTCaptureDeviceAttributeDidChangeNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                      OnAttributeChanged(notification);}];
}

QTKitMonitorImpl::~QTKitMonitorImpl() {
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:device_arrival_];
  [nc removeObserver:device_removal_];
  [nc removeObserver:device_change_];
}

void QTKitMonitorImpl::OnAttributeChanged(
    NSNotification* notification) {
  if ([[[notification userInfo]
         objectForKey:QTCaptureDeviceChangedAttributeKey]
      isEqualToString:QTCaptureDeviceSuspendedAttribute]) {
    OnDeviceChanged();
  }
}

void QTKitMonitorImpl::OnDeviceChanged() {
  std::vector<DeviceInfo> snapshot_devices;

  NSArray* devices = [QTCaptureDevice inputDevices];
  for (QTCaptureDevice* device in devices) {
    DeviceInfo::DeviceType device_type = DeviceInfo::kUnknown;
    // Act as if suspended video capture devices are not attached.  For
    // example, a laptop's internal webcam is suspended when the lid is closed.
    if ([device hasMediaType:QTMediaTypeVideo] &&
        ![[device attributeForKey:QTCaptureDeviceSuspendedAttribute]
        boolValue]) {
      device_type = DeviceInfo::kVideo;
    } else if ([device hasMediaType:QTMediaTypeMuxed] &&
        ![[device attributeForKey:QTCaptureDeviceSuspendedAttribute]
        boolValue]) {
      device_type = DeviceInfo::kMuxed;
    } else if ([device hasMediaType:QTMediaTypeSound] &&
        ![[device attributeForKey:QTCaptureDeviceSuspendedAttribute]
        boolValue]) {
      device_type = DeviceInfo::kAudio;
    }
    snapshot_devices.push_back(
        DeviceInfo([[device uniqueID] UTF8String], device_type));
  }
  ConsolidateDevicesListAndNotify(snapshot_devices);
}

// Forward declaration for use by CrAVFoundationDeviceObserver.
class SuspendObserverDelegate;

}  // namespace

// This class is a Key-Value Observer (KVO) shim. It is needed because C++
// classes cannot observe Key-Values directly. Created, manipulated, and
// destroyed on the Device Thread by SuspendedObserverDelegate.
@interface CrAVFoundationDeviceObserver : NSObject {
 @private
  SuspendObserverDelegate* receiver_;  // weak
  // Member to keep track of the devices we are already monitoring.
  std::set<CrAVCaptureDevice*> monitoredDevices_;
}

- (id)initWithChangeReceiver:(SuspendObserverDelegate*)receiver;
- (void)startObserving:(CrAVCaptureDevice*)device;
- (void)stopObserving:(CrAVCaptureDevice*)device;

@end

namespace {

// This class owns and manages the lifetime of a CrAVFoundationDeviceObserver.
// Provides a callback for this device observer to indicate that there has been
// a device change of some kind. Created by AVFoundationMonitorImpl in UI thread
// but living in Device Thread.
class SuspendObserverDelegate :
    public base::RefCountedThreadSafe<SuspendObserverDelegate> {
 public:
  explicit SuspendObserverDelegate(DeviceMonitorMacImpl* monitor)
      : avfoundation_monitor_impl_(monitor) {
    device_thread_checker_.DetachFromThread();
  }

  void OnDeviceChanged();
  void StartObserver();
  void ResetDeviceMonitorOnUIThread();

 private:
  friend class base::RefCountedThreadSafe<SuspendObserverDelegate>;

  virtual ~SuspendObserverDelegate() {}

  void OnDeviceChangedOnUIThread(
      const std::vector<DeviceInfo>& snapshot_devices);

  base::ThreadChecker device_thread_checker_;
  base::scoped_nsobject<CrAVFoundationDeviceObserver> suspend_observer_;
  DeviceMonitorMacImpl* avfoundation_monitor_impl_;
};

void SuspendObserverDelegate::OnDeviceChanged() {
  DCHECK(device_thread_checker_.CalledOnValidThread());
  NSArray* devices = [AVCaptureDeviceGlue devices];
  std::vector<DeviceInfo> snapshot_devices;
  for (CrAVCaptureDevice* device in devices) {
    [suspend_observer_ startObserving:device];
    BOOL suspended = [device respondsToSelector:@selector(isSuspended)] &&
        [device isSuspended];
    DeviceInfo::DeviceType device_type = DeviceInfo::kUnknown;
    if ([device hasMediaType:AVFoundationGlue::AVMediaTypeVideo()]) {
      if (suspended)
        continue;
      device_type = DeviceInfo::kVideo;
    } else if ([device hasMediaType:AVFoundationGlue::AVMediaTypeMuxed()]) {
      device_type = suspended ? DeviceInfo::kAudio : DeviceInfo::kMuxed;
    } else if ([device hasMediaType:AVFoundationGlue::AVMediaTypeAudio()]) {
      device_type = DeviceInfo::kAudio;
    }
    snapshot_devices.push_back(DeviceInfo([[device uniqueID] UTF8String],
                                          device_type));
  }
  // Post the consolidation of enumerated devices to be done on UI thread.
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&SuspendObserverDelegate::OnDeviceChangedOnUIThread,
          this, snapshot_devices));
}

void SuspendObserverDelegate::StartObserver() {
  DCHECK(device_thread_checker_.CalledOnValidThread());
  suspend_observer_.reset([[CrAVFoundationDeviceObserver alloc]
                               initWithChangeReceiver:this]);
  for (CrAVCaptureDevice* device in [AVCaptureDeviceGlue devices])
    [suspend_observer_ startObserving:device];
}

void SuspendObserverDelegate::ResetDeviceMonitorOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  avfoundation_monitor_impl_ = NULL;
}

void SuspendObserverDelegate::OnDeviceChangedOnUIThread(
    const std::vector<DeviceInfo>& snapshot_devices) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // |avfoundation_monitor_impl_| might have been NULLed asynchronously before
  // arriving at this line.
  if (avfoundation_monitor_impl_) {
    avfoundation_monitor_impl_->ConsolidateDevicesListAndNotify(
        snapshot_devices);
  }
}

// AVFoundation implementation of the Mac Device Monitor, registers as a global
// device connect/disconnect observer and plugs suspend/wake up device observers
// per device. Owns a SuspendObserverDelegate living in |device_task_runner_|
// and gets notified when a device is suspended/resumed. This class is created
// and lives in UI thread;
class AVFoundationMonitorImpl : public DeviceMonitorMacImpl {
 public:
  AVFoundationMonitorImpl(
      content::DeviceMonitorMac* monitor,
      const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner);
  virtual ~AVFoundationMonitorImpl();

  virtual void OnDeviceChanged() OVERRIDE;

 private:
  base::ThreadChecker thread_checker_;

  // {Video,AudioInput}DeviceManager's "Device" thread task runner used for
  // posting tasks to |suspend_observer_delegate_|; valid after
  // MediaStreamManager calls StartMonitoring().
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  scoped_refptr<SuspendObserverDelegate> suspend_observer_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AVFoundationMonitorImpl);
};

AVFoundationMonitorImpl::AVFoundationMonitorImpl(
    content::DeviceMonitorMac* monitor,
    const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner)
    : DeviceMonitorMacImpl(monitor),
      device_task_runner_(device_task_runner),
      suspend_observer_delegate_(new SuspendObserverDelegate(this)) {
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  device_arrival_ =
      [nc addObserverForName:AVFoundationGlue::
          AVCaptureDeviceWasConnectedNotification()
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                      OnDeviceChanged();}];
  device_removal_ =
      [nc addObserverForName:AVFoundationGlue::
          AVCaptureDeviceWasDisconnectedNotification()
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                      OnDeviceChanged();}];
  device_task_runner_->PostTask(FROM_HERE,
      base::Bind(&SuspendObserverDelegate::StartObserver,
                 suspend_observer_delegate_));
}

AVFoundationMonitorImpl::~AVFoundationMonitorImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  suspend_observer_delegate_->ResetDeviceMonitorOnUIThread();
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:device_arrival_];
  [nc removeObserver:device_removal_];
}

void AVFoundationMonitorImpl::OnDeviceChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  device_task_runner_->PostTask(FROM_HERE,
      base::Bind(&SuspendObserverDelegate::OnDeviceChanged,
                 suspend_observer_delegate_));
}

}  // namespace

@implementation CrAVFoundationDeviceObserver

- (id)initWithChangeReceiver:(SuspendObserverDelegate*)receiver {
  if ((self = [super init])) {
    DCHECK(receiver != NULL);
    receiver_ = receiver;
  }
  return self;
}

- (void)dealloc {
  std::set<CrAVCaptureDevice*>::iterator it = monitoredDevices_.begin();
  while (it != monitoredDevices_.end())
    [self stopObserving:*it++];
  [super dealloc];
}

- (void)startObserving:(CrAVCaptureDevice*)device {
  DCHECK(device != nil);
  // Skip this device if there are already observers connected to it.
  if (std::find(monitoredDevices_.begin(), monitoredDevices_.end(), device) !=
          monitoredDevices_.end()) {
    return;
  }
  [device addObserver:self
           forKeyPath:@"suspended"
              options:0
              context:device];
  [device addObserver:self
           forKeyPath:@"connected"
              options:0
              context:device];
  monitoredDevices_.insert(device);
}

- (void)stopObserving:(CrAVCaptureDevice*)device {
  DCHECK(device != nil);
  std::set<CrAVCaptureDevice*>::iterator found =
      std::find(monitoredDevices_.begin(), monitoredDevices_.end(), device);
  DCHECK(found != monitoredDevices_.end());
  // Every so seldom, |device| might be gone when getting here, in that case
  // removing the observer causes a crash. Try to avoid it by checking sanity of
  // the |device| via its -observationInfo. http://crbug.com/371271.
  if ([device observationInfo]) {
    [device removeObserver:self
                forKeyPath:@"suspended"];
    [device removeObserver:self
                forKeyPath:@"connected"];
  }
  monitoredDevices_.erase(found);
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([keyPath isEqual:@"suspended"])
    receiver_->OnDeviceChanged();
  if ([keyPath isEqual:@"connected"])
    [self stopObserving:static_cast<CrAVCaptureDevice*>(context)];
}

@end  // @implementation CrAVFoundationDeviceObserver

namespace content {

DeviceMonitorMac::DeviceMonitorMac() {
  // Both QTKit and AVFoundation do not need to be fired up until the user
  // exercises a GetUserMedia. Bringing up either library and enumerating the
  // devices in the system is an operation taking in the range of hundred of ms,
  // so it is triggered explicitly from MediaStreamManager::StartMonitoring().
}

DeviceMonitorMac::~DeviceMonitorMac() {}

void DeviceMonitorMac::StartMonitoring(
    const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (AVFoundationGlue::IsAVFoundationSupported()) {
    DVLOG(1) << "Monitoring via AVFoundation";
    device_monitor_impl_.reset(new AVFoundationMonitorImpl(this,
                                                           device_task_runner));
  } else {
    DVLOG(1) << "Monitoring via QTKit";
    device_monitor_impl_.reset(new QTKitMonitorImpl(this));
  }
}

void DeviceMonitorMac::NotifyDeviceChanged(
    base::SystemMonitor::DeviceType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(xians): Remove the global variable for SystemMonitor.
  base::SystemMonitor::Get()->ProcessDevicesChanged(type);
}

}  // namespace content
