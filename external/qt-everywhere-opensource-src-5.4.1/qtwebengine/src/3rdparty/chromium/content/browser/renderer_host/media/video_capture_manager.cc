// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_manager.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/media_stream_request.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/scoped_histogram_timer.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_device_factory.h"

#if defined(ENABLE_SCREEN_CAPTURE)
#include "content/browser/media/capture/desktop_capture_device.h"
#if defined(USE_AURA)
#include "content/browser/media/capture/desktop_capture_device_aura.h"
#endif
#endif

namespace {

// Compares two VideoCaptureFormat by checking smallest frame_size area, then
// by _largest_ frame_rate. Used to order a VideoCaptureFormats vector so that
// the first entry for a given resolution has the largest frame rate, as needed
// by the ConsolidateCaptureFormats() method.
bool IsCaptureFormatSmaller(const media::VideoCaptureFormat& format1,
                            const media::VideoCaptureFormat& format2) {
  if (format1.frame_size.GetArea() == format2.frame_size.GetArea())
    return format1.frame_rate > format2.frame_rate;
  return format1.frame_size.GetArea() < format2.frame_size.GetArea();
}

bool IsCaptureFormatSizeEqual(const media::VideoCaptureFormat& format1,
                              const media::VideoCaptureFormat& format2) {
  return format1.frame_size.GetArea() == format2.frame_size.GetArea();
}

// This function receives a list of capture formats, removes duplicated
// resolutions while keeping the highest frame rate for each, and forcing I420
// pixel format.
void ConsolidateCaptureFormats(media::VideoCaptureFormats* formats) {
  if (formats->empty())
    return;
  std::sort(formats->begin(), formats->end(), IsCaptureFormatSmaller);
  // Due to the ordering imposed, the largest frame_rate is kept while removing
  // duplicated resolutions.
  media::VideoCaptureFormats::iterator last =
      std::unique(formats->begin(), formats->end(), IsCaptureFormatSizeEqual);
  formats->erase(last, formats->end());
  // Mark all formats as I420, since this is what the renderer side will get
  // anyhow: the actual pixel format is decided at the device level.
  for (media::VideoCaptureFormats::iterator it = formats->begin();
       it != formats->end(); ++it) {
    it->pixel_format = media::PIXEL_FORMAT_I420;
  }
}

}  // namespace

namespace content {

VideoCaptureManager::DeviceEntry::DeviceEntry(
    MediaStreamType stream_type,
    const std::string& id,
    scoped_ptr<VideoCaptureController> controller)
    : stream_type(stream_type),
      id(id),
      video_capture_controller(controller.Pass()) {}

VideoCaptureManager::DeviceEntry::~DeviceEntry() {}

VideoCaptureManager::DeviceInfo::DeviceInfo() {}

VideoCaptureManager::DeviceInfo::DeviceInfo(
    const media::VideoCaptureDevice::Name& name,
    const media::VideoCaptureFormats& supported_formats)
    : name(name),
      supported_formats(supported_formats) {}

VideoCaptureManager::DeviceInfo::~DeviceInfo() {}

VideoCaptureManager::VideoCaptureManager(
    scoped_ptr<media::VideoCaptureDeviceFactory> factory)
    : listener_(NULL),
      new_capture_session_id_(1),
      video_capture_device_factory_(factory.Pass()) {
}

VideoCaptureManager::~VideoCaptureManager() {
  DCHECK(devices_.empty());
}

void VideoCaptureManager::Register(
    MediaStreamProviderListener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!listener_);
  DCHECK(!device_task_runner_.get());
  listener_ = listener;
  device_task_runner_ = device_task_runner;
}

void VideoCaptureManager::Unregister() {
  DCHECK(listener_);
  listener_ = NULL;
}

void VideoCaptureManager::EnumerateDevices(MediaStreamType stream_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureManager::EnumerateDevices, type " << stream_type;
  DCHECK(listener_);
  DCHECK_EQ(stream_type, MEDIA_DEVICE_VIDEO_CAPTURE);

  // Bind a callback to ConsolidateDevicesInfoOnDeviceThread() with an argument
  // for another callback to OnDevicesInfoEnumerated() to be run in the current
  // loop, i.e. IO loop. Pass a timer for UMA histogram collection.
  base::Callback<void(scoped_ptr<media::VideoCaptureDevice::Names>)>
      devices_enumerated_callback =
          base::Bind(&VideoCaptureManager::ConsolidateDevicesInfoOnDeviceThread,
                     this,
                     media::BindToCurrentLoop(base::Bind(
                         &VideoCaptureManager::OnDevicesInfoEnumerated,
                         this,
                         stream_type,
                         base::Owned(new base::ElapsedTimer()))),
                     stream_type,
                     devices_info_cache_);
  // OK to use base::Unretained() since we own the VCDFactory and |this| is
  // bound in |devices_enumerated_callback|.
  device_task_runner_->PostTask(FROM_HERE,
      base::Bind(&media::VideoCaptureDeviceFactory::EnumerateDeviceNames,
                 base::Unretained(video_capture_device_factory_.get()),
                 devices_enumerated_callback));
}

int VideoCaptureManager::Open(const StreamDeviceInfo& device_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listener_);

  // Generate a new id for the session being opened.
  const media::VideoCaptureSessionId capture_session_id =
      new_capture_session_id_++;

  DCHECK(sessions_.find(capture_session_id) == sessions_.end());
  DVLOG(1) << "VideoCaptureManager::Open, id " << capture_session_id;

  // We just save the stream info for processing later.
  sessions_[capture_session_id] = device_info.device;

  // Notify our listener asynchronously; this ensures that we return
  // |capture_session_id| to the caller of this function before using that same
  // id in a listener event.
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureManager::OnOpened, this,
                 device_info.device.type, capture_session_id));
  return capture_session_id;
}

void VideoCaptureManager::Close(int capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listener_);
  DVLOG(1) << "VideoCaptureManager::Close, id " << capture_session_id;

  SessionMap::iterator session_it = sessions_.find(capture_session_id);
  if (session_it == sessions_.end()) {
    NOTREACHED();
    return;
  }

  DeviceEntry* const existing_device = GetDeviceEntryForMediaStreamDevice(
      session_it->second);
  if (existing_device) {
    // Remove any client that is still using the session. This is safe to call
    // even if there are no clients using the session.
    existing_device->video_capture_controller->StopSession(capture_session_id);

    // StopSession() may have removed the last client, so we might need to
    // close the device.
    DestroyDeviceEntryIfNoClients(existing_device);
  }

  // Notify listeners asynchronously, and forget the session.
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureManager::OnClosed, this, session_it->second.type,
                 capture_session_id));
  sessions_.erase(session_it);
}

void VideoCaptureManager::DoStartDeviceOnDeviceThread(
    media::VideoCaptureSessionId session_id,
    DeviceEntry* entry,
    const media::VideoCaptureParams& params,
    scoped_ptr<media::VideoCaptureDevice::Client> device_client) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(IsOnDeviceThread());

  scoped_ptr<media::VideoCaptureDevice> video_capture_device;
  switch (entry->stream_type) {
    case MEDIA_DEVICE_VIDEO_CAPTURE: {
      // We look up the device id from the renderer in our local enumeration
      // since the renderer does not have all the information that might be
      // held in the browser-side VideoCaptureDevice::Name structure.
      DeviceInfo* found = FindDeviceInfoById(entry->id, devices_info_cache_);
      if (found) {
        video_capture_device =
            video_capture_device_factory_->Create(found->name);
      }
      break;
    }
    case MEDIA_TAB_VIDEO_CAPTURE: {
      video_capture_device.reset(
          WebContentsVideoCaptureDevice::Create(entry->id));
      break;
    }
    case MEDIA_DESKTOP_VIDEO_CAPTURE: {
#if defined(ENABLE_SCREEN_CAPTURE)
      DesktopMediaID id = DesktopMediaID::Parse(entry->id);
#if defined(USE_AURA)
      if (id.type == DesktopMediaID::TYPE_AURA_WINDOW) {
        video_capture_device.reset(DesktopCaptureDeviceAura::Create(id));
      } else
#endif
      if (id.type != DesktopMediaID::TYPE_NONE &&
          id.type != DesktopMediaID::TYPE_AURA_WINDOW) {
        video_capture_device = DesktopCaptureDevice::Create(id);
        if (notification_window_ids_.find(session_id) !=
            notification_window_ids_.end()) {
          static_cast<DesktopCaptureDevice*>(video_capture_device.get())
              ->SetNotificationWindowId(notification_window_ids_[session_id]);
        }
      }
#endif  // defined(ENABLE_SCREEN_CAPTURE)
      break;
    }
    default: {
      NOTIMPLEMENTED();
      break;
    }
  }

  if (!video_capture_device) {
    device_client->OnError("Could not create capture device");
    return;
  }

  video_capture_device->AllocateAndStart(params, device_client.Pass());
  entry->video_capture_device = video_capture_device.Pass();
}

void VideoCaptureManager::StartCaptureForClient(
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params,
    base::ProcessHandle client_render_process,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler,
    const DoneCB& done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureManager::StartCaptureForClient, "
           << params.requested_format.frame_size.ToString() << ", "
           << params.requested_format.frame_rate << ", #" << session_id << ")";

  DeviceEntry* entry = GetOrCreateDeviceEntry(session_id);
  if (!entry) {
    done_cb.Run(base::WeakPtr<VideoCaptureController>());
    return;
  }

  DCHECK(entry->video_capture_controller);

  // First client starts the device.
  if (entry->video_capture_controller->GetClientCount() == 0) {
    DVLOG(1) << "VideoCaptureManager starting device (type = "
             << entry->stream_type << ", id = " << entry->id << ")";

    device_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &VideoCaptureManager::DoStartDeviceOnDeviceThread,
            this,
            session_id,
            entry,
            params,
            base::Passed(entry->video_capture_controller->NewDeviceClient())));
  }
  // Run the callback first, as AddClient() may trigger OnFrameInfo().
  done_cb.Run(entry->video_capture_controller->GetWeakPtr());
  entry->video_capture_controller->AddClient(
      client_id, client_handler, client_render_process, session_id, params);
}

void VideoCaptureManager::StopCaptureForClient(
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler,
    bool aborted_due_to_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(controller);
  DCHECK(client_handler);

  DeviceEntry* entry = GetDeviceEntryForController(controller);
  if (!entry) {
    NOTREACHED();
    return;
  }
  if (aborted_due_to_error) {
    SessionMap::iterator it;
    for (it = sessions_.begin(); it != sessions_.end(); ++it) {
      if (it->second.type == entry->stream_type &&
          it->second.id == entry->id) {
        listener_->Aborted(it->second.type, it->first);
        break;
      }
    }
  }

  // Detach client from controller.
  media::VideoCaptureSessionId session_id =
      controller->RemoveClient(client_id, client_handler);
  DVLOG(1) << "VideoCaptureManager::StopCaptureForClient, session_id = "
           << session_id;

  // If controller has no more clients, delete controller and device.
  DestroyDeviceEntryIfNoClients(entry);
}

bool VideoCaptureManager::GetDeviceSupportedFormats(
    media::VideoCaptureSessionId capture_session_id,
    media::VideoCaptureFormats* supported_formats) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(supported_formats->empty());

  SessionMap::iterator it = sessions_.find(capture_session_id);
  if (it == sessions_.end())
    return false;
  DVLOG(1) << "GetDeviceSupportedFormats for device: " << it->second.name;

  // Return all available formats of the device, regardless its started state.
  DeviceInfo* existing_device =
      FindDeviceInfoById(it->second.id, devices_info_cache_);
  if (existing_device)
    *supported_formats = existing_device->supported_formats;
  return true;
}

bool VideoCaptureManager::GetDeviceFormatsInUse(
    media::VideoCaptureSessionId capture_session_id,
    media::VideoCaptureFormats* formats_in_use) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(formats_in_use->empty());

  SessionMap::iterator it = sessions_.find(capture_session_id);
  if (it == sessions_.end())
    return false;
  DVLOG(1) << "GetDeviceFormatsInUse for device: " << it->second.name;

  // Return the currently in-use format(s) of the device, if it's started.
  DeviceEntry* device_in_use =
      GetDeviceEntryForMediaStreamDevice(it->second);
  if (device_in_use) {
    // Currently only one format-in-use is supported at the VCC level.
    formats_in_use->push_back(
        device_in_use->video_capture_controller->GetVideoCaptureFormat());
  }
  return true;
}

void VideoCaptureManager::SetDesktopCaptureWindowId(
    media::VideoCaptureSessionId session_id,
    gfx::NativeViewId window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SessionMap::iterator session_it = sessions_.find(session_id);
  if (session_it == sessions_.end()) {
    device_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &VideoCaptureManager::SaveDesktopCaptureWindowIdOnDeviceThread,
            this,
            session_id,
            window_id));
    return;
  }

  DeviceEntry* const existing_device =
      GetDeviceEntryForMediaStreamDevice(session_it->second);
  if (!existing_device)
    return;

  DCHECK_EQ(MEDIA_DESKTOP_VIDEO_CAPTURE, existing_device->stream_type);
  DesktopMediaID id = DesktopMediaID::Parse(existing_device->id);
  if (id.type == DesktopMediaID::TYPE_NONE ||
      id.type == DesktopMediaID::TYPE_AURA_WINDOW) {
    return;
  }

  device_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureManager::SetDesktopCaptureWindowIdOnDeviceThread,
                 this,
                 existing_device,
                 window_id));
}

void VideoCaptureManager::DoStopDeviceOnDeviceThread(DeviceEntry* entry) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StopDeviceTime");
  DCHECK(IsOnDeviceThread());
  if (entry->video_capture_device) {
    entry->video_capture_device->StopAndDeAllocate();
  }
  entry->video_capture_device.reset();
}

void VideoCaptureManager::OnOpened(
    MediaStreamType stream_type,
    media::VideoCaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->Opened(stream_type, capture_session_id);
}

void VideoCaptureManager::OnClosed(
    MediaStreamType stream_type,
    media::VideoCaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->Closed(stream_type, capture_session_id);
}

void VideoCaptureManager::OnDevicesInfoEnumerated(
    MediaStreamType stream_type,
    base::ElapsedTimer* timer,
    const DeviceInfos& new_devices_info_cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UMA_HISTOGRAM_TIMES(
      "Media.VideoCaptureManager.GetAvailableDevicesInfoOnDeviceThreadTime",
      timer->Elapsed());
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  devices_info_cache_ = new_devices_info_cache;

  // Walk the |devices_info_cache_| and transform from VCD::Name to
  // StreamDeviceInfo for return purposes.
  StreamDeviceInfoArray devices;
  for (DeviceInfos::const_iterator it = devices_info_cache_.begin();
       it != devices_info_cache_.end(); ++it) {
    devices.push_back(StreamDeviceInfo(
        stream_type, it->name.GetNameAndModel(), it->name.id()));
  }
  listener_->DevicesEnumerated(stream_type, devices);
}

bool VideoCaptureManager::IsOnDeviceThread() const {
  return device_task_runner_->BelongsToCurrentThread();
}

void VideoCaptureManager::ConsolidateDevicesInfoOnDeviceThread(
    base::Callback<void(const DeviceInfos&)> on_devices_enumerated_callback,
    MediaStreamType stream_type,
    const DeviceInfos& old_device_info_cache,
    scoped_ptr<media::VideoCaptureDevice::Names> names_snapshot) {
  DCHECK(IsOnDeviceThread());
  // Construct |new_devices_info_cache| with the cached devices that are still
  // present in the system, and remove their names from |names_snapshot|, so we
  // keep there the truly new devices.
  DeviceInfos new_devices_info_cache;
  for (DeviceInfos::const_iterator it_device_info =
           old_device_info_cache.begin();
       it_device_info != old_device_info_cache.end(); ++it_device_info) {
     for (media::VideoCaptureDevice::Names::iterator it =
         names_snapshot->begin();
          it != names_snapshot->end(); ++it) {
      if (it_device_info->name.id() == it->id()) {
        new_devices_info_cache.push_back(*it_device_info);
        names_snapshot->erase(it);
        break;
      }
    }
  }

  // Get the supported capture formats for the new devices in |names_snapshot|.
  for (media::VideoCaptureDevice::Names::const_iterator it =
      names_snapshot->begin();
       it != names_snapshot->end(); ++it) {
    media::VideoCaptureFormats supported_formats;
    DeviceInfo device_info(*it, media::VideoCaptureFormats());
    video_capture_device_factory_->GetDeviceSupportedFormats(
        *it, &(device_info.supported_formats));
    ConsolidateCaptureFormats(&device_info.supported_formats);
    new_devices_info_cache.push_back(device_info);
  }

  on_devices_enumerated_callback.Run(new_devices_info_cache);
}

VideoCaptureManager::DeviceEntry*
VideoCaptureManager::GetDeviceEntryForMediaStreamDevice(
    const MediaStreamDevice& device_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (DeviceEntries::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    DeviceEntry* device = *it;
    if (device_info.type == device->stream_type &&
        device_info.id == device->id) {
      return device;
    }
  }
  return NULL;
}

VideoCaptureManager::DeviceEntry*
VideoCaptureManager::GetDeviceEntryForController(
    const VideoCaptureController* controller) const {
  // Look up |controller| in |devices_|.
  for (DeviceEntries::const_iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if ((*it)->video_capture_controller.get() == controller) {
      return *it;
    }
  }
  return NULL;
}

void VideoCaptureManager::DestroyDeviceEntryIfNoClients(DeviceEntry* entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Removal of the last client stops the device.
  if (entry->video_capture_controller->GetClientCount() == 0) {
    DVLOG(1) << "VideoCaptureManager stopping device (type = "
             << entry->stream_type << ", id = " << entry->id << ")";

    // The DeviceEntry is removed from |devices_| immediately. The controller is
    // deleted immediately, and the device is freed asynchronously. After this
    // point, subsequent requests to open this same device ID will create a new
    // DeviceEntry, VideoCaptureController, and VideoCaptureDevice.
    devices_.erase(entry);
    entry->video_capture_controller.reset();
    device_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureManager::DoStopDeviceOnDeviceThread, this,
                   base::Owned(entry)));
  }
}

VideoCaptureManager::DeviceEntry* VideoCaptureManager::GetOrCreateDeviceEntry(
    media::VideoCaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  SessionMap::iterator session_it = sessions_.find(capture_session_id);
  if (session_it == sessions_.end()) {
    return NULL;
  }
  const MediaStreamDevice& device_info = session_it->second;

  // Check if another session has already opened this device. If so, just
  // use that opened device.
  DeviceEntry* const existing_device =
      GetDeviceEntryForMediaStreamDevice(device_info);
  if (existing_device) {
    DCHECK_EQ(device_info.type, existing_device->stream_type);
    return existing_device;
  }

  scoped_ptr<VideoCaptureController> video_capture_controller(
      new VideoCaptureController());
  DeviceEntry* new_device = new DeviceEntry(device_info.type,
                                            device_info.id,
                                            video_capture_controller.Pass());
  devices_.insert(new_device);
  return new_device;
}

VideoCaptureManager::DeviceInfo* VideoCaptureManager::FindDeviceInfoById(
    const std::string& id,
    DeviceInfos& device_vector) {
  for (DeviceInfos::iterator it = device_vector.begin();
       it != device_vector.end(); ++it) {
    if (it->name.id() == id)
      return &(*it);
  }
  return NULL;
}

void VideoCaptureManager::SetDesktopCaptureWindowIdOnDeviceThread(
    DeviceEntry* entry,
    gfx::NativeViewId window_id) {
  DCHECK(IsOnDeviceThread());
  DCHECK(entry->stream_type == MEDIA_DESKTOP_VIDEO_CAPTURE);
#if defined(ENABLE_SCREEN_CAPTURE)
  DesktopCaptureDevice* device =
      static_cast<DesktopCaptureDevice*>(entry->video_capture_device.get());
  device->SetNotificationWindowId(window_id);
#endif
}

void VideoCaptureManager::SaveDesktopCaptureWindowIdOnDeviceThread(
    media::VideoCaptureSessionId session_id,
    gfx::NativeViewId window_id) {
  DCHECK(IsOnDeviceThread());
  DCHECK(notification_window_ids_.find(session_id) ==
         notification_window_ids_.end());
  notification_window_ids_[session_id] = window_id;
}

}  // namespace content
