// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_device.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/safe_strerror.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"

#if defined(USE_DRM_ATOMIC)
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"
#endif

namespace ui {

namespace {

typedef base::Callback<void(uint32_t /* frame */,
                            uint32_t /* seconds */,
                            uint32_t /* useconds */,
                            uint64_t /* id */)> DrmEventHandler;

bool DrmCreateDumbBuffer(int fd,
                         const SkImageInfo& info,
                         uint32_t* handle,
                         uint32_t* stride) {
  struct drm_mode_create_dumb request;
  memset(&request, 0, sizeof(request));
  request.width = info.width();
  request.height = info.height();
  request.bpp = info.bytesPerPixel() << 3;
  request.flags = 0;

  if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &request) < 0) {
    VPLOG(2) << "Cannot create dumb buffer";
    return false;
  }

  // The driver may choose to align the last row as well. We don't care about
  // the last alignment bits since they aren't used for display purposes, so
  // just check that the expected size is <= to what the driver allocated.
  DCHECK_LE(info.getSafeSize(request.pitch), request.size);

  *handle = request.handle;
  *stride = request.pitch;
  return true;
}

bool DrmDestroyDumbBuffer(int fd, uint32_t handle) {
  struct drm_mode_destroy_dumb destroy_request;
  memset(&destroy_request, 0, sizeof(destroy_request));
  destroy_request.handle = handle;
  return !drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_request);
}

bool ProcessDrmEvent(int fd, const DrmEventHandler& callback) {
  char buffer[1024];
  int len = read(fd, buffer, sizeof(buffer));
  if (len == 0)
    return false;

  if (len < static_cast<int>(sizeof(drm_event))) {
    PLOG(ERROR) << "Failed to read DRM event";
    return false;
  }

  int idx = 0;
  while (idx < len) {
    DCHECK_LE(static_cast<int>(sizeof(drm_event)), len - idx);
    drm_event event;
    memcpy(&event, &buffer[idx], sizeof(event));
    switch (event.type) {
      case DRM_EVENT_FLIP_COMPLETE: {
        DCHECK_LE(static_cast<int>(sizeof(drm_event_vblank)), len - idx);
        drm_event_vblank vblank;
        memcpy(&vblank, &buffer[idx], sizeof(vblank));
        callback.Run(vblank.sequence, vblank.tv_sec, vblank.tv_usec,
                     vblank.user_data);
      } break;
      case DRM_EVENT_VBLANK:
        break;
      default:
        NOTREACHED();
        break;
    }

    idx += event.length;
  }

  return true;
}

bool CanQueryForResources(int fd) {
  drm_mode_card_res resources;
  memset(&resources, 0, sizeof(resources));
  // If there is no error getting DRM resources then assume this is a
  // modesetting device.
  return !drmIoctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &resources);
}

// TODO(robert.bradford): Replace with libdrm structures after libdrm roll.
// https://crbug.com/586475
struct DrmColorLut {
  uint16_t red;
  uint16_t green;
  uint16_t blue;
  uint16_t reserved;
};

struct DrmColorCtm {
  int64_t ctm_coeff[9];
};

struct DrmModeCreateBlob {
  uint64_t data;
  uint32_t length;
  uint32_t blob_id;
};

struct DrmModeDestroyBlob {
  uint32_t blob_id;
};

#ifndef DRM_IOCTL_MODE_CREATEPROPBLOB
#define DRM_IOCTL_MODE_CREATEPROPBLOB DRM_IOWR(0xBD, struct DrmModeCreateBlob)
#endif

#ifndef DRM_IOCTL_MODE_DESTROYPROPBLOB
#define DRM_IOCTL_MODE_DESTROYPROPBLOB DRM_IOWR(0xBE, struct DrmModeDestroyBlob)
#endif

int CreatePropertyBlob(int fd, const void* data, size_t length, uint32_t* id) {
  DrmModeCreateBlob create;
  int ret;

  if (length >= 0xffffffff)
    return -ERANGE;

  memset(&create, 0, sizeof(create));

  create.length = length;
  create.data = (uintptr_t)data;
  create.blob_id = 0;
  *id = 0;

  ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATEPROPBLOB, &create);
  ret = ret < 0 ? -errno : ret;
  if (ret != 0)
    return ret;

  *id = create.blob_id;
  return 0;
}

int DestroyPropertyBlob(int fd, uint32_t id) {
  DrmModeDestroyBlob destroy;
  int ret;

  memset(&destroy, 0, sizeof(destroy));
  destroy.blob_id = id;
  ret = drmIoctl(fd, DRM_IOCTL_MODE_DESTROYPROPBLOB, &destroy);
  return ret < 0 ? -errno : ret;
}

using ScopedDrmColorLutPtr = std::unique_ptr<DrmColorLut, base::FreeDeleter>;
using ScopedDrmColorCtmPtr = std::unique_ptr<DrmColorCtm, base::FreeDeleter>;

ScopedDrmColorLutPtr CreateLutBlob(
    const std::vector<GammaRampRGBEntry>& source) {
  TRACE_EVENT0("drm", "CreateLutBlob");
  if (source.empty())
    return nullptr;

  ScopedDrmColorLutPtr lut(
      static_cast<DrmColorLut*>(malloc(sizeof(DrmColorLut) * source.size())));
  DrmColorLut* p = lut.get();
  for (size_t i = 0; i < source.size(); ++i) {
    p[i].red = source[i].r;
    p[i].green = source[i].g;
    p[i].blue = source[i].b;
  }
  return lut;
}

ScopedDrmColorCtmPtr CreateCTMBlob(
    const std::vector<float>& correction_matrix) {
  if (correction_matrix.empty())
    return nullptr;

  ScopedDrmColorCtmPtr ctm(
      static_cast<DrmColorCtm*>(malloc(sizeof(DrmColorCtm))));
  for (size_t i = 0; i < arraysize(ctm->ctm_coeff); ++i) {
    if (correction_matrix[i] < 0) {
      ctm->ctm_coeff[i] = static_cast<uint64_t>(
          -correction_matrix[i] * (static_cast<uint64_t>(1) << 32));
      ctm->ctm_coeff[i] |= static_cast<uint64_t>(1) << 63;
    } else {
      ctm->ctm_coeff[i] = static_cast<uint64_t>(
          correction_matrix[i] * (static_cast<uint64_t>(1) << 32));
    }
  }
  return ctm;
}

bool SetBlobProperty(int fd,
                     uint32_t object_id,
                     uint32_t object_type,
                     uint32_t prop_id,
                     const char* property_name,
                     unsigned char* data,
                     size_t length) {
  uint32_t blob_id = 0;
  int res;

  if (data) {
    res = CreatePropertyBlob(fd, data, length, &blob_id);
    if (res != 0) {
      LOG(ERROR) << "Error creating property blob: " << base::safe_strerror(res)
                 << " for property " << property_name;
      return false;
    }
  }

  bool success = false;
  res = drmModeObjectSetProperty(fd, object_id, object_type, prop_id, blob_id);
  if (res != 0) {
    LOG(ERROR) << "Error updating property: " << base::safe_strerror(res)
               << " for property " << property_name;
  } else {
    success = true;
  }
  if (blob_id != 0)
    DestroyPropertyBlob(fd, blob_id);
  return success;
}

std::vector<GammaRampRGBEntry> ResampleLut(
    const std::vector<GammaRampRGBEntry>& lut_in,
    size_t desired_size) {
  TRACE_EVENT1("drm", "ResampleLut", "desired_size", desired_size);
  if (lut_in.empty())
    return std::vector<GammaRampRGBEntry>();

  if (lut_in.size() == desired_size)
    return lut_in;

  std::vector<GammaRampRGBEntry> result;
  result.resize(desired_size);

  for (size_t i = 0; i < desired_size; ++i) {
    size_t base_index = lut_in.size() * i / desired_size;
    size_t remaining = lut_in.size() * i % desired_size;
    if (base_index < lut_in.size() - 1) {
      result[i].r = lut_in[base_index].r +
                    (lut_in[base_index + 1].r - lut_in[base_index].r) *
                        remaining / desired_size;
      result[i].g = lut_in[base_index].g +
                    (lut_in[base_index + 1].g - lut_in[base_index].g) *
                        remaining / desired_size;
      result[i].b = lut_in[base_index].b +
                    (lut_in[base_index + 1].b - lut_in[base_index].b) *
                        remaining / desired_size;
    } else {
      result[i] = lut_in[lut_in.size() - 1];
    }
  }

  return result;
}

}  // namespace

class DrmDevice::PageFlipManager {
 public:
  PageFlipManager() : next_id_(0) {}
  ~PageFlipManager() {}

  void OnPageFlip(uint32_t frame,
                  uint32_t seconds,
                  uint32_t useconds,
                  uint64_t id) {
    auto it =
        std::find_if(callbacks_.begin(), callbacks_.end(), FindCallback(id));
    if (it == callbacks_.end()) {
      LOG(WARNING) << "Could not find callback for page flip id=" << id;
      return;
    }

    DrmDevice::PageFlipCallback callback = it->callback;
    it->pending_calls -= 1;

    if (it->pending_calls)
      return;

    callbacks_.erase(it);
    callback.Run(frame, seconds, useconds);
  }

  uint64_t GetNextId() { return next_id_++; }

  void RegisterCallback(uint64_t id,
                        uint64_t pending_calls,
                        const DrmDevice::PageFlipCallback& callback) {
    callbacks_.push_back({id, pending_calls, callback});
  }

 private:
  struct PageFlip {
    uint64_t id;
    uint32_t pending_calls;
    DrmDevice::PageFlipCallback callback;
  };

  struct FindCallback {
    FindCallback(uint64_t id) : id(id) {}

    bool operator()(const PageFlip& flip) const { return flip.id == id; }

    uint64_t id;
  };

  uint64_t next_id_;

  std::vector<PageFlip> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PageFlipManager);
};

class DrmDevice::IOWatcher : public base::MessagePumpLibevent::Watcher {
 public:
  IOWatcher(int fd, DrmDevice::PageFlipManager* page_flip_manager)
      : page_flip_manager_(page_flip_manager), fd_(fd) {
    Register();
  }

  ~IOWatcher() override { Unregister(); }

 private:
  void Register() {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    base::MessageLoopForIO::current()->WatchFileDescriptor(
        fd_, true, base::MessageLoopForIO::WATCH_READ, &controller_, this);
  }

  void Unregister() {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    controller_.StopWatchingFileDescriptor();
  }

  // base::MessagePumpLibevent::Watcher overrides:
  void OnFileCanReadWithoutBlocking(int fd) override {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    TRACE_EVENT1("drm", "OnDrmEvent", "socket", fd);

    if (!ProcessDrmEvent(fd, base::Bind(&DrmDevice::PageFlipManager::OnPageFlip,
                                        base::Unretained(page_flip_manager_))))
      Unregister();
  }

  void OnFileCanWriteWithoutBlocking(int fd) override { NOTREACHED(); }

  DrmDevice::PageFlipManager* page_flip_manager_;

  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  int fd_;

  DISALLOW_COPY_AND_ASSIGN(IOWatcher);
};

DrmDevice::DrmDevice(const base::FilePath& device_path,
                     base::File file,
                     bool is_primary_device)
    : device_path_(device_path),
      file_(std::move(file)),
      page_flip_manager_(new PageFlipManager()),
      is_primary_device_(is_primary_device) {}

DrmDevice::~DrmDevice() {}

bool DrmDevice::Initialize(bool use_atomic) {
  // Ignore devices that cannot perform modesetting.
  if (!CanQueryForResources(file_.GetPlatformFile())) {
    VLOG(2) << "Cannot query for resources for '" << device_path_.value()
            << "'";
    return false;
  }

#if defined(USE_DRM_ATOMIC)
  // Use atomic only if the build, kernel & flags all allow it.
  if (use_atomic && SetCapability(DRM_CLIENT_CAP_ATOMIC, 1))
    plane_manager_.reset(new HardwareDisplayPlaneManagerAtomic());
#endif  // defined(USE_DRM_ATOMIC)

  if (!plane_manager_)
    plane_manager_.reset(new HardwareDisplayPlaneManagerLegacy());
  if (!plane_manager_->Initialize(this)) {
    LOG(ERROR) << "Failed to initialize the plane manager for "
               << device_path_.value();
    plane_manager_.reset();
    return false;
  }

  watcher_.reset(
      new IOWatcher(file_.GetPlatformFile(), page_flip_manager_.get()));

  return true;
}

ScopedDrmCrtcPtr DrmDevice::GetCrtc(uint32_t crtc_id) {
  DCHECK(file_.IsValid());
  return ScopedDrmCrtcPtr(drmModeGetCrtc(file_.GetPlatformFile(), crtc_id));
}

bool DrmDevice::SetCrtc(uint32_t crtc_id,
                        uint32_t framebuffer,
                        std::vector<uint32_t> connectors,
                        drmModeModeInfo* mode) {
  DCHECK(file_.IsValid());
  DCHECK(!connectors.empty());
  DCHECK(mode);

  TRACE_EVENT2("drm", "DrmDevice::SetCrtc", "crtc", crtc_id, "size",
               gfx::Size(mode->hdisplay, mode->vdisplay).ToString());
  return !drmModeSetCrtc(file_.GetPlatformFile(), crtc_id, framebuffer, 0, 0,
                         connectors.data(), connectors.size(), mode);
}

bool DrmDevice::SetCrtc(drmModeCrtc* crtc, std::vector<uint32_t> connectors) {
  DCHECK(file_.IsValid());
  // If there's no buffer then the CRTC was disabled.
  if (!crtc->buffer_id)
    return DisableCrtc(crtc->crtc_id);

  DCHECK(!connectors.empty());

  TRACE_EVENT1("drm", "DrmDevice::RestoreCrtc", "crtc", crtc->crtc_id);
  return !drmModeSetCrtc(file_.GetPlatformFile(), crtc->crtc_id,
                         crtc->buffer_id, crtc->x, crtc->y, connectors.data(),
                         connectors.size(), &crtc->mode);
}

bool DrmDevice::DisableCrtc(uint32_t crtc_id) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::DisableCrtc", "crtc", crtc_id);
  return !drmModeSetCrtc(file_.GetPlatformFile(), crtc_id, 0, 0, 0, NULL, 0,
                         NULL);
}

ScopedDrmConnectorPtr DrmDevice::GetConnector(uint32_t connector_id) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::GetConnector", "connector", connector_id);
  return ScopedDrmConnectorPtr(
      drmModeGetConnector(file_.GetPlatformFile(), connector_id));
}

bool DrmDevice::AddFramebuffer2(uint32_t width,
                                uint32_t height,
                                uint32_t format,
                                uint32_t handles[4],
                                uint32_t strides[4],
                                uint32_t offsets[4],
                                uint32_t* framebuffer,
                                uint32_t flags) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::AddFramebuffer", "handle", handles[0]);
  return !drmModeAddFB2(file_.GetPlatformFile(), width, height, format, handles,
                        strides, offsets, framebuffer, flags);
}

bool DrmDevice::RemoveFramebuffer(uint32_t framebuffer) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::RemoveFramebuffer", "framebuffer",
               framebuffer);
  return !drmModeRmFB(file_.GetPlatformFile(), framebuffer);
}

bool DrmDevice::PageFlip(uint32_t crtc_id,
                         uint32_t framebuffer,
                         const PageFlipCallback& callback) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmDevice::PageFlip", "crtc", crtc_id, "framebuffer",
               framebuffer);

  // NOTE: Calling drmModeSetCrtc will immediately update the state, though
  // callbacks to already scheduled page flips will be honored by the kernel.
  uint64_t id = page_flip_manager_->GetNextId();
  if (!drmModePageFlip(file_.GetPlatformFile(), crtc_id, framebuffer,
                       DRM_MODE_PAGE_FLIP_EVENT, reinterpret_cast<void*>(id))) {
    // If successful the payload will be removed by a PageFlip event.
    page_flip_manager_->RegisterCallback(id, 1, callback);
    return true;
  }

  return false;
}

bool DrmDevice::PageFlipOverlay(uint32_t crtc_id,
                                uint32_t framebuffer,
                                const gfx::Rect& location,
                                const gfx::Rect& source,
                                int overlay_plane) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmDevice::PageFlipOverlay", "crtc", crtc_id,
               "framebuffer", framebuffer);
  return !drmModeSetPlane(file_.GetPlatformFile(), overlay_plane, crtc_id,
                          framebuffer, 0, location.x(), location.y(),
                          location.width(), location.height(), source.x(),
                          source.y(), source.width(), source.height());
}

ScopedDrmFramebufferPtr DrmDevice::GetFramebuffer(uint32_t framebuffer) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::GetFramebuffer", "framebuffer", framebuffer);
  return ScopedDrmFramebufferPtr(
      drmModeGetFB(file_.GetPlatformFile(), framebuffer));
}

ScopedDrmPropertyPtr DrmDevice::GetProperty(drmModeConnector* connector,
                                            const char* name) {
  TRACE_EVENT2("drm", "DrmDevice::GetProperty", "connector",
               connector->connector_id, "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(file_.GetPlatformFile(), connector->props[i]));
    if (!property)
      continue;

    if (strcmp(property->name, name) == 0)
      return property;
  }

  return ScopedDrmPropertyPtr();
}

bool DrmDevice::SetProperty(uint32_t connector_id,
                            uint32_t property_id,
                            uint64_t value) {
  DCHECK(file_.IsValid());
  return !drmModeConnectorSetProperty(file_.GetPlatformFile(), connector_id,
                                      property_id, value);
}

bool DrmDevice::GetCapability(uint64_t capability, uint64_t* value) {
  DCHECK(file_.IsValid());
  return !drmGetCap(file_.GetPlatformFile(), capability, value);
}

ScopedDrmPropertyBlobPtr DrmDevice::GetPropertyBlob(drmModeConnector* connector,
                                                    const char* name) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmDevice::GetPropertyBlob", "connector",
               connector->connector_id, "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(file_.GetPlatformFile(), connector->props[i]));
    if (!property)
      continue;

    if (strcmp(property->name, name) == 0 &&
        (property->flags & DRM_MODE_PROP_BLOB))
      return ScopedDrmPropertyBlobPtr(drmModeGetPropertyBlob(
          file_.GetPlatformFile(), connector->prop_values[i]));
  }

  return ScopedDrmPropertyBlobPtr();
}

bool DrmDevice::SetCursor(uint32_t crtc_id,
                          uint32_t handle,
                          const gfx::Size& size) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmDevice::SetCursor", "crtc_id", crtc_id, "handle",
               handle);
  return !drmModeSetCursor(file_.GetPlatformFile(), crtc_id, handle,
                           size.width(), size.height());
}

bool DrmDevice::MoveCursor(uint32_t crtc_id, const gfx::Point& point) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::MoveCursor", "crtc_id", crtc_id);
  return !drmModeMoveCursor(file_.GetPlatformFile(), crtc_id, point.x(),
                            point.y());
}

bool DrmDevice::CreateDumbBuffer(const SkImageInfo& info,
                                 uint32_t* handle,
                                 uint32_t* stride) {
  DCHECK(file_.IsValid());

  TRACE_EVENT0("drm", "DrmDevice::CreateDumbBuffer");
  return DrmCreateDumbBuffer(file_.GetPlatformFile(), info, handle, stride);
}

bool DrmDevice::DestroyDumbBuffer(uint32_t handle) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::DestroyDumbBuffer", "handle", handle);
  return DrmDestroyDumbBuffer(file_.GetPlatformFile(), handle);
}

bool DrmDevice::MapDumbBuffer(uint32_t handle, size_t size, void** pixels) {
  struct drm_mode_map_dumb map_request;
  memset(&map_request, 0, sizeof(map_request));
  map_request.handle = handle;
  if (drmIoctl(file_.GetPlatformFile(), DRM_IOCTL_MODE_MAP_DUMB,
               &map_request)) {
    PLOG(ERROR) << "Cannot prepare dumb buffer for mapping";
    return false;
  }

  *pixels = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                 file_.GetPlatformFile(), map_request.offset);
  if (*pixels == MAP_FAILED) {
    PLOG(ERROR) << "Cannot mmap dumb buffer";
    return false;
  }

  return true;
}

bool DrmDevice::UnmapDumbBuffer(void* pixels, size_t size) {
  return !munmap(pixels, size);
}

bool DrmDevice::CloseBufferHandle(uint32_t handle) {
  struct drm_gem_close close_request;
  memset(&close_request, 0, sizeof(close_request));
  close_request.handle = handle;
  return !drmIoctl(file_.GetPlatformFile(), DRM_IOCTL_GEM_CLOSE,
                   &close_request);
}

bool DrmDevice::CommitProperties(drmModeAtomicReq* properties,
                                 uint32_t flags,
                                 uint32_t crtc_count,
                                 const PageFlipCallback& callback) {
#if defined(USE_DRM_ATOMIC)
  uint64_t id = 0;
  bool page_flip_event_requested = flags & DRM_MODE_PAGE_FLIP_EVENT;

  if (page_flip_event_requested)
    id = page_flip_manager_->GetNextId();

  if (!drmModeAtomicCommit(file_.GetPlatformFile(), properties, flags,
                           reinterpret_cast<void*>(id))) {
    if (page_flip_event_requested)
      page_flip_manager_->RegisterCallback(id, crtc_count, callback);

    return true;
  }
#endif  // defined(USE_DRM_ATOMIC)
  return false;
}

bool DrmDevice::SetCapability(uint64_t capability, uint64_t value) {
  DCHECK(file_.IsValid());
  return !drmSetClientCap(file_.GetPlatformFile(), capability, value);
}

bool DrmDevice::SetMaster() {
  TRACE_EVENT1("drm", "DrmDevice::SetMaster", "path", device_path_.value());
  DCHECK(file_.IsValid());
  return (drmSetMaster(file_.GetPlatformFile()) == 0);
}

bool DrmDevice::DropMaster() {
  TRACE_EVENT1("drm", "DrmDevice::DropMaster", "path", device_path_.value());
  DCHECK(file_.IsValid());
  return (drmDropMaster(file_.GetPlatformFile()) == 0);
}

bool DrmDevice::SetGammaRamp(uint32_t crtc_id,
                             const std::vector<GammaRampRGBEntry>& lut) {
  ScopedDrmCrtcPtr crtc = GetCrtc(crtc_id);
  size_t gamma_size = static_cast<size_t>(crtc->gamma_size);

  if (gamma_size == 0 && lut.empty())
    return true;

  if (gamma_size == 0) {
    LOG(ERROR) << "Gamma table not supported";
    return false;
  }

  // TODO(robert.bradford) resample the incoming ramp to match what the kernel
  // expects.
  if (!lut.empty() && gamma_size != lut.size()) {
    LOG(ERROR) << "Gamma table size mismatch: supplied " << lut.size()
               << " expected " << gamma_size;
    return false;
  }

  std::vector<uint16_t> r, g, b;
  r.reserve(gamma_size);
  g.reserve(gamma_size);
  b.reserve(gamma_size);

  if (lut.empty()) {
    // Create a linear gamma ramp table to deactivate the feature.
    for (size_t i = 0; i < gamma_size; ++i) {
      uint16_t value = (i * ((1 << 16) - 1)) / (gamma_size - 1);
      r.push_back(value);
      g.push_back(value);
      b.push_back(value);
    }
  } else {
    for (size_t i = 0; i < gamma_size; ++i) {
      r.push_back(lut[i].r);
      g.push_back(lut[i].g);
      b.push_back(lut[i].b);
    }
  }

  DCHECK(file_.IsValid());
  TRACE_EVENT0("drm", "DrmDevice::SetGamma");
  return (drmModeCrtcSetGamma(file_.GetPlatformFile(), crtc_id, r.size(), &r[0],
                              &g[0], &b[0]) == 0);
}

bool DrmDevice::SetColorCorrection(
    uint32_t crtc_id,
    const std::vector<GammaRampRGBEntry>& degamma_lut,
    const std::vector<GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  ScopedDrmObjectPropertyPtr crtc_props(drmModeObjectGetProperties(
      file_.GetPlatformFile(), crtc_id, DRM_MODE_OBJECT_CRTC));
  uint64_t degamma_lut_size = 0;
  uint64_t gamma_lut_size = 0;

  for (uint32_t i = 0; i < crtc_props->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(file_.GetPlatformFile(), crtc_props->props[i]));
    if (property && !strcmp(property->name, "DEGAMMA_LUT_SIZE")) {
      degamma_lut_size = crtc_props->prop_values[i];
    }
    if (property && !strcmp(property->name, "GAMMA_LUT_SIZE")) {
      gamma_lut_size = crtc_props->prop_values[i];
    }

    if (degamma_lut_size && gamma_lut_size)
      break;
  }

  // If we can't find the degamma & gamma lut size, it means the properties
  // aren't available. We should then use the legacy gamma ramp ioctl.
  if (degamma_lut_size == 0 || gamma_lut_size == 0) {
    return SetGammaRamp(crtc_id, gamma_lut);
  }

  ScopedDrmColorLutPtr degamma_blob_data =
      CreateLutBlob(ResampleLut(degamma_lut, degamma_lut_size));
  ScopedDrmColorLutPtr gamma_blob_data =
      CreateLutBlob(ResampleLut(gamma_lut, gamma_lut_size));
  ScopedDrmColorCtmPtr ctm_blob_data = CreateCTMBlob(correction_matrix);

  for (uint32_t i = 0; i < crtc_props->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(file_.GetPlatformFile(), crtc_props->props[i]));
    if (!property)
      continue;

    if (!strcmp(property->name, "DEGAMMA_LUT")) {
      if (!SetBlobProperty(
              file_.GetPlatformFile(), crtc_id, DRM_MODE_OBJECT_CRTC,
              crtc_props->props[i], property->name,
              reinterpret_cast<unsigned char*>(degamma_blob_data.get()),
              sizeof(DrmColorLut) * degamma_lut_size))
        return false;
    }
    if (!strcmp(property->name, "GAMMA_LUT")) {
      if (!SetBlobProperty(
              file_.GetPlatformFile(), crtc_id, DRM_MODE_OBJECT_CRTC,
              crtc_props->props[i], property->name,
              reinterpret_cast<unsigned char*>(gamma_blob_data.get()),
              sizeof(DrmColorLut) * gamma_lut_size))
        return false;
    }
    if (!strcmp(property->name, "CTM")) {
      if (!SetBlobProperty(
              file_.GetPlatformFile(), crtc_id, DRM_MODE_OBJECT_CRTC,
              crtc_props->props[i], property->name,
              reinterpret_cast<unsigned char*>(ctm_blob_data.get()),
              sizeof(DrmColorCtm)))
        return false;
    }
  }

  return true;
}

}  // namespace ui
